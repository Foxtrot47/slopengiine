#include "Engine/Renderer/ForwardPipeline.h"
#include "Engine/Core/Logger.h"
#include <d3dcompiler.h>
#include <cmath>

namespace SE {

namespace {

void BuildSphereMesh(float radius, int rings, int segs,
    std::vector<MeshVertex>& verts, std::vector<uint32_t>& indices)
{
    const float pi = DirectX::XM_PI;
    for (int r = 0; r <= rings; ++r)
    {
        float phi = -pi * 0.5f + pi * r / rings;
        for (int s = 0; s <= segs; ++s)
        {
            float theta = DirectX::XM_2PI * s / segs;
            float cp = cosf(phi), sp = sinf(phi);
            float ct = cosf(theta), st = sinf(theta);

            MeshVertex v;
            v.x  = cp * ct * radius; v.y = sp * radius; v.z = cp * st * radius;
            v.nx = cp * ct;          v.ny = sp;          v.nz = cp * st;
            v.u  = (float)s / segs;  v.v  = (float)r / rings;
            v.tx = -st;      v.ty = 0.0f; v.tz = ct;
            v.bx = -sp * ct; v.by = cp;   v.bz = -sp * st;
            verts.push_back(v);
        }
    }
    for (int r = 0; r < rings; ++r)
    {
        for (int s = 0; s < segs; ++s)
        {
            uint32_t i0 = static_cast<uint32_t>(r * (segs + 1) + s);
            uint32_t i1 = i0 + 1;
            uint32_t i2 = i0 + static_cast<uint32_t>(segs + 1);
            uint32_t i3 = i2 + 1;
            indices.push_back(i0); indices.push_back(i2); indices.push_back(i1);
            indices.push_back(i1); indices.push_back(i2); indices.push_back(i3);
        }
    }
}

} // anonymous namespace

bool ForwardPipeline::Init(ID3D11Device* device, AssetManager& assets)
{
    Microsoft::WRL::ComPtr<ID3DBlob> vsBlob, psBlob, errBlob;
    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef SE_DEBUG
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    HRESULT hr = D3DCompileFromFile(L"Shaders/Basic.hlsl",
        nullptr, nullptr, "VS_Main", "vs_5_0", flags, 0, &vsBlob, &errBlob);
    if (FAILED(hr))
    {
        if (errBlob) SE_LOG_ERROR("ForwardPipeline VS: %s", (char*)errBlob->GetBufferPointer());
        return false;
    }

    hr = D3DCompileFromFile(L"Shaders/Basic.hlsl",
        nullptr, nullptr, "PS_Main", "ps_5_0", flags, 0, &psBlob, &errBlob);
    if (FAILED(hr))
    {
        if (errBlob) SE_LOG_ERROR("ForwardPipeline PS: %s", (char*)errBlob->GetBufferPointer());
        return false;
    }

    SE_HR(device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &m_vs));
    SE_HR(device->CreatePixelShader( psBlob->GetBufferPointer(),  psBlob->GetBufferSize(), nullptr, &m_ps));

    D3D11_INPUT_ELEMENT_DESC layoutDesc[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TANGENT",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "BINORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 44, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    SE_HR(device->CreateInputLayout(layoutDesc, 5,
        vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &m_layout));

    if (!m_transformCB.Create(device)) return false;
    if (!m_materialCB.Create(device))  return false;

    if (!m_sampler.Create(device, { FilterMode::Anisotropic, AddressMode::Wrap }))
        return false;

    {
        std::vector<MeshVertex> verts;
        std::vector<uint32_t>   idx;
        BuildSphereMesh(1.0f, 16, 16, verts, idx);
        m_sphereVB.Create(device, verts.data(),
            static_cast<uint32_t>(verts.size() * sizeof(MeshVertex)), sizeof(MeshVertex));
        m_sphereIB.Create(device, idx.data(), static_cast<uint32_t>(idx.size()));
    }

    m_defaultWhite  = assets.GetDefaultWhite();
    m_defaultNormal = assets.GetDefaultNormal();
    return true;
}

std::vector<ForwardPipeline::SubMat> ForwardPipeline::LoadMeshMaterials(AssetManager& assets, const Mesh& mesh)
{
    auto toWide = [](const std::string& s) -> std::wstring {
        return std::wstring(s.begin(), s.end());
    };

    const std::string& dir = mesh.GetDirectory();
    std::vector<SubMat> mats(mesh.GetSubMeshCount());

    for (uint32_t i = 0; i < mesh.GetSubMeshCount(); ++i)
    {
        SubMeshInfo info = mesh.GetSubMeshInfo(i);
        SubMat& mat = mats[i];

        if (!info.albedoPath.empty())
            mat.albedo = assets.GetTexture(toWide(dir + info.albedoPath));
        if (!mat.albedo)
            mat.albedo = assets.GetDefaultWhite();

        if (!info.normalPath.empty())
            mat.normal = assets.GetTexture(toWide(dir + info.normalPath));
        if (!mat.normal)
            mat.normal = assets.GetDefaultNormal();

        if (!info.roughnessPath.empty())
            mat.roughness = assets.GetTexture(toWide(dir + info.roughnessPath));
        if (!mat.roughness)
            mat.roughness = assets.GetDefaultWhite();
    }

    return mats;
}

void ForwardPipeline::Begin(ID3D11DeviceContext* ctx, DirectX::XMMATRIX view, DirectX::XMMATRIX proj)
{
    m_view = view;
    m_proj = proj;

    m_sampler.BindPS(ctx, 0);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx->IASetInputLayout(m_layout.Get());
    ctx->VSSetShader(m_vs.Get(), nullptr, 0);
    ctx->PSSetShader(m_ps.Get(), nullptr, 0);
}

void ForwardPipeline::SetMaterialParams(ID3D11DeviceContext* ctx,
                                         DirectX::XMFLOAT3 tint, float roughnessScale, float metallic)
{
    MaterialParamsCBData mc;
    mc.albedoTint     = tint;
    mc.roughnessScale = roughnessScale;
    mc.metallic       = metallic;
    mc._pad[0] = mc._pad[1] = mc._pad[2] = 0.0f;
    m_materialCB.Update(ctx, mc);
    m_materialCB.BindPS(ctx, 3);
}

void ForwardPipeline::DrawMesh(ID3D11DeviceContext* ctx, const Mesh& mesh,
                                DirectX::XMMATRIX model, const std::vector<SubMat>& mats)
{
    using namespace DirectX;

    TransformCBData cb;
    XMStoreFloat4x4(&cb.model,      model);
    XMStoreFloat4x4(&cb.view,       m_view);
    XMStoreFloat4x4(&cb.projection, m_proj);
    m_transformCB.Update(ctx, cb);
    m_transformCB.BindVS(ctx, 0);

    for (uint32_t i = 0; i < mesh.GetSubMeshCount(); ++i)
    {
        mats[i].albedo->BindPS(ctx, 0);
        mats[i].roughness->BindPS(ctx, 1);
        mats[i].normal->BindPS(ctx, 2);
        mesh.DrawSubMesh(ctx, i);
    }
}

void ForwardPipeline::DrawSphere(ID3D11DeviceContext* ctx,
                                  DirectX::XMFLOAT3 position, float radius, DirectX::XMFLOAT3 tint)
{
    using namespace DirectX;

    SetMaterialParams(ctx, tint, 0.7f, 0.0f);

    m_defaultWhite->BindPS(ctx, 0);
    m_defaultWhite->BindPS(ctx, 1);
    m_defaultNormal->BindPS(ctx, 2);

    TransformCBData cb;
    XMStoreFloat4x4(&cb.model,
        XMMatrixScaling(radius, radius, radius) *
        XMMatrixTranslation(position.x, position.y, position.z));
    XMStoreFloat4x4(&cb.view,       m_view);
    XMStoreFloat4x4(&cb.projection, m_proj);
    m_transformCB.Update(ctx, cb);
    m_transformCB.BindVS(ctx, 0);

    m_sphereVB.Bind(ctx);
    m_sphereIB.Bind(ctx);
    ctx->DrawIndexed(m_sphereIB.GetCount(), 0, 0);
}

} // namespace SE
