// Point light omnidirectional shadow depth pass.
// Writes linear distance / LightFar into an R32_FLOAT cube face (colour target).

cbuffer PointShadowCB : register(b0)
{
    row_major matrix WorldViewProj;
    row_major matrix World;
    float3 LightPos;
    float  LightFar;
};

struct VSIn
{
    float3 Position  : POSITION;
    float3 Normal    : NORMAL;
    float2 TexCoord  : TEXCOORD;
    float3 Tangent   : TANGENT;
    float3 Bitangent : BINORMAL;
};

struct VSOut
{
    float4 pos      : SV_POSITION;
    float3 worldPos : TEXCOORD0;
};

VSOut VS_Main(VSIn input)
{
    VSOut o;
    float4 world = mul(float4(input.Position, 1.0f), World);
    o.worldPos   = world.xyz;
    o.pos        = mul(float4(input.Position, 1.0f), WorldViewProj);
    return o;
}

float PS_Main(VSOut input) : SV_Target0
{
    return length(input.worldPos - LightPos) / LightFar;
}
