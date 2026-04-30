// Bloom shader — brightness extract, dual Kawase downsample, upsample, and composite.
// Each pass compiled via preprocessor define: PASS_EXTRACT, PASS_DOWN, PASS_UP, PASS_COMPOSITE.

Texture2D    g_source  : register(t0);
SamplerState g_sampler : register(s0);

#ifdef PASS_COMPOSITE
Texture2D    g_bloom   : register(t1);
#endif

cbuffer BloomCB : register(b0)
{
    float2 TexelSize;  // 1.0 / target resolution
    float  Threshold;
    float  Intensity;
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
#ifdef PASS_EXTRACT
    // Brightness extract — soft-knee luminance threshold
    float3 c = g_source.Sample(g_sampler, input.uv).rgb;
    float  lum = dot(c, float3(0.2126f, 0.7152f, 0.0722f));
    float  contrib = max(lum - Threshold, 0.0f) / max(lum, 0.0001f);
    return float4(c * contrib, 1.0f);

#elif defined(PASS_DOWN)
    // Dual Kawase downsample — 5 bilinear taps at half-pixel offsets
    float2 uv = input.uv;
    float2 ht = TexelSize * 0.5f;
    float3 sum = g_source.Sample(g_sampler, uv).rgb * 4.0f;
    sum += g_source.Sample(g_sampler, uv + float2(-ht.x, -ht.y)).rgb;
    sum += g_source.Sample(g_sampler, uv + float2( ht.x, -ht.y)).rgb;
    sum += g_source.Sample(g_sampler, uv + float2(-ht.x,  ht.y)).rgb;
    sum += g_source.Sample(g_sampler, uv + float2( ht.x,  ht.y)).rgb;
    return float4(sum / 8.0f, 1.0f);

#elif defined(PASS_UP)
    // Dual Kawase upsample — 8-tap tent filter
    float2 uv = input.uv;
    float2 hs = TexelSize * 0.5f;
    float2 ts = TexelSize;
    float3 sum  = g_source.Sample(g_sampler, uv + float2(-ts.x,  0.0f)).rgb;
    sum        += g_source.Sample(g_sampler, uv + float2( ts.x,  0.0f)).rgb;
    sum        += g_source.Sample(g_sampler, uv + float2( 0.0f, -ts.y)).rgb;
    sum        += g_source.Sample(g_sampler, uv + float2( 0.0f,  ts.y)).rgb;
    sum        += g_source.Sample(g_sampler, uv + float2(-hs.x, -hs.y)).rgb * 2.0f;
    sum        += g_source.Sample(g_sampler, uv + float2( hs.x, -hs.y)).rgb * 2.0f;
    sum        += g_source.Sample(g_sampler, uv + float2(-hs.x,  hs.y)).rgb * 2.0f;
    sum        += g_source.Sample(g_sampler, uv + float2( hs.x,  hs.y)).rgb * 2.0f;
    return float4(sum / 12.0f, 1.0f);

#elif defined(PASS_COMPOSITE)
    // Additive composite: scene + bloom * intensity
    float3 scene = g_source.Sample(g_sampler, input.uv).rgb;
    float3 bloom = g_bloom.Sample(g_sampler, input.uv).rgb;
    return float4(scene + bloom * Intensity, 1.0f);

#else
    return float4(1, 0, 1, 1); // magenta = missing pass define
#endif
}
