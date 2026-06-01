// SSAOApply.hlsl — Outputs the AO value for multiplicative blending.
// Used with a multiply blend state (DestColor * SrcColor).
// Inputs: t0=blurred AO
// Output: AO value in RGB (for multiply blend with scene)

Texture2D    g_ao    : register(t0);
SamplerState g_clamp : register(s0);

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
    float ao = g_ao.SampleLevel(g_clamp, input.uv, 0).r;
    return float4(ao, ao, ao, 1.0f);
}
