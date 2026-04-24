cbuffer TransformCB : register(b0)
{
    row_major matrix Model;
    row_major matrix View;
    row_major matrix Projection;
};

cbuffer LightCB : register(b1)
{
    float3 LightDir;     // world-space, pointing TOWARD the light source
    float  Shininess;
    float3 LightColor;
    float  _pad0;
    float3 AmbientColor;
    float  _pad1;
    float3 CameraPos;    // world-space eye position
    float  _pad2;
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
    float3 WorldPos : TEXCOORD1;
    float3 Normal   : NORMAL;
    float2 TexCoord : TEXCOORD0;
};

PSIn VS_Main(VSIn input)
{
    PSIn output;
    float4 world     = mul(float4(input.Position, 1.0f), Model);
    output.WorldPos  = world.xyz;
    output.Position  = mul(mul(world, View), Projection);
    // Upper-left 3x3 of Model is correct for normal transform under uniform scale.
    output.Normal    = mul(input.Normal, (float3x3)Model);
    output.TexCoord  = input.TexCoord;
    return output;
}

float4 PS_Main(PSIn input) : SV_TARGET
{
    float3 N = normalize(input.Normal);
    float3 L = normalize(LightDir);
    float3 V = normalize(CameraPos - input.WorldPos);
    float3 H = normalize(L + V);

    float  diff = max(dot(N, L), 0.0f);
    float  spec = (diff > 0.0f) ? pow(max(dot(N, H), 0.0f), Shininess) : 0.0f;

    float4 albedo = g_texture.Sample(g_sampler, input.TexCoord);

    float3 color = AmbientColor  * albedo.rgb
                 + LightColor    * diff * albedo.rgb
                 + LightColor    * spec * 0.4f;

    return float4(color, albedo.a);
}
