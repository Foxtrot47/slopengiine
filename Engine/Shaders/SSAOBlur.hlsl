// Bilateral blur for SSAO — depth + normal edge-preserving, separable (H then V).

Texture2D    g_ao     : register(t0);
Texture2D    g_depth  : register(t1);
Texture2D    g_normal : register(t2);
SamplerState g_point  : register(s0);

cbuffer BlurParams : register(b0)
{
    float2 BlurDirection;   // (1/w, 0) for horizontal; (0, 1/h) for vertical
    float2 _pad;
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

float4 PS_Main(VSOutput input) : SV_TARGET
{
    float  centerAO     = g_ao.Sample(g_point, input.uv).r;
    float  centerDepth  = g_depth.Sample(g_point, input.uv).r;
    float3 centerNormal = g_normal.Sample(g_point, input.uv).xyz;

    float result      = centerAO;
    float totalWeight = 1.0f;

    [unroll]
    for (int i = -2; i <= 2; ++i)
    {
        if (i == 0) continue;

        float2 sampleUV     = input.uv + BlurDirection * float(i);
        float  sampleAO     = g_ao.Sample(g_point, sampleUV).r;
        float  sampleDepth  = g_depth.Sample(g_point, sampleUV).r;
        float3 sampleNormal = g_normal.Sample(g_point, sampleUV).xyz;

        // Bilateral weights: reject across depth/normal discontinuities
        float depthWeight  = exp(-abs(centerDepth - sampleDepth) * 500.0f);
        float normalWeight = max(0.0f, dot(centerNormal, sampleNormal));
        float w = depthWeight * normalWeight;

        result      += sampleAO * w;
        totalWeight += w;
    }

    return float4(result / totalWeight, 0.0f, 0.0f, 1.0f);
}
