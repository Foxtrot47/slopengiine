// SSR.hlsl — Screen-Space Reflections via view-space ray marching.
// Inputs: t0=sceneColor (HDR), t1=depth (R32), t2=normalRoughness (viewN.xyz*0.5+0.5, roughness)
// Output: reflection color with alpha = confidence (0 = no hit, 1 = full hit)

Texture2D    g_scene   : register(t0);
Texture2D    g_depth   : register(t1);
Texture2D    g_normal  : register(t2);
SamplerState g_sampler : register(s0);

cbuffer SSRCB : register(b0)
{
    row_major matrix InvProj;
    row_major matrix Proj;
    float2 ScreenSize;
    float  MaxDistance;
    float  Thickness;
    float  _unused0;
    int    MaxSteps;
    int    BinarySteps;
    float  Intensity;
    int    _unused1;
    float3 _pad;
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

float LinearizeDepth(float d)
{
    float4 ndc = float4(0.0f, 0.0f, d, 1.0f);
    float4 vp  = mul(ndc, InvProj);
    return vp.z / vp.w;
}

float2 ProjectToUV(float3 viewPos)
{
    float4 clip = mul(float4(viewPos, 1.0f), Proj);
    clip.xy /= clip.w;
    return float2(clip.x * 0.5f + 0.5f, -clip.y * 0.5f + 0.5f);
}

float ScreenEdgeFade(float2 uv)
{
    float2 fade = smoothstep(0.0f, 0.05f, uv) * (1.0f - smoothstep(0.95f, 1.0f, uv));
    return fade.x * fade.y;
}

float FresnelSchlick(float cosTheta)
{
    float f0 = 0.04f;
    return f0 + (1.0f - f0) * pow(saturate(1.0f - cosTheta), 5.0f);
}

float4 PS_Main(VSOutput input) : SV_TARGET
{
    float2 uv = input.uv;

    float4 normRough = g_normal.SampleLevel(g_sampler, uv, 0);
    float3 viewN     = normalize(normRough.xyz * 2.0f - 1.0f);
    float  roughness = normRough.w;

    float rawDepth = g_depth.SampleLevel(g_sampler, uv, 0).r;

    // Skip sky and very rough surfaces
    if (rawDepth >= 1.0f) return float4(0.0f, 0.0f, 0.0f, 0.0f);
    if (roughness > 0.8f) return float4(0.0f, 0.0f, 0.0f, 0.0f);

    // Reconstruct view-space position and reflection direction
    float3 viewPos = ViewPosFromDepth(uv, rawDepth);
    float3 viewDir = normalize(viewPos);
    float3 reflDir = normalize(reflect(viewDir, viewN));

    // Fresnel weight
    float NdotV   = saturate(dot(viewN, -viewDir));
    float fresnel = FresnelSchlick(NdotV);

    // View-space ray march
    float stepSize = MaxDistance / (float)MaxSteps;
    float3 rayPos  = viewPos + reflDir * (stepSize * 0.5f);

    bool   hit   = false;
    float  hitT  = 0.0f;
    float2 hitUV = float2(0.0f, 0.0f);

    [loop]
    for (int i = 0; i < MaxSteps; ++i)
    {
        rayPos += reflDir * stepSize;

        float2 sampleUV = ProjectToUV(rayPos);
        if (sampleUV.x < 0.0f || sampleUV.x > 1.0f ||
            sampleUV.y < 0.0f || sampleUV.y > 1.0f)
            break;

        float sampledRawDepth = g_depth.SampleLevel(g_sampler, sampleUV, 0).r;
        if (sampledRawDepth >= 1.0f) continue;

        float sampledLinearZ = LinearizeDepth(sampledRawDepth);
        float depthDiff      = rayPos.z - sampledLinearZ;

        if (depthDiff > 0.0f && depthDiff < Thickness)
        {
            hit   = true;
            hitT  = (float)i / (float)MaxSteps;
            hitUV = sampleUV;
            break;
        }
    }

    if (!hit) return float4(0.0f, 0.0f, 0.0f, 0.0f);

    // Binary search refinement
    float refineDist = stepSize;
    float3 refinePos = rayPos;
    [loop]
    for (int b = 0; b < BinarySteps; ++b)
    {
        refineDist *= 0.5f;
        float2 sUV = ProjectToUV(refinePos);
        float  sd  = g_depth.SampleLevel(g_sampler, sUV, 0).r;
        float  sz  = LinearizeDepth(sd);

        if (refinePos.z - sz > 0.0f)
            refinePos -= reflDir * refineDist;
        else
            refinePos += reflDir * refineDist;
    }
    hitUV = ProjectToUV(refinePos);

    if (hitUV.x < 0.0f || hitUV.x > 1.0f ||
        hitUV.y < 0.0f || hitUV.y > 1.0f)
        return float4(0.0f, 0.0f, 0.0f, 0.0f);

    // Confidence: screen-edge fade × roughness fade × distance fade × Fresnel
    float edgeFade   = ScreenEdgeFade(hitUV);
    float roughFade  = 1.0f - smoothstep(0.1f, 0.6f, roughness);
    float distFade   = 1.0f - smoothstep(0.5f, 1.0f, hitT);
    float confidence = edgeFade * roughFade * distFade * fresnel;

    float3 reflColor = g_scene.SampleLevel(g_sampler, hitUV, 0).rgb;
    return float4(reflColor * Intensity, confidence);
}
