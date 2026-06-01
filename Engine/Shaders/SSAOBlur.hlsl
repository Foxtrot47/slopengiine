// SSAOBlur.hlsl — Bilateral blur for SSAO (edge-preserving).
// Inputs: t0=SSAO result, t1=depth
// Output: blurred AO value

Texture2D    g_ao    : register(t0);
Texture2D    g_depth : register(t1);
SamplerState g_clamp : register(s0);

cbuffer BlurCB : register(b0)
{
    float2 TexelSize;
    float2 Direction; // (1,0) for horizontal, (0,1) for vertical
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
    float2 uv = input.uv;

    float centerDepth = g_depth.SampleLevel(g_clamp, uv, 0).r;
    float centerAO    = g_ao.SampleLevel(g_clamp, uv, 0).r;

    // Skip sky
    if (centerDepth >= 1.0f) return float4(1.0f, 1.0f, 1.0f, 1.0f);

    // 4-tap bilateral blur in given direction
    static const float weights[4] = { 0.324f, 0.232f, 0.0855f, 0.0205f };
    static const float offsets[4] = { 0.0f, 1.0f, 2.0f, 3.0f };

    float totalAO     = centerAO * weights[0];
    float totalWeight = weights[0];

    float depthThreshold = 0.001f;

    [unroll]
    for (int i = 1; i < 4; ++i)
    {
        float2 offset = Direction * TexelSize * offsets[i];

        // Positive direction
        float2 uvP = uv + offset;
        float dP = g_depth.SampleLevel(g_clamp, uvP, 0).r;
        float wP = weights[i] * (abs(dP - centerDepth) < depthThreshold ? 1.0f : 0.0f);
        totalAO += g_ao.SampleLevel(g_clamp, uvP, 0).r * wP;
        totalWeight += wP;

        // Negative direction
        float2 uvN = uv - offset;
        float dN = g_depth.SampleLevel(g_clamp, uvN, 0).r;
        float wN = weights[i] * (abs(dN - centerDepth) < depthThreshold ? 1.0f : 0.0f);
        totalAO += g_ao.SampleLevel(g_clamp, uvN, 0).r * wN;
        totalWeight += wN;
    }

    float ao = totalAO / max(totalWeight, 0.001f);
    return float4(ao, ao, ao, 1.0f);
}
