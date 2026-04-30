cbuffer TransformCB : register(b0)
{
    row_major matrix Model;
    row_major matrix View;
    row_major matrix Projection;
};

cbuffer LightCB : register(b1)
{
    float3 LightDir;      // world-space, toward light
    float  Shininess;
    float3 LightColor;
    float  _pad0;
    float3 AmbientColor;
    float  _pad1;
    float3 CameraPos;
    float  DebugLightMode; // 0=normal, 1=force lit, 2=show NdotL
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

cbuffer MaterialCB : register(b3)
{
    float3 AlbedoTint;     float  RoughnessScale;
    float  Metallic;       float  Unlit;  float  DebugShadow;  float _mat_pad2;
};

Texture2D    g_albedo    : register(t0);
Texture2D    g_roughness : register(t1);
Texture2D    g_normal    : register(t2);
Texture2D    g_shadowMap : register(t3);
SamplerState g_sampler   : register(s0);
SamplerComparisonState g_shadowSampler : register(s1);

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
    float4 Position  : SV_POSITION;
    float3 WorldPos  : TEXCOORD1;
    float2 TexCoord  : TEXCOORD0;
    float3 T         : TEXCOORD2;
    float3 B         : TEXCOORD3;
    float3 N         : TEXCOORD4;
};

PSIn VS_Main(VSIn input)
{
    PSIn output;
    float4 world    = mul(float4(input.Position, 1.0f), Model);
    output.WorldPos = world.xyz;
    output.Position = mul(mul(world, View), Projection);
    output.TexCoord = input.TexCoord;

    float3x3 m3 = (float3x3)Model;
    output.T = normalize(mul(input.Tangent,   m3));
    output.B = normalize(mul(input.Bitangent, m3));
    output.N = normalize(mul(input.Normal,    m3));
    return output;
}

float4 PS_Main(PSIn input) : SV_TARGET
{
    if (Unlit > 0.5f)
        return float4(AlbedoTint, 1.0f);

    // Reconstruct TBN and transform normal map sample to world space.
    // RG channels hold the tangent-space XY; Z is reconstructed so this
    // works for both ATI2/BC5 (no stored Z) and BC3 (Z stored but redundant).
    float2 tbn_rg = g_normal.Sample(g_sampler, input.TexCoord).rg * 2.0f - 1.0f;
    float3 tbn_n  = float3(tbn_rg, sqrt(saturate(1.0f - tbn_rg.x*tbn_rg.x - tbn_rg.y*tbn_rg.y)));
    float3x3 TBN  = float3x3(normalize(input.T), normalize(input.B), normalize(input.N));
    float3 N      = normalize(mul(tbn_n, TBN));

    float3 V         = normalize(CameraPos - input.WorldPos);
    float4 albedo    = g_albedo.Sample(g_sampler, input.TexCoord);
    albedo.rgb      *= AlbedoTint;
    float  roughness = g_roughness.Sample(g_sampler, input.TexCoord).g * RoughnessScale;
    float  specMask  = 1.0f - saturate(roughness);
    float  pixShine  = max(1.0f, Shininess * specMask);

    // Metallic workflow: specular colour blends toward albedo; diffuse killed for metals
    float3 F0    = lerp(float3(0.04f, 0.04f, 0.04f), albedo.rgb, Metallic);
    float  kDiff = 1.0f - Metallic;

    // Shadow mapping with PCF 3x3
    float shadow = 1.0f;
    {
        float4 shadowClip = mul(float4(input.WorldPos, 1.0f), LightViewProj);
        float3 shadowNDC  = shadowClip.xyz / shadowClip.w;
        float2 shadowUV   = float2(shadowNDC.x * 0.5f + 0.5f, -shadowNDC.y * 0.5f + 0.5f);
        float  depthRef   = shadowNDC.z;

        if (saturate(shadowUV.x) == shadowUV.x && saturate(shadowUV.y) == shadowUV.y && depthRef < 1.0f)
        {
            float w, h;
            g_shadowMap.GetDimensions(w, h);
            float texelSize = 1.0f / w;

            shadow = 0.0f;
            [unroll] for (int dx = -1; dx <= 1; ++dx)
            {
                [unroll] for (int dy = -1; dy <= 1; ++dy)
                {
                    shadow += g_shadowMap.SampleCmpLevelZero(g_shadowSampler,
                        shadowUV + float2(dx, dy) * texelSize, depthRef);
                }
            }
            shadow /= 9.0f;
        }
    }
    if (DebugLightMode > 0.5f) shadow = 1.0f;
    float3 L    = normalize(LightDir);
    float3 H    = normalize(L + V);
    float  diff = max(dot(N, L), 0.0f);
    float  spec = (diff > 0.0f) ? pow(max(dot(N, H), 0.0f), pixShine) * specMask : 0.0f;

    float3 color = AmbientColor * albedo.rgb
                 + shadow * LightColor * diff * kDiff * albedo.rgb
                 + shadow * LightColor * spec * F0;

    // Point lights
    for (int i = 0; i < NumPointLights; ++i)
    {
        float3 toLight = PointLights[i].Position - input.WorldPos;
        float  dist    = length(toLight);
        if (dist >= PointLights[i].Radius) continue;

        float  atten = 1.0f - (dist / PointLights[i].Radius);
        atten        = atten * atten;

        float3 PL = toLight / dist;
        float3 PH = normalize(PL + V);
        float  pd = max(dot(N, PL), 0.0f);
        float  ps = (pd > 0.0f) ? pow(max(dot(N, PH), 0.0f), pixShine) * specMask : 0.0f;

        color += PointLights[i].Color * atten * (pd * kDiff * albedo.rgb + ps * F0);
    }

    // Debug: visualise shadow factor as greyscale when DebugShadow > 0.5
    if (DebugShadow > 0.5f)
        return float4(shadow, shadow, shadow, 1.0f);

    // Debug: show NdotL as greyscale when DebugLightMode > 1.5
    if (DebugLightMode > 1.5f)
        return float4(diff, diff, diff, 1.0f);

    return float4(color, albedo.a);
}
