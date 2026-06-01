// SSAO.hlsl — Screen-Space Ambient Occlusion (hemisphere sampling).
// Inputs: t0=depth (R32), t1=normalRoughness (viewN.xyz*0.5+0.5, roughness), t2=noise (4x4 random)
// Output: single-channel occlusion (0 = fully occluded, 1 = no occlusion)

Texture2D    g_depth   : register(t0);
Texture2D    g_normal  : register(t1);
Texture2D    g_noise   : register(t2);
SamplerState g_clamp   : register(s0);
SamplerState g_wrap    : register(s1);

#define KERNEL_SIZE 64

cbuffer SSAOCB : register(b0)
{
    row_major matrix InvProj;
    row_major matrix Proj;
    float4 Samples[KERNEL_SIZE];
    float2 ScreenSize;
    float2 NoiseScale;
    float  Radius;
    float  Bias;
    float  Power;
    float  _pad;
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

float3 ViewPosFromDepth(float2 uv, float depth)
{
    float4 ndc = float4(uv * 2.0f - 1.0f, depth, 1.0f);
    ndc.y = -ndc.y;
    float4 viewPos = mul(ndc, InvProj);
    return viewPos.xyz / viewPos.w;
}

float4 PS_Main(VSOutput input) : SV_TARGET
{
    float2 uv = input.uv;

    float rawDepth = g_depth.SampleLevel(g_clamp, uv, 0).r;
    if (rawDepth >= 1.0f) return float4(1.0f, 1.0f, 1.0f, 1.0f);

    float3 viewPos = ViewPosFromDepth(uv, rawDepth);

    // Decode view-space normal
    float4 normData = g_normal.SampleLevel(g_clamp, uv, 0);
    float3 viewN = normalize(normData.xyz * 2.0f - 1.0f);

    // Random rotation vector from noise texture (tiled across screen)
    float3 randomVec = g_noise.SampleLevel(g_wrap, uv * NoiseScale, 0).xyz * 2.0f - 1.0f;

    // Construct TBN to orient hemisphere along the surface normal
    float3 tangent  = normalize(randomVec - viewN * dot(randomVec, viewN));
    float3 binormal = cross(viewN, tangent);
    float3x3 TBN   = float3x3(tangent, binormal, viewN);

    float occlusion = 0.0f;

    [unroll]
    for (int i = 0; i < KERNEL_SIZE; ++i)
    {
        // Transform sample from tangent space to view space
        float3 sampleDir = mul(Samples[i].xyz, TBN);
        float3 samplePos = viewPos + sampleDir * Radius;

        // Project sample to screen UV
        float4 offset = mul(float4(samplePos, 1.0f), Proj);
        offset.xy /= offset.w;
        float2 sampleUV = float2(offset.x * 0.5f + 0.5f, -offset.y * 0.5f + 0.5f);

        // Skip samples outside screen
        if (sampleUV.x < 0.0f || sampleUV.x > 1.0f ||
            sampleUV.y < 0.0f || sampleUV.y > 1.0f)
            continue;

        // Get depth at sample position
        float sampleDepth = g_depth.SampleLevel(g_clamp, sampleUV, 0).r;
        if (sampleDepth >= 1.0f) continue;

        float3 sampleViewPos = ViewPosFromDepth(sampleUV, sampleDepth);
        float sampleZ = sampleViewPos.z;

        // Range check: only occlude if sample is within radius
        float rangeCheck = smoothstep(0.0f, 1.0f, Radius / abs(viewPos.z - sampleZ));

        // Occluded if the actual surface is closer to camera than our sample point
        occlusion += (sampleZ <= samplePos.z - Bias ? 1.0f : 0.0f) * rangeCheck;
    }

    occlusion = 1.0f - (occlusion / (float)KERNEL_SIZE);
    occlusion = pow(saturate(occlusion), Power);

    return float4(occlusion, occlusion, occlusion, 1.0f);
}
