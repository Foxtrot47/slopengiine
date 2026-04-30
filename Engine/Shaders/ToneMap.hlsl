// Tone mapping pass — reads HDR linear scene, outputs LDR to back buffer.
// Operator 0 = Reinhard, 1 = ACES (Narkowicz 2015 approximation).
// GammaCorrect = 1: applies sRGB encode (pow 1/2.2 approx); keep off until
// albedo textures are properly linearised at load time (M52).

Texture2D    g_scene   : register(t0);
SamplerState g_sampler : register(s0);

cbuffer ToneMapCB : register(b0)
{
    float Exposure;
    int   Operator;   // 0 = Reinhard, 1 = ACES
    int   GammaCorrect;
    float _pad;
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

float3 TonemapReinhard(float3 x)
{
    return x / (1.0f + x);
}

float3 TonemapACES(float3 x)
{
    // Narkowicz 2015 — cheap analytic approximation of the ACES filmic curve.
    const float a = 2.51f, b = 0.03f, c = 2.43f, d = 0.59f, e = 0.14f;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

float3 LinearToSRGB(float3 x)
{
    // Piecewise sRGB encode (IEC 61966-2-1).
    float3 lo = x * 12.92f;
    float3 hi = 1.055f * pow(max(x, 0.0001f), 1.0f / 2.4f) - 0.055f;
    return x < 0.0031308f ? lo : hi;
}

float4 PS_Main(VSOutput input) : SV_TARGET
{
    float3 hdr = max(g_scene.Sample(g_sampler, input.uv).rgb, 0.0f);
    hdr *= max(Exposure, 0.0001f);

    float3 ldr;
    if (Operator == 0)
        ldr = TonemapReinhard(hdr);
    else
        ldr = TonemapACES(hdr);

    if (GammaCorrect)
        ldr = LinearToSRGB(ldr);

    return float4(ldr, 1.0f);
}
