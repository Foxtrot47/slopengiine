cbuffer TransformCB : register(b0)
{
    row_major matrix Model;
    row_major matrix View;
    row_major matrix Projection;
};

struct VSIn
{
    float3 Position : POSITION;
    float4 Color    : COLOR;
};

struct PSIn
{
    float4 Position : SV_POSITION;
    float4 Color    : COLOR;
};

PSIn VS_Main(VSIn input)
{
    PSIn output;
    float4 pos  = float4(input.Position, 1.0f);
    pos         = mul(pos, Model);
    pos         = mul(pos, View);
    pos         = mul(pos, Projection);
    output.Position = pos;
    output.Color    = input.Color;
    return output;
}

float4 PS_Main(PSIn input) : SV_TARGET
{
    return input.Color;
}
