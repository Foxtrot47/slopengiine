// SSRComposite.hlsl — Blends SSR result into the HDR scene buffer.
// t0 = SSR result (rgb = reflected color, a = confidence)
// Outputs additively to the bound render target using alpha as blend weight.

Texture2D    g_ssr     : register(t0);
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
    float4 ssr = g_ssr.Sample(g_sampler, input.uv);
    // Output color with alpha for alpha blending (SrcAlpha, InvSrcAlpha)
    return float4(ssr.rgb, ssr.a);
}
