// Basic.hlsl — forward PBR + equirectangular IBL + directional/point shadows.
// Registers: b0=TransformCB, b1=LightCB, b2=PointLightCB, b3=MaterialCB, b4=ForwardShadowCB
//            t0=albedo, t1=roughness, t2=normal, t3=shadowMap, t4=sky(IBL), t5-t6=pointShadow, t7=metallic
//            s0=sampler, s1=shadowSampler(cmp), s2=cubeSampler

cbuffer TransformCB : register(b0)
{
    row_major matrix Model;
    row_major matrix View;
    row_major matrix Projection;
};

cbuffer LightCB : register(b1)
{
    float3 LightDir;      float  IBLIntensity;
    float3 LightColor;    float  _pad0;
    float3 CameraPos;     float  DebugLightMode;  // 0=normal, 1=force lit, 2=show NdotL
    row_major matrix LightViewProj;
};

struct PointLight { float3 Position; float Radius; float3 Color; float _pad; };
cbuffer PointLightCB : register(b2)
{
    PointLight PointLights[8];
    int        NumPointLights;
    float3     _plpad;
};

cbuffer MaterialCB : register(b3)
{
    float3 AlbedoTint;  float  RoughnessScale;
    float  Metallic;    float  Unlit;  float  DebugShadow;  float  AlphaCutoff;
};

cbuffer ForwardShadowCB : register(b4)
{
    int   NumPointShadowCasters;
    float PointShadowBias;
    float2 _fscbPad;
};

Texture2D              g_albedo       : register(t0);
Texture2D              g_roughness    : register(t1);
Texture2D              g_normal       : register(t2);
Texture2D              g_shadowMap    : register(t3);
Texture2D              g_sky          : register(t4);  // equirectangular HDR panorama
TextureCube<float>     g_pointShadow0 : register(t5);
TextureCube<float>     g_pointShadow1 : register(t6);
Texture2D              g_metallic     : register(t7);  // metallic map (R channel)
SamplerState           g_sampler      : register(s0);
SamplerComparisonState g_shadowSampler: register(s1);
SamplerState           g_cubeSampler  : register(s2);  // linear-clamp for point shadows

struct VSIn
{
    float3 Position  : POSITION;
    float3 Normal    : NORMAL;
    float2 TexCoord  : TEXCOORD;
    float3 Tangent   : TANGENT;
    float3 Bitangent : BINORMAL;
};

struct PSIn
{
    float4 Position : SV_POSITION;
    float3 WorldPos : TEXCOORD1;
    float2 TexCoord : TEXCOORD0;
    float3 T        : TEXCOORD2;
    float3 B        : TEXCOORD3;
    float3 N        : TEXCOORD4;
};

PSIn VS_Main(VSIn input)
{
    PSIn o;
    float4 world = mul(float4(input.Position, 1.0f), Model);
    o.WorldPos   = world.xyz;
    o.Position   = mul(mul(world, View), Projection);
    o.TexCoord   = input.TexCoord;
    float3x3 m3  = (float3x3)Model;
    o.T = normalize(mul(input.Tangent,   m3));
    o.B = normalize(mul(input.Bitangent, m3));
    o.N = normalize(mul(input.Normal,    m3));
    return o;
}

// ---- PBR helpers ----

static const float PI = 3.14159265359f;

float NDF_GGX(float NdotH, float roughness)
{
    float a  = roughness * roughness;
    float a2 = a * a;
    float d  = NdotH * NdotH * (a2 - 1.0f) + 1.0f;
    return a2 / max(PI * d * d, 1e-5f);
}

float G_SchlickGGX(float NdotX, float roughness)
{
    float r = roughness + 1.0f;
    float k = (r * r) / 8.0f;
    return NdotX / max(NdotX * (1.0f - k) + k, 1e-5f);
}

float3 F_Schlick(float cosTheta, float3 F0)
{
    return F0 + (1.0f - F0) * pow(saturate(1.0f - cosTheta), 5.0f);
}

float3 F_SchlickRoughness(float cosTheta, float3 F0, float roughness)
{
    float3 r = max(float3(1.0f - roughness, 1.0f - roughness, 1.0f - roughness), F0);
    return F0 + (r - F0) * pow(saturate(1.0f - cosTheta), 5.0f);
}

// Cook-Torrance BRDF times NdotL (ready to multiply by lightColor).
float3 CookTorrance(float3 N, float3 V, float3 L,
                    float3 albedo, float roughness, float metallic, float3 F0)
{
    float3 H    = normalize(L + V);
    float NdotV = max(dot(N, V), 1e-4f);
    float NdotL = max(dot(N, L), 0.0f);
    float NdotH = max(dot(N, H), 0.0f);
    float VdotH = max(dot(V, H), 0.0f);

    float  D = NDF_GGX(NdotH, roughness);
    float  G = G_SchlickGGX(NdotV, roughness) * G_SchlickGGX(NdotL, roughness);
    float3 F = F_Schlick(VdotH, F0);

    float3 spec = D * G * F / max(4.0f * NdotV * NdotL, 1e-5f);
    float3 kD   = (1.0f - F) * (1.0f - metallic);
    return (kD * albedo / PI + spec) * NdotL;
}

// ---- IBL helpers ----

float2 DirToEnvUV(float3 dir)
{
    return float2(atan2(dir.z, dir.x) * (0.5f / PI) + 0.5f,
                  0.5f - asin(clamp(dir.y, -1.0f, 1.0f)) / PI);
}

// IBL ambient:
//   Diffuse  — samples the panorama at max mip in the normal direction (approximate irradiance).
//   Specular — split-sum approximation: prefiltered env × BRDF(NdotV, roughness).
//              BRDF term uses Lazarov 2013 analytical fit to avoid a LUT texture.
float3 IBL(float3 N, float3 V, float3 albedo, float roughness, float metallic, float3 F0)
{
    float NdotV = max(dot(N, V), 0.0f);
    float3 F    = F_SchlickRoughness(NdotV, F0, roughness);
    float3 kD   = (1.0f - F) * (1.0f - metallic);

    // Diffuse: sample panorama at its maximum mip level (approximate average irradiance).
    float skyW, skyH, skyMips;
    g_sky.GetDimensions(0, skyW, skyH, skyMips);
    float3 irradiance = g_sky.SampleLevel(g_sampler, DirToEnvUV(N), skyMips - 1.0f).rgb;
    float3 diffuse    = kD * (irradiance / PI) * albedo * IBLIntensity;

    // Specular: prefiltered env at roughness-scaled LOD × split-sum BRDF approximation.
    float3 R        = reflect(-V, N);
    float3 envColor = g_sky.SampleLevel(g_sampler, DirToEnvUV(R), roughness * (skyMips - 1.0f)).rgb;

    // Analytical fit for the BRDF integration map (Lazarov 2013 / UE4 approach).
    float4 c0 = float4(-1.0f, -0.0275f, -0.572f,  0.022f);
    float4 c1 = float4( 1.0f,  0.0425f,  1.04f,  -0.04f);
    float4 r  = roughness * c0 + c1;
    float  a004 = min(r.x * r.x, exp2(-9.28f * NdotV)) * r.x + r.y;
    float2 brdf = float2(-1.04f, 1.04f) * a004 + r.zw;

    float3 specular = envColor * (F0 * brdf.x + brdf.y) * IBLIntensity;

    return diffuse + specular;
}

// ---- PS ----

struct PSOut
{
    float4 color   : SV_TARGET0;
    float4 normal  : SV_TARGET1;  // view-space normal (xyz) + roughness (w)
};

PSOut PS_Main(PSIn input)
{
    PSOut o;
    if (Unlit > 0.5f)
    {
        o.color  = float4(AlbedoTint, 1.0f);
        o.normal = float4(0.0f, 0.0f, 0.0f, 1.0f);
        return o;
    }

    // Normal map → world-space N
    float2 tbn_rg = g_normal.Sample(g_sampler, input.TexCoord).rg * 2.0f - 1.0f;
    float3 tbn_n  = float3(tbn_rg, sqrt(saturate(1.0f - dot(tbn_rg, tbn_rg))));
    float3x3 TBN  = float3x3(normalize(input.T), normalize(input.B), normalize(input.N));
    float3 N      = normalize(mul(tbn_n, TBN));

    float3 V        = normalize(CameraPos - input.WorldPos);
    float4 albedo   = g_albedo.Sample(g_sampler, input.TexCoord);
    albedo.rgb     *= AlbedoTint;

    // Alpha test (cutout): discard fragments below threshold
    if (AlphaCutoff > 0.0f)
        clip(albedo.a - AlphaCutoff);

    float roughness = max(saturate(g_roughness.Sample(g_sampler, input.TexCoord).r * RoughnessScale), 0.04f);
    float metallic  = saturate(g_metallic.Sample(g_sampler, input.TexCoord).r * Metallic);
    float3 F0       = lerp(float3(0.04f, 0.04f, 0.04f), albedo.rgb, metallic);

    // Directional shadow (PCF 3×3)
    float shadow = 1.0f;
    {
        float4 sc   = mul(float4(input.WorldPos, 1.0f), LightViewProj);
        float3 ndc  = sc.xyz / sc.w;
        float2 uv   = float2(ndc.x * 0.5f + 0.5f, -ndc.y * 0.5f + 0.5f);
        float  dref = ndc.z;
        if (saturate(uv.x) == uv.x && saturate(uv.y) == uv.y && dref < 1.0f)
        {
            float w, h; g_shadowMap.GetDimensions(w, h);
            float ts = 1.0f / w;
            shadow = 0.0f;
            [unroll] for (int dx = -1; dx <= 1; ++dx)
            [unroll] for (int dy = -1; dy <= 1; ++dy)
                shadow += g_shadowMap.SampleCmpLevelZero(g_shadowSampler, uv + float2(dx, dy) * ts, dref);
            shadow /= 9.0f;
        }
    }
    if (DebugLightMode > 0.5f) shadow = 1.0f;

    float3 L    = normalize(LightDir);
    float  NdotL = max(dot(N, L), 0.0f);

    // IBL ambient
    float3 color = IBL(N, V, albedo.rgb, roughness, metallic, F0);

    // Directional light (Cook-Torrance)
    color += shadow * LightColor * CookTorrance(N, V, L, albedo.rgb, roughness, metallic, F0);

    // Point lights
    for (int i = 0; i < NumPointLights; ++i)
    {
        float3 toL = PointLights[i].Position - input.WorldPos;
        float  d   = length(toL);
        if (d >= PointLights[i].Radius) continue;
        float  atten = 1.0f - (d / PointLights[i].Radius);
        atten        = atten * atten;

        float ptShadow = 1.0f;
        if (i < NumPointShadowCasters)
        {
            float3 L2F   = input.WorldPos - PointLights[i].Position;
            float  sd    = length(L2F) / PointLights[i].Radius;
            float  clos  = (i == 0) ? g_pointShadow0.Sample(g_cubeSampler, L2F).r
                                    : g_pointShadow1.Sample(g_cubeSampler, L2F).r;
            ptShadow = (sd > clos + PointShadowBias) ? 0.0f : 1.0f;
        }

        float3 PL = toL / d;
        color += ptShadow * atten * PointLights[i].Color
               * CookTorrance(N, V, PL, albedo.rgb, roughness, metallic, F0);
    }

    if (DebugShadow > 0.5f)
    {
        o.color  = float4(shadow, shadow, shadow, 1.0f);
        o.normal = float4(0.0f, 0.0f, 0.0f, 1.0f);
        return o;
    }
    if (DebugLightMode > 1.5f)
    {
        o.color  = float4(NdotL, NdotL, NdotL, 1.0f);
        o.normal = float4(0.0f, 0.0f, 0.0f, 1.0f);
        return o;
    }

    // Transform world-space normal to view space for SSR
    float3 viewN = normalize(mul(N, (float3x3)View));

    o.color  = float4(color, albedo.a);
    o.normal = float4(viewN * 0.5f + 0.5f, roughness);
    return o;
}
