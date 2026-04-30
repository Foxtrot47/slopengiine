// G-Buffer geometry pass — writes albedo, normal, material to MRT.
// Vertex shader is identical to Basic.hlsl VS; pixel shader outputs to 3 targets.

cbuffer TransformCB : register(b0)
{
    row_major matrix Model;
    row_major matrix View;
    row_major matrix Projection;
};

cbuffer MaterialCB : register(b3)
{
    float3 AlbedoTint;     float RoughnessScale;
    float  Metallic;       float Unlit;  float DebugShadow;  float _mat_pad2;
};

Texture2D    g_albedo    : register(t0);
Texture2D    g_roughness : register(t1);
Texture2D    g_normal    : register(t2);
SamplerState g_sampler   : register(s0);

struct VSIn
{
    float3 Position  : POSITION;
    float3 Normal    : NORMAL;
    float2 TexCoord  : TEXCOORD;
    float3 Tangent   : TANGENT;
    float3 Bitangent : BINORMAL;
};

struct PSIn
{
    float4 Position  : SV_POSITION;
    float3 WorldPos  : TEXCOORD1;
    float2 TexCoord  : TEXCOORD0;
    float3 T         : TEXCOORD2;
    float3 B         : TEXCOORD3;
    float3 N         : TEXCOORD4;
};

PSIn VS_Main(VSIn input)
{
    PSIn output;
    float4 world    = mul(float4(input.Position, 1.0f), Model);
    output.WorldPos = world.xyz;
    output.Position = mul(mul(world, View), Projection);
    output.TexCoord = input.TexCoord;

    float3x3 m3 = (float3x3)Model;
    output.T = normalize(mul(input.Tangent,   m3));
    output.B = normalize(mul(input.Bitangent, m3));
    output.N = normalize(mul(input.Normal,    m3));
    return output;
}

struct PSOut
{
    float4 Albedo   : SV_TARGET0;   // RGB = albedo * tint, A = 1
    float4 Normal   : SV_TARGET1;   // XYZ = world normal, A = 0
    float4 Material : SV_TARGET2;   // R = roughness, G = metallic, BA = 0
};

PSOut PS_Main(PSIn input)
{
    PSOut o;

    // Albedo
    float4 albedo = g_albedo.Sample(g_sampler, input.TexCoord);
    o.Albedo = float4(albedo.rgb * AlbedoTint, 1.0f);

    // World-space normal from normal map (RT1 is FLOAT, store raw [-1,1]).
    // RG channels hold tangent-space XY; Z reconstructed — handles ATI2/BC5 and BC3.
    float2 tbn_rg = g_normal.Sample(g_sampler, input.TexCoord).rg * 2.0f - 1.0f;
    float3 tbn_n  = float3(tbn_rg, sqrt(saturate(1.0f - tbn_rg.x*tbn_rg.x - tbn_rg.y*tbn_rg.y)));
    float3x3 TBN  = float3x3(normalize(input.T), normalize(input.B), normalize(input.N));
    float3 N      = normalize(mul(tbn_n, TBN));
    o.Normal = float4(N, 0.0f);

    // Material
    float roughness = g_roughness.Sample(g_sampler, input.TexCoord).g * RoughnessScale;
    o.Material = float4(roughness, Metallic, 0.0f, 0.0f);

    return o;
}
