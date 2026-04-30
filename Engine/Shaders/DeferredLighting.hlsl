// Deferred lighting pass — fullscreen quad that reads the G-buffer and computes
// Cook-Torrance PBR lighting with directional shadows and point lights.

// G-buffer inputs
Texture2D    g_albedo   : register(t0);
Texture2D    g_normal   : register(t1);
Texture2D    g_material : register(t2);
Texture2D    g_depth    : register(t3);
Texture2D    g_shadow   : register(t4);
Texture2D    g_ao       : register(t5);
// Point shadow cube maps (up to 2 shadow-casting lights at indices 0..1)
TextureCube<float> g_pointShadow0 : register(t6);
TextureCube<float> g_pointShadow1 : register(t7);
SamplerState               g_sampler       : register(s0);
SamplerComparisonState     g_shadowSampler : register(s1);
SamplerState               g_cubeSampler   : register(s2);

cbuffer LightCB : register(b1)
{
    float3 LightDir;      float _shininess; // legacy slot, unused
    float3 LightColor;    float _pad0;
    float3 AmbientColor;  float _pad1;
    float3 CameraPos;     float DebugLightMode;
    row_major matrix LightViewProj;
};

struct PointLight
{
    float3 Position;
    float  Radius;
    float3 Color;
    float  _pad;
};

cbuffer PointLightCB : register(b2)
{
    PointLight PointLights[8];
    int        NumPointLights;
    float3     _plpad;
};

cbuffer DeferredCB : register(b0)
{
    row_major matrix InvViewProj;
    float2           ScreenSize;
    float            DebugMode;
    float            EnableSSAO;
    int              NumPointShadowCasters;
    float            PointShadowBias;
    float2           _dcpad;
};

// ---- PBR helpers (Cook-Torrance) ----
static const float PI = 3.14159265358979f;

// GGX / Trowbridge-Reitz normal distribution
float D_GGX(float NdotH, float roughness)
{
    float a  = roughness * roughness;
    float a2 = a * a;
    float d  = NdotH * NdotH * (a2 - 1.0f) + 1.0f;
    return a2 / max(PI * d * d, 0.0001f);
}

// Smith-Schlick-GGX geometry sub-term (separate for V and L)
float G_SchlickGGX(float NdotX, float roughness)
{
    float r = roughness + 1.0f;
    float k = (r * r) / 8.0f;
    return NdotX / max(NdotX * (1.0f - k) + k, 0.0001f);
}

float G_Smith(float NdotV, float NdotL, float roughness)
{
    return G_SchlickGGX(NdotV, roughness) * G_SchlickGGX(NdotL, roughness);
}

// Fresnel-Schlick
float3 F_Schlick(float cosTheta, float3 F0)
{
    return F0 + (1.0f - F0) * pow(saturate(1.0f - cosTheta), 5.0f);
}

// Full Cook-Torrance specular BRDF + Lambertian diffuse, returns Lo contribution.
// radiance: light colour × attenuation (pre-multiplied by caller)
float3 PBR_DirectLight(float3 N, float3 V, float3 L,
                       float3 albedo, float roughness, float metallic,
                       float3 F0,    float3 radiance)
{
    float rough = max(roughness, 0.04f); // clamp to avoid NaN at 0
    float3 H    = normalize(V + L);

    float NdotV = max(dot(N, V), 0.0001f);
    float NdotL = max(dot(N, L), 0.0001f);
    float NdotH = saturate(dot(N, H));
    float HdotV = saturate(dot(H, V));

    float  D  = D_GGX(NdotH, rough);
    float  G  = G_Smith(NdotV, NdotL, rough);
    float3 F  = F_Schlick(HdotV, F0);

    float3 kS = F;
    float3 kD = (1.0f - kS) * (1.0f - metallic); // metals have no diffuse

    float3 specular = D * G * F / max(4.0f * NdotV * NdotL, 0.001f);

    return (kD * albedo / PI + specular) * radiance * NdotL;
}

// ---- Vertex / pixel shaders ----
struct VSOutput
{
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};

VSOutput VS_Main(float2 pos : POSITION, float2 uv : TEXCOORD0)
{
    VSOutput o;
    o.pos = float4(pos, 0.0f, 1.0f);
    o.uv  = uv;
    return o;
}

float3 ReconstructWorldPos(float2 uv, float hwDepth)
{
    float2 ndc;
    ndc.x =  uv.x * 2.0f - 1.0f;
    ndc.y = 1.0f - uv.y * 2.0f;
    float4 clip  = float4(ndc, hwDepth, 1.0f);
    float4 world = mul(clip, InvViewProj);
    return world.xyz / world.w;
}

float4 PS_Main(VSOutput input) : SV_TARGET
{
    float depth = g_depth.Sample(g_sampler, input.uv).r;
    if (depth >= 1.0f) discard; // sky pixels

    float3 albedo    = g_albedo.Sample(g_sampler, input.uv).rgb;
    float3 N         = normalize(g_normal.Sample(g_sampler, input.uv).rgb);
    float2 matSample = g_material.Sample(g_sampler, input.uv).rg;
    float  roughness = matSample.r;
    float  metallic  = matSample.g;

    float3 worldPos = ReconstructWorldPos(input.uv, depth);
    float3 V        = normalize(CameraPos - worldPos);
    float3 F0       = lerp(float3(0.04f, 0.04f, 0.04f), albedo, metallic);

    // --- Directional shadow PCF 3x3 ---
    float shadow = 1.0f;
    {
        float4 shadowClip = mul(float4(worldPos, 1.0f), LightViewProj);
        float3 shadowNDC  = shadowClip.xyz / shadowClip.w;
        float2 shadowUV   = float2(shadowNDC.x * 0.5f + 0.5f, -shadowNDC.y * 0.5f + 0.5f);
        float  depthRef   = shadowNDC.z;

        if (saturate(shadowUV.x) == shadowUV.x &&
            saturate(shadowUV.y) == shadowUV.y &&
            depthRef < 1.0f)
        {
            float w, h;
            g_shadow.GetDimensions(w, h);
            float texelSize = 1.0f / w;

            shadow = 0.0f;
            [unroll] for (int dx = -1; dx <= 1; ++dx)
            [unroll] for (int dy = -1; dy <= 1; ++dy)
                shadow += g_shadow.SampleCmpLevelZero(g_shadowSampler,
                    shadowUV + float2(dx, dy) * texelSize, depthRef);
            shadow /= 9.0f;
        }
    }
    if (DebugLightMode > 0.5f) shadow = 1.0f;

    // --- SSAO ---
    float ao = 1.0f;
    if (EnableSSAO > 0.5f)
        ao = g_ao.Sample(g_sampler, input.uv).r;

    // --- Directional light (PBR) ---
    float3 L      = normalize(LightDir);
    float3 color  = ao * AmbientColor * albedo * (1.0f - metallic * 0.9f); // simple ambient
    float  NdotL  = max(dot(N, L), 0.0f);
    color += shadow * PBR_DirectLight(N, V, L, albedo, roughness, metallic, F0, LightColor);

    // --- Point lights (PBR) ---
    for (int i = 0; i < NumPointLights; ++i)
    {
        float3 toLight = PointLights[i].Position - worldPos;
        float  dist    = length(toLight);
        if (dist >= PointLights[i].Radius) continue;

        float  atten = 1.0f - (dist / PointLights[i].Radius);
        atten        = atten * atten;

        float3 PL = toLight / dist;

        float pointShadow = 1.0f;
        if (i < NumPointShadowCasters)
        {
            float3 L2F        = worldPos - PointLights[i].Position;
            float  shadowDist = length(L2F) / PointLights[i].Radius;
            float  closest    = (i == 0) ? g_pointShadow0.Sample(g_cubeSampler, L2F).r
                                         : g_pointShadow1.Sample(g_cubeSampler, L2F).r;
            pointShadow = (shadowDist > closest + PointShadowBias) ? 0.0f : 1.0f;
        }

        color += pointShadow * PBR_DirectLight(N, V, PL, albedo, roughness, metallic, F0,
                                               PointLights[i].Color * atten);
    }

    // Debug: NdotL greyscale
    if (DebugLightMode > 1.5f)
        return float4(NdotL, NdotL, NdotL, 1.0f);

    return float4(color, 1.0f);
}

