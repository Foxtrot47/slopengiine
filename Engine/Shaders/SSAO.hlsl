// Screen-Space Ambient Occlusion — hemisphere sampling with depth comparison.
// Reads G-buffer depth + normals, outputs single-channel AO to R8_UNORM target.

Texture2D    g_depth  : register(t0);
Texture2D    g_normal : register(t1);
Texture2D    g_noise  : register(t2);
SamplerState g_pointClamp : register(s0);
SamplerState g_pointWrap  : register(s1);

cbuffer SSAOParams : register(b0)
{
    row_major matrix Projection;
    row_major matrix InvProjection;
    row_major matrix View;
    float4 Samples[64];
    float2 ScreenSize;
    float2 NoiseScale;
    float  Radius;
    float  Bias;
    float  Intensity;
    int    KernelSize;
};

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

float3 ReconstructViewPos(float2 uv, float hwDepth)
{
    float2 ndc = float2(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f);
    float4 clip = float4(ndc, hwDepth, 1.0f);
    float4 viewH = mul(clip, InvProjection);
    return viewH.xyz / viewH.w;
}

float LinearizeDepth(float hwDepth)
{
    return Projection._43 / (hwDepth - Projection._33);
}

float4 PS_Main(VSOutput input) : SV_TARGET
{
    float hwDepth = g_depth.Sample(g_pointClamp, input.uv).r;
    if (hwDepth >= 1.0f)
        return float4(1.0f, 1.0f, 1.0f, 1.0f);

    // View-space position of this pixel
    float3 viewPos = ReconstructViewPos(input.uv, hwDepth);

    // World-space normal → view-space
    float3 normalWS = g_normal.Sample(g_pointClamp, input.uv).xyz;
    float3 normalVS = normalize(mul(normalWS, (float3x3)View));

    // Random rotation vector from tiled noise
    float3 randomVec = g_noise.Sample(g_pointWrap, input.uv * NoiseScale).xyz;

    // Gram-Schmidt: build TBN with normal as Z axis
    float3 tangent   = normalize(randomVec - normalVS * dot(randomVec, normalVS));
    float3 bitangent = cross(normalVS, tangent);
    float3x3 TBN     = float3x3(tangent, bitangent, normalVS);

    float occlusion = 0.0f;
    for (int i = 0; i < KernelSize; ++i)
    {
        // Orient kernel sample to hemisphere around normal
        float3 sampleVS = viewPos + mul(Samples[i].xyz, TBN) * Radius;

        // Project sample to screen
        float4 sampleClip = mul(float4(sampleVS, 1.0f), Projection);
        float2 sampleUV = float2( sampleClip.x / sampleClip.w * 0.5f + 0.5f,
                                  -sampleClip.y / sampleClip.w * 0.5f + 0.5f);

        // Linearize depth at the projected sample position
        float sampleHW = g_depth.Sample(g_pointClamp, sampleUV).r;
        float sceneZ   = LinearizeDepth(sampleHW);

        // Range-check: ignore samples too far from the pixel
        float rangeCheck = smoothstep(0.0f, 1.0f, Radius / abs(viewPos.z - sceneZ));

        // If scene geometry at sample UV is closer than sample → occluded
        occlusion += (sceneZ >= sampleVS.z + Bias ? 0.0f : 1.0f) * rangeCheck;
    }

    float ao = 1.0f - (occlusion / float(KernelSize));
    ao = pow(saturate(ao), Intensity);
    return float4(ao, ao, ao, 1.0f);
}
