#include "Engine/Renderer/ForwardPipeline.h"
#include "Engine/Core/Logger.h"
#include <d3dcompiler.h>
#include <cmath>

namespace SE {

namespace {

void BuildWireAABB(std::vector<MeshVertex>& verts, std::vector<uint32_t>& indices)
{
    // Unit cube corners (-1..1); scaled in draw call to match AABB half-extents.
    static const float c[8][3] = {
        {-1,-1,-1}, {1,-1,-1}, {1,1,-1}, {-1,1,-1},
        {-1,-1, 1}, {1,-1, 1}, {1,1, 1}, {-1,1, 1},
    };
    for (auto& v : c) { MeshVertex mv = {}; mv.x = v[0]; mv.y = v[1]; mv.z = v[2]; verts.push_back(mv); }

    static const uint32_t edges[24] = {
        0,1, 1,2, 2,3, 3,0,   // front face
        4,5, 5,6, 6,7, 7,4,   // back face
        0,4, 1,5, 2,6, 3,7,   // connecting edges
    };
    for (auto i : edges) indices.push_back(i);
}

void BuildWireSphere(int segs, std::vector<MeshVertex>& verts, std::vector<uint32_t>& indices)
{
    // Three great circles (XZ, XY, YZ planes); radius=1, scaled in draw call.
    for (int ring = 0; ring < 3; ++ring)
    {
        auto base = static_cast<uint32_t>(verts.size());
        for (int i = 0; i < segs; ++i)
        {
            float t = DirectX::XM_2PI * i / segs;
            float c = cosf(t), s = sinf(t);
            MeshVertex v = {};
            if      (ring == 0) { v.x = c; v.y = 0; v.z = s; }
            else if (ring == 1) { v.x = c; v.y = s; v.z = 0; }
            else                { v.x = 0; v.y = c; v.z = s; }
            verts.push_back(v);
        }
        for (int i = 0; i < segs; ++i)
        {
            indices.push_back(base + i);
            indices.push_back(base + (i + 1) % static_cast<uint32_t>(segs));
        }
    }
}

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

bool ForwardPipeline::Init(ID3D11Device* device, AssetManager& assets, ShaderLibrary& shaders)
{
    const ShaderPermutation* perm = shaders.Get(L"Shaders/Basic.hlsl");
    if (!perm)
    {
        SE_LOG_ERROR("ForwardPipeline: failed to compile Basic.hlsl via ShaderLibrary");
        return false;
    }
    m_vs = perm->vs;
    m_ps = perm->ps;

    D3D11_INPUT_ELEMENT_DESC layoutDesc[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TANGENT",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "BINORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 44, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    SE_HR(device->CreateInputLayout(layoutDesc, 5,
        perm->vsBlob->GetBufferPointer(), perm->vsBlob->GetBufferSize(), &m_layout));

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
    {
        std::vector<MeshVertex> verts;
        std::vector<uint32_t>   idx;
        BuildWireSphere(32, verts, idx);
        m_wireSphereVB.Create(device, verts.data(),
            static_cast<uint32_t>(verts.size() * sizeof(MeshVertex)), sizeof(MeshVertex));
        m_wireSphereIB.Create(device, idx.data(), static_cast<uint32_t>(idx.size()));
    }
    {
        std::vector<MeshVertex> verts;
        std::vector<uint32_t>   idx;
        BuildWireAABB(verts, idx);
        m_wireAABBVB.Create(device, verts.data(),
            static_cast<uint32_t>(verts.size() * sizeof(MeshVertex)), sizeof(MeshVertex));
        m_wireAABBIB.Create(device, idx.data(), static_cast<uint32_t>(idx.size()));
    }

    {
        D3D11_BUFFER_DESC bd  = {};
        bd.Usage              = D3D11_USAGE_DYNAMIC;
        bd.ByteWidth          = sizeof(MeshVertex) * 2;
        bd.BindFlags          = D3D11_BIND_VERTEX_BUFFER;
        bd.CPUAccessFlags     = D3D11_CPU_ACCESS_WRITE;
        SE_HR(device->CreateBuffer(&bd, nullptr, &m_lineBuffer));
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
    m_queue.Clear();
    m_queuedDraws.clear();

    m_sampler.BindPS(ctx, 0);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx->IASetInputLayout(m_layout.Get());
    ctx->VSSetShader(m_vs.Get(), nullptr, 0);
    ctx->PSSetShader(m_ps.Get(), nullptr, 0);
}

void ForwardPipeline::SubmitMesh(const Mesh& mesh, DirectX::XMMATRIX model,
                                  const std::vector<SubMat>& mats, bool transparent)
{
    using namespace DirectX;

    // Compute camera-space Z of the mesh origin for sorting.
    XMVECTOR origin = XMVector3Transform(XMVectorSet(0, 0, 0, 1), model);
    XMVECTOR viewOrigin = XMVector3Transform(origin, m_view);
    float depth = XMVectorGetZ(viewOrigin);

    uint32_t drawIdx = static_cast<uint32_t>(m_queuedDraws.size());
    m_queuedDraws.push_back({ &mesh, &mats });

    for (uint32_t i = 0; i < mesh.GetSubMeshCount(); ++i)
    {
        RenderItem item;
        item.model        = model;
        item.meshIndex    = drawIdx;
        item.subMeshIndex = i;
        item.sortDepth    = depth;
        item.transparent  = transparent;
        m_queue.Push(item);
    }
}

void ForwardPipeline::Flush(ID3D11DeviceContext* ctx)
{
    using namespace DirectX;

    m_queue.Sort();
    m_lastDrawCalls = 0;

    for (auto& item : m_queue.Items())
    {
        auto& draw = m_queuedDraws[item.meshIndex];

        TransformCBData cb;
        XMStoreFloat4x4(&cb.model,      item.model);
        XMStoreFloat4x4(&cb.view,       m_view);
        XMStoreFloat4x4(&cb.projection, m_proj);
        m_transformCB.Update(ctx, cb);
        m_transformCB.BindVS(ctx, 0);

        auto& mat = (*draw.mats)[item.subMeshIndex];
        mat.albedo->BindPS(ctx, 0);
        mat.roughness->BindPS(ctx, 1);
        mat.normal->BindPS(ctx, 2);
        draw.mesh->DrawSubMesh(ctx, item.subMeshIndex);
        ++m_lastDrawCalls;
    }
}

void ForwardPipeline::SetMaterialParams(ID3D11DeviceContext* ctx,
                                         DirectX::XMFLOAT3 tint, float roughnessScale, float metallic)
{
    MaterialParamsCBData mc;
    mc.albedoTint     = tint;
    mc.roughnessScale = roughnessScale;
    mc.metallic       = metallic;
    mc.unlit          = 0.0f;
    mc._pad[0] = mc._pad[1] = 0.0f;
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

void ForwardPipeline::DrawWireSphere(ID3D11DeviceContext* ctx,
                                      DirectX::XMFLOAT3 position, float radius,
                                      DirectX::XMFLOAT3 color)
{
    using namespace DirectX;

    MaterialParamsCBData mc = {};
    mc.albedoTint = color; mc.roughnessScale = 1.0f; mc.unlit = 1.0f;
    m_materialCB.Update(ctx, mc);
    m_materialCB.BindPS(ctx, 3);

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

    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
    m_wireSphereVB.Bind(ctx);
    m_wireSphereIB.Bind(ctx);
    ctx->DrawIndexed(m_wireSphereIB.GetCount(), 0, 0);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void ForwardPipeline::DrawWireAABB(ID3D11DeviceContext* ctx,
                                    DirectX::XMFLOAT3 mn, DirectX::XMFLOAT3 mx,
                                    DirectX::XMFLOAT3 color)
{
    using namespace DirectX;

    MaterialParamsCBData mc = {};
    mc.albedoTint = color; mc.roughnessScale = 1.0f; mc.unlit = 1.0f;
    m_materialCB.Update(ctx, mc);
    m_materialCB.BindPS(ctx, 3);

    m_defaultWhite->BindPS(ctx, 0);
    m_defaultWhite->BindPS(ctx, 1);
    m_defaultNormal->BindPS(ctx, 2);

    // Unit cube spans -1..1; scale by half-extents then translate to center.
    float hx = (mx.x - mn.x) * 0.5f, cx = (mn.x + mx.x) * 0.5f;
    float hy = (mx.y - mn.y) * 0.5f, cy = (mn.y + mx.y) * 0.5f;
    float hz = (mx.z - mn.z) * 0.5f, cz = (mn.z + mx.z) * 0.5f;

    TransformCBData cb;
    XMStoreFloat4x4(&cb.model,
        XMMatrixScaling(hx, hy, hz) *
        XMMatrixTranslation(cx, cy, cz));
    XMStoreFloat4x4(&cb.view,       m_view);
    XMStoreFloat4x4(&cb.projection, m_proj);
    m_transformCB.Update(ctx, cb);
    m_transformCB.BindVS(ctx, 0);

    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
    m_wireAABBVB.Bind(ctx);
    m_wireAABBIB.Bind(ctx);
    ctx->DrawIndexed(m_wireAABBIB.GetCount(), 0, 0);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void ForwardPipeline::DrawWireBox(ID3D11DeviceContext* ctx,
                                   DirectX::XMMATRIX world, DirectX::XMFLOAT3 color)
{
    using namespace DirectX;

    MaterialParamsCBData mc = {};
    mc.albedoTint = color; mc.roughnessScale = 1.0f; mc.unlit = 1.0f;
    m_materialCB.Update(ctx, mc);
    m_materialCB.BindPS(ctx, 3);

    m_defaultWhite->BindPS(ctx, 0);
    m_defaultWhite->BindPS(ctx, 1);
    m_defaultNormal->BindPS(ctx, 2);

    TransformCBData cb;
    XMStoreFloat4x4(&cb.model,      world);
    XMStoreFloat4x4(&cb.view,       m_view);
    XMStoreFloat4x4(&cb.projection, m_proj);
    m_transformCB.Update(ctx, cb);
    m_transformCB.BindVS(ctx, 0);

    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
    m_wireAABBVB.Bind(ctx);
    m_wireAABBIB.Bind(ctx);
    ctx->DrawIndexed(m_wireAABBIB.GetCount(), 0, 0);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void ForwardPipeline::DrawLine(ID3D11DeviceContext* ctx,
                               DirectX::XMFLOAT3 from, DirectX::XMFLOAT3 to,
                               DirectX::XMFLOAT3 color)
{
    using namespace DirectX;

    MaterialParamsCBData mc = {};
    mc.albedoTint = color; mc.roughnessScale = 1.0f; mc.unlit = 1.0f;
    m_materialCB.Update(ctx, mc);
    m_materialCB.BindPS(ctx, 3);

    m_defaultWhite->BindPS(ctx, 0);
    m_defaultWhite->BindPS(ctx, 1);
    m_defaultNormal->BindPS(ctx, 2);

    // Identity model — vertices are supplied in world space.
    TransformCBData cb;
    XMStoreFloat4x4(&cb.model,      XMMatrixIdentity());
    XMStoreFloat4x4(&cb.view,       m_view);
    XMStoreFloat4x4(&cb.projection, m_proj);
    m_transformCB.Update(ctx, cb);
    m_transformCB.BindVS(ctx, 0);

    MeshVertex verts[2] = {};
    verts[0].x = from.x; verts[0].y = from.y; verts[0].z = from.z;
    verts[1].x = to.x;   verts[1].y = to.y;   verts[1].z = to.z;

    D3D11_MAPPED_SUBRESOURCE mapped = {};
    SE_HR(ctx->Map(m_lineBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped));
    memcpy(mapped.pData, verts, sizeof(verts));
    ctx->Unmap(m_lineBuffer.Get(), 0);

    UINT stride = sizeof(MeshVertex), offset = 0;
    ctx->IASetVertexBuffers(0, 1, m_lineBuffer.GetAddressOf(), &stride, &offset);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
    ctx->Draw(2, 0);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void ForwardPipeline::DrawWireDisc(ID3D11DeviceContext* ctx,
                                    DirectX::XMFLOAT3 center, float radius,
                                    DirectX::XMFLOAT3 color)
{
    using namespace DirectX;

    MaterialParamsCBData mc = {};
    mc.albedoTint = color; mc.roughnessScale = 1.0f; mc.unlit = 1.0f;
    m_materialCB.Update(ctx, mc);
    m_materialCB.BindPS(ctx, 3);

    m_defaultWhite->BindPS(ctx, 0);
    m_defaultWhite->BindPS(ctx, 1);
    m_defaultNormal->BindPS(ctx, 2);

    TransformCBData cb;
    XMStoreFloat4x4(&cb.model,
        XMMatrixScaling(radius, radius, radius) *
        XMMatrixTranslation(center.x, center.y, center.z));
    XMStoreFloat4x4(&cb.view,       m_view);
    XMStoreFloat4x4(&cb.projection, m_proj);
    m_transformCB.Update(ctx, cb);
    m_transformCB.BindVS(ctx, 0);

    // Reuse ring 0 of the wire sphere mesh — it sits in the XZ plane (y=0).
    // 32 segments × 2 indices per line = 64 indices.
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
    m_wireSphereVB.Bind(ctx);
    m_wireSphereIB.Bind(ctx);
    ctx->DrawIndexed(64, 0, 0);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

} // namespace SE
