// Deferred lighting pass — fullscreen quad that reads the G-buffer and computes
// Blinn-Phong lighting with directional shadows and point lights.

// G-buffer inputs
Texture2D    g_albedo   : register(t0);
Texture2D    g_normal   : register(t1);
Texture2D    g_material : register(t2);
Texture2D    g_depth    : register(t3);
Texture2D    g_shadow   : register(t4);
Texture2D    g_ao       : register(t5);
SamplerState g_sampler  : register(s0);
SamplerComparisonState g_shadowSampler : register(s1);

cbuffer LightCB : register(b1)
{
    float3 LightDir;      float Shininess;
    float3 LightColor;    float _pad0;
    float3 AmbientColor;  float _pad1;
    float3 CameraPos;     float _pad2;
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
};

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
    float4 clip = float4(ndc, hwDepth, 1.0f);
    float4 world = mul(clip, InvViewProj);
    return world.xyz / world.w;
}

float4 PS_Main(VSOutput input) : SV_TARGET
{
    float depth = g_depth.Sample(g_sampler, input.uv).r;
    // Sky pixels have depth == 1.0 (cleared value). Skip lighting.
    if (depth >= 1.0f)
        discard;

    float3 albedo    = g_albedo.Sample(g_sampler, input.uv).rgb;
    float3 N         = normalize(g_normal.Sample(g_sampler, input.uv).rgb);
    float2 matSample = g_material.Sample(g_sampler, input.uv).rg;
    float  roughness = matSample.r;
    float  metallic  = matSample.g;

    float3 worldPos  = ReconstructWorldPos(input.uv, depth);
    float3 V         = normalize(CameraPos - worldPos);

    float  specMask  = 1.0f - saturate(roughness);
    float  pixShine  = max(1.0f, Shininess * specMask);
    float3 F0        = lerp(float3(0.04f, 0.04f, 0.04f), albedo, metallic);
    float  kDiff     = 1.0f - metallic;

    // Shadow mapping with PCF 3x3
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
            {
                [unroll] for (int dy = -1; dy <= 1; ++dy)
                {
                    shadow += g_shadow.SampleCmpLevelZero(g_shadowSampler,
                        shadowUV + float2(dx, dy) * texelSize, depthRef);
                }
            }
            shadow /= 9.0f;
        }
    }

    // SSAO — sample AO factor
    float ao = 1.0f;
    if (EnableSSAO > 0.5f)
        ao = g_ao.Sample(g_sampler, input.uv).r;

    // Directional light
    float3 L    = normalize(LightDir);
    float3 H    = normalize(L + V);
    float  diff = max(dot(N, L), 0.0f);
    float  spec = (diff > 0.0f) ? pow(max(dot(N, H), 0.0f), pixShine) * specMask : 0.0f;

    float3 color = ao * AmbientColor * albedo
                 + shadow * LightColor * diff * kDiff * albedo
                 + shadow * LightColor * spec * F0;

    // Point lights
    for (int i = 0; i < NumPointLights; ++i)
    {
        float3 toLight = PointLights[i].Position - worldPos;
        float  dist    = length(toLight);
        if (dist >= PointLights[i].Radius) continue;

        float  atten = 1.0f - (dist / PointLights[i].Radius);
        atten        = atten * atten;

        float3 PL = toLight / dist;
        float3 PH = normalize(PL + V);
        float  pd = max(dot(N, PL), 0.0f);
        float  ps = (pd > 0.0f) ? pow(max(dot(N, PH), 0.0f), pixShine) * specMask : 0.0f;

        color += PointLights[i].Color * atten * (pd * kDiff * albedo + ps * F0);
    }

    return float4(color, 1.0f);
}
