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
    output.Position = float4(input.Position, 1.0f);
    output.Color    = input.Color;
    return output;
}

float4 PS_Main(PSIn input) : SV_TARGET
{
    return input.Color;
}
