cbuffer TransformCB : register(b0)
{
    row_major matrix Model;
    row_major matrix View;
    row_major matrix Projection;
};

cbuffer LightCB : register(b1)
{
    float3 LightDir;      // world-space, toward light
    float  Shininess;
    float3 LightColor;
    float  _pad0;
    float3 AmbientColor;
    float  _pad1;
    float3 CameraPos;
    float  _pad2;
};

struct PointLight
{
    float3 Position;
    float  Radius;
    float3 Color;
    float  _pad;
};

cbuffer PointLightCB : register(b2)
{
    PointLight PointLights[8];
    int        NumPointLights;
    float3     _plpad;
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

float4 PS_Main(PSIn input) : SV_TARGET
{
    // Reconstruct TBN and transform normal map sample to world space
    float3 tbn_n  = g_normal.Sample(g_sampler, input.TexCoord).rgb * 2.0f - 1.0f;
    float3x3 TBN  = float3x3(normalize(input.T), normalize(input.B), normalize(input.N));
    float3 N      = normalize(mul(tbn_n, TBN));

    float3 V         = normalize(CameraPos - input.WorldPos);
    float4 albedo    = g_albedo.Sample(g_sampler, input.TexCoord);
    float  roughness = g_roughness.Sample(g_sampler, input.TexCoord).r;
    float  specMask  = 1.0f - roughness;
    float  pixShine  = max(1.0f, Shininess * specMask);

    // Directional light
    float3 L    = normalize(LightDir);
    float3 H    = normalize(L + V);
    float  diff = max(dot(N, L), 0.0f);
    float  spec = (diff > 0.0f) ? pow(max(dot(N, H), 0.0f), pixShine) * specMask : 0.0f;

    float3 color = AmbientColor * albedo.rgb
                 + LightColor   * diff * albedo.rgb
                 + LightColor   * spec * 0.4f;

    // Point lights
    for (int i = 0; i < NumPointLights; ++i)
    {
        float3 toLight = PointLights[i].Position - input.WorldPos;
        float  dist    = length(toLight);
        if (dist >= PointLights[i].Radius) continue;

        float  atten = 1.0f - (dist / PointLights[i].Radius);
        atten        = atten * atten;

        float3 PL = toLight / dist;
        float3 PH = normalize(PL + V);
        float  pd = max(dot(N, PL), 0.0f);
        float  ps = (pd > 0.0f) ? pow(max(dot(N, PH), 0.0f), pixShine) * specMask : 0.0f;

        color += PointLights[i].Color * atten * (pd * albedo.rgb + ps * 0.4f);
    }

    return float4(color, albedo.a);
}
