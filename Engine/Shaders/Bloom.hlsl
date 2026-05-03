// Bloom.hlsl — dual Kawase bloom: threshold, downsample, upsample, composite.
// Select pass at compile time: BLOOM_THRESHOLD / BLOOM_DOWNSAMPLE / BLOOM_UPSAMPLE / BLOOM_COMPOSITE.
// t0 = source; t1 = same-level downchain addend (upsample pass only).

Texture2D    g_src     : register(t0);
Texture2D    g_addend  : register(t1);
SamplerState g_sampler : register(s0);

cbuffer BloomCB : register(b0)
{
    float2 TexelSize;   // 1 / source dimensions
    float  Threshold;
    float  Intensity;
    float  Scatter;
    float3 _pad;
};

struct VSOut
{
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};

VSOut VS_Main(float2 pos : POSITION, float2 uv : TEXCOORD0)
{
    VSOut o;
    o.pos = float4(pos, 0.0f, 1.0f);
    o.uv  = uv;
    return o;
}

// Soft-knee threshold: smoothly extracts pixels brighter than Threshold.
float3 ApplyThreshold(float3 c)
{
    float lum  = dot(c, float3(0.2126f, 0.7152f, 0.0722f));
    float knee = Threshold * 0.5f;
    float rq   = clamp(lum - Threshold + knee, 0.0f, 2.0f * knee);
    rq = (rq * rq) / max(4.0f * knee, 0.00001f);
    return c * (max(rq, lum - Threshold) / max(lum, 0.00001f));
}

// 5-tap bilinear downsample (dual-filter style: centre×4 + 4 diagonal bilinear taps).
float3 KawaseDown(float2 uv)
{
    float2 ts = TexelSize;
    float3 s  = g_src.Sample(g_sampler, uv).rgb * 4.0f;
    s += g_src.Sample(g_sampler, uv + float2(-ts.x, -ts.y)).rgb;
    s += g_src.Sample(g_sampler, uv + float2( ts.x, -ts.y)).rgb;
    s += g_src.Sample(g_sampler, uv + float2(-ts.x,  ts.y)).rgb;
    s += g_src.Sample(g_sampler, uv + float2( ts.x,  ts.y)).rgb;
    return s / 8.0f;
}

// 9-tap tent upsample filter (samples coarser source; offsets in source-texel space).
float3 KawaseUp(float2 uv)
{
    float2 ts = TexelSize;
    float3 s;
    s  = g_src.Sample(g_sampler, uv + float2(-2.0f * ts.x,         0.0f)).rgb;
    s += g_src.Sample(g_sampler, uv + float2(       -ts.x,         ts.y)).rgb * 2.0f;
    s += g_src.Sample(g_sampler, uv + float2(        0.0f,  2.0f * ts.y)).rgb;
    s += g_src.Sample(g_sampler, uv + float2(        ts.x,         ts.y)).rgb * 2.0f;
    s += g_src.Sample(g_sampler, uv + float2( 2.0f * ts.x,         0.0f)).rgb;
    s += g_src.Sample(g_sampler, uv + float2(        ts.x,        -ts.y)).rgb * 2.0f;
    s += g_src.Sample(g_sampler, uv + float2(        0.0f, -2.0f * ts.y)).rgb;
    s += g_src.Sample(g_sampler, uv + float2(       -ts.x,        -ts.y)).rgb * 2.0f;
    return s / 12.0f;
}

#if defined(BLOOM_THRESHOLD)

float4 PS_Main(VSOut i) : SV_TARGET
{
    float3 c = g_src.Sample(g_sampler, i.uv).rgb;
    return float4(ApplyThreshold(c), 1.0f);
}

#elif defined(BLOOM_DOWNSAMPLE)

float4 PS_Main(VSOut i) : SV_TARGET
{
    return float4(KawaseDown(i.uv), 1.0f);
}

#elif defined(BLOOM_UPSAMPLE)

// Blend upsampled coarser level (g_src) with same-level downchain (g_addend).
// Scatter=1 → pure bloom spread; Scatter=0 → pure local detail.
float4 PS_Main(VSOut i) : SV_TARGET
{
    float3 bloom  = KawaseUp(i.uv);
    float3 addend = g_addend.Sample(g_sampler, i.uv).rgb;
    return float4(lerp(addend, bloom, Scatter), 1.0f);
}

#elif defined(BLOOM_COMPOSITE)

// Tent-upsample bloom from half-res to full-res, scale by Intensity.
// Rendered with additive blend into the HDR RT.
float4 PS_Main(VSOut i) : SV_TARGET
{
    float3 bloom = KawaseUp(i.uv);
    return float4(bloom * Intensity, 1.0f);
}

#else

float4 PS_Main(VSOut i) : SV_TARGET
{
    return g_src.Sample(g_sampler, i.uv);
}

#endif
