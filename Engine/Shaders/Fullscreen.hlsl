// Fullscreen quad passthrough shader.
// Draws a full-screen triangle pair and samples texture t0.
// Used as the basis for all post-process passes (HDR, bloom, SSAO, etc.).

Texture2D    g_source  : register(t0);
SamplerState g_sampler : register(s0);

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
    return g_source.Sample(g_sampler, input.uv);
}
