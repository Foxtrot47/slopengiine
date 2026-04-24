cbuffer TransformCB : register(b0)
{
    row_major matrix Model;
    row_major matrix View;
    row_major matrix Projection;
};

Texture2D    g_texture : register(t0);
SamplerState g_sampler : register(s0);

struct VSIn
{
    float3 Position : POSITION;
    float3 Normal   : NORMAL;
    float2 TexCoord : TEXCOORD;
};

struct PSIn
{
    float4 Position : SV_POSITION;
    float3 Normal   : NORMAL;
    float2 TexCoord : TEXCOORD;
};

PSIn VS_Main(VSIn input)
{
    PSIn output;
    float4 pos      = float4(input.Position, 1.0f);
    pos             = mul(pos, Model);
    pos             = mul(pos, View);
    pos             = mul(pos, Projection);
    output.Position = pos;
    output.Normal   = input.Normal;
    output.TexCoord = input.TexCoord;
    return output;
}

float4 PS_Main(PSIn input) : SV_TARGET
{
    return g_texture.Sample(g_sampler, input.TexCoord);
}
