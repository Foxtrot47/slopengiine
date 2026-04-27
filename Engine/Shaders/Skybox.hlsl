cbuffer SkyboxCB : register(b0)
{
    float4x4 ViewProjNoTrans;
};

Texture2D<float4> g_panorama : register(t0);
SamplerState      g_sampler  : register(s0);

struct VSIn  { float3 pos : POSITION; };
struct VSOut
{
    float4 clipPos : SV_POSITION;
    float3 dir     : TEXCOORD0;
};

static const float PI = 3.14159265359f;

VSOut VS_Main(VSIn input)
{
    VSOut o;
    o.dir = input.pos;
    float4 clip = mul(float4(input.pos, 1.0f), ViewProjNoTrans);
    o.clipPos   = clip.xyww;   // force NDC z = 1 so sky sits at the far plane
    return o;
}

float4 PS_Main(VSOut input) : SV_TARGET
{
    float3 d = normalize(input.dir);
    float  u = atan2(d.z, d.x) * (0.5f / PI) + 0.5f;
    float  v = 0.5f - asin(clamp(d.y, -1.0f, 1.0f)) / PI;
    return g_panorama.Sample(g_sampler, float2(u, v));
}
