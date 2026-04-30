// IBL shader — equirect-to-cubemap, irradiance convolution, specular prefilter, BRDF LUT.
// Each pass compiled via #ifdef: PASS_EQUIRECT, PASS_IRRADIANCE, PASS_PREFILTER, PASS_BRDF.

static const float PI = 3.14159265358979f;

// --- Common ---
cbuffer IBLCB : register(b0)
{
    int   FaceIndex;
    float Roughness;
    float EnvResolution;   // env cubemap face size (e.g. 512)
    float _iblPad;
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

// Convert (uv, face) → world-space direction for cubemap texel
float3 GetCubeDirection(float2 uv, int face)
{
    float2 ndc = uv * 2.0f - 1.0f;
    ndc.y = -ndc.y;
    float3 dir;
    switch (face)
    {
        case 0: dir = float3( 1.0f, ndc.y, -ndc.x); break; // +X
        case 1: dir = float3(-1.0f, ndc.y,  ndc.x); break; // -X
        case 2: dir = float3( ndc.x,  1.0f, -ndc.y); break; // +Y
        case 3: dir = float3( ndc.x, -1.0f,  ndc.y); break; // -Y
        case 4: dir = float3( ndc.x, ndc.y,   1.0f); break; // +Z
        case 5: dir = float3(-ndc.x, ndc.y,  -1.0f); break; // -Z
        default: dir = float3(0, 0, 1); break;
    }
    return normalize(dir);
}

// Equirectangular UV from direction
float2 DirToEquirect(float3 d)
{
    float u = atan2(d.z, d.x) * (0.5f / PI) + 0.5f;
    float v = 0.5f - asin(clamp(d.y, -1.0f, 1.0f)) / PI;
    return float2(u, v);
}

// --- Van der Corput / Hammersley ---
float RadicalInverse_VdC(uint bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10f;
}

float2 Hammersley(uint i, uint N)
{
    return float2(float(i) / float(N), RadicalInverse_VdC(i));
}

// GGX importance sampling — returns half-vector in tangent space of N
float3 ImportanceSampleGGX(float2 Xi, float3 N, float roughness)
{
    float a = roughness * roughness;

    float phi      = 2.0f * PI * Xi.x;
    float cosTheta = sqrt((1.0f - Xi.y) / (1.0f + (a * a - 1.0f) * Xi.y));
    float sinTheta = sqrt(1.0f - cosTheta * cosTheta);

    float3 H;
    H.x = cos(phi) * sinTheta;
    H.y = sin(phi) * sinTheta;
    H.z = cosTheta;

    // Tangent-space to world
    float3 up    = abs(N.z) < 0.999f ? float3(0, 0, 1) : float3(1, 0, 0);
    float3 tangX = normalize(cross(up, N));
    float3 tangY = cross(N, tangX);

    return normalize(tangX * H.x + tangY * H.y + N * H.z);
}

// Smith-Schlick geometry (IBL version: k = roughness²/2)
float GeometrySchlickGGX_IBL(float NdotX, float roughness)
{
    float a = roughness;
    float k = (a * a) / 2.0f;
    return NdotX / (NdotX * (1.0f - k) + k);
}

float GeometrySmith_IBL(float3 N, float3 V, float3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0f);
    float NdotL = max(dot(N, L), 0.0f);
    return GeometrySchlickGGX_IBL(NdotV, roughness) *
           GeometrySchlickGGX_IBL(NdotL, roughness);
}

// ======================== PASS: Equirect → Cubemap ========================
#ifdef PASS_EQUIRECT
Texture2D    g_panorama : register(t0);
SamplerState g_sampler  : register(s0);

float4 PS_Main(VSOutput input) : SV_TARGET
{
    float3 dir = GetCubeDirection(input.uv, FaceIndex);
    float2 eq  = DirToEquirect(dir);
    return g_panorama.Sample(g_sampler, eq);
}
#endif

// ======================== PASS: Irradiance Convolution ========================
#ifdef PASS_IRRADIANCE
TextureCube  g_envCube  : register(t0);
SamplerState g_sampler  : register(s0);

float4 PS_Main(VSOutput input) : SV_TARGET
{
    float3 N = GetCubeDirection(input.uv, FaceIndex);
    float3 irradiance = float3(0, 0, 0);

    float3 up    = abs(N.y) < 0.999f ? float3(0, 1, 0) : float3(0, 0, 1);
    float3 right = normalize(cross(up, N));
    up = cross(N, right);

    float sampleDelta = 0.025f;
    float nrSamples = 0.0f;

    for (float phi = 0.0f; phi < 2.0f * PI; phi += sampleDelta)
    {
        for (float theta = 0.0f; theta < 0.5f * PI; theta += sampleDelta)
        {
            float3 tangentSample = float3(sin(theta) * cos(phi),
                                           sin(theta) * sin(phi),
                                           cos(theta));
            float3 sampleVec = tangentSample.x * right +
                               tangentSample.y * up +
                               tangentSample.z * N;

            irradiance += g_envCube.Sample(g_sampler, sampleVec).rgb *
                          cos(theta) * sin(theta);
            nrSamples += 1.0f;
        }
    }
    irradiance = PI * irradiance / max(nrSamples, 1.0f);

    return float4(irradiance, 1.0f);
}
#endif

// GGX NDF for prefilter LOD computation
float D_GGX_IBL(float NdotH, float roughness)
{
    float a  = roughness * roughness;
    float a2 = a * a;
    float d  = NdotH * NdotH * (a2 - 1.0f) + 1.0f;
    return a2 / max(PI * d * d, 0.0001f);
}

// ======================== PASS: Pre-filtered Specular ========================
#ifdef PASS_PREFILTER
TextureCube  g_envCube  : register(t0);
SamplerState g_sampler  : register(s0);

float4 PS_Main(VSOutput input) : SV_TARGET
{
    float3 N = GetCubeDirection(input.uv, FaceIndex);
    float3 R = N;
    float3 V = R;

    const uint SAMPLE_COUNT = 1024u;
    float3 prefilteredColor = float3(0, 0, 0);
    float totalWeight = 0.0f;

    for (uint i = 0u; i < SAMPLE_COUNT; ++i)
    {
        float2 Xi = Hammersley(i, SAMPLE_COUNT);
        float3 H  = ImportanceSampleGGX(Xi, N, Roughness);
        float3 L  = normalize(2.0f * dot(V, H) * H - V);

        float NdotL = max(dot(N, L), 0.0f);
        if (NdotL > 0.0f)
        {
            float NdotH = max(dot(N, H), 0.0f);
            float HdotV = max(dot(H, V), 0.0f);

            // PDF-based LOD to reduce firefly artifacts from bright point sources
            float D   = D_GGX_IBL(NdotH, Roughness);
            float pdf = D * NdotH / (4.0f * HdotV) + 0.0001f;

            float saTexel  = 4.0f * PI / (6.0f * EnvResolution * EnvResolution);
            float saSample = 1.0f / (float(SAMPLE_COUNT) * pdf + 0.0001f);
            float mipLevel = (Roughness == 0.0f) ? 0.0f : 0.5f * log2(saSample / saTexel);

            prefilteredColor += g_envCube.SampleLevel(g_sampler, L, mipLevel).rgb * NdotL;
            totalWeight += NdotL;
        }
    }
    prefilteredColor /= max(totalWeight, 0.001f);
    return float4(prefilteredColor, 1.0f);
}
#endif

// ======================== PASS: BRDF Integration LUT ========================
#ifdef PASS_BRDF
float4 PS_Main(VSOutput input) : SV_TARGET
{
    float NdotV    = input.uv.x;
    float roughness = input.uv.y;

    float3 V;
    V.x = sqrt(1.0f - NdotV * NdotV);
    V.y = 0.0f;
    V.z = NdotV;

    float A = 0.0f;
    float B = 0.0f;

    float3 N = float3(0, 0, 1);

    const uint SAMPLE_COUNT = 1024u;
    for (uint i = 0u; i < SAMPLE_COUNT; ++i)
    {
        float2 Xi = Hammersley(i, SAMPLE_COUNT);
        float3 H  = ImportanceSampleGGX(Xi, N, roughness);
        float3 L  = normalize(2.0f * dot(V, H) * H - V);

        float NdotL = max(L.z, 0.0f);
        float NdotH = max(H.z, 0.0f);
        float VdotH = max(dot(V, H), 0.0f);

        if (NdotL > 0.0f)
        {
            float G     = GeometrySmith_IBL(N, V, L, roughness);
            float G_Vis = (G * VdotH) / max(NdotH * NdotV, 0.001f);
            float Fc    = pow(1.0f - VdotH, 5.0f);

            A += (1.0f - Fc) * G_Vis;
            B += Fc * G_Vis;
        }
    }

    A /= float(SAMPLE_COUNT);
    B /= float(SAMPLE_COUNT);

    return float4(A, B, 0.0f, 1.0f);
}
#endif
