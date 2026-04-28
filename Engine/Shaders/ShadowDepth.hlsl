// Shadow depth-only pass — renders to a depth texture from the sun's perspective.
cbuffer ShadowTransformCB : register(b0)
{
    row_major matrix ShadowModel;
    row_major matrix ShadowViewProj;
};

struct VSIn
{
    float3 Position : POSITION;
    float3 Normal   : NORMAL;
    float2 TexCoord : TEXCOORD;
    float3 Tangent  : TANGENT;
    float3 Bitangent: BINORMAL;
};

float4 VS_Main(VSIn input) : SV_POSITION
{
    float4 world = mul(float4(input.Position, 1.0f), ShadowModel);
    return mul(world, ShadowViewProj);
}

// No pixel shader needed — depth-only pass.
// But we declare a minimal one so the ShaderLibrary compile succeeds.
void PS_Main() {}
