#include "Engine/Renderer/FullscreenQuad.h"
#include "Engine/Core/Logger.h"
#include <DirectXMath.h>

using namespace DirectX;

namespace SE {

struct FSQuadVertex
{
    XMFLOAT2 pos;
    XMFLOAT2 uv;
};

bool FullscreenQuad::Init(ID3D11Device* device, ShaderLibrary& shaders)
{
    // Two triangles covering NDC [-1,1]
    FSQuadVertex verts[] = {
        { {-1.0f,  1.0f}, {0.0f, 0.0f} },   // top-left
        { { 1.0f,  1.0f}, {1.0f, 0.0f} },   // top-right
        { { 1.0f, -1.0f}, {1.0f, 1.0f} },   // bottom-right
        { {-1.0f, -1.0f}, {0.0f, 1.0f} },   // bottom-left
    };
    UINT16 indices[] = { 0, 1, 2, 0, 2, 3 };

    D3D11_BUFFER_DESC bd = {};
    bd.ByteWidth = sizeof(verts);
    bd.Usage     = D3D11_USAGE_IMMUTABLE;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA sd = { verts };
    HRESULT hr = device->CreateBuffer(&bd, &sd, m_vb.GetAddressOf());
    if (FAILED(hr)) { SE_LOG_ERROR("FullscreenQuad: VB creation failed"); return false; }

    bd.ByteWidth = sizeof(indices);
    bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    sd.pSysMem   = indices;
    hr = device->CreateBuffer(&bd, &sd, m_ib.GetAddressOf());
    if (FAILED(hr)) { SE_LOG_ERROR("FullscreenQuad: IB creation failed"); return false; }

    // Compile passthrough shader
    m_perm = shaders.Get(L"Shaders/Fullscreen.hlsl");
    if (!m_perm) { SE_LOG_ERROR("FullscreenQuad: failed to compile Fullscreen.hlsl"); return false; }

    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0,                            D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    hr = device->CreateInputLayout(layout, 2,
        m_perm->vsBlob->GetBufferPointer(), m_perm->vsBlob->GetBufferSize(),
        m_layout.GetAddressOf());
    if (FAILED(hr)) { SE_LOG_ERROR("FullscreenQuad: input layout creation failed"); return false; }

    // Point sampler — no filtering, exact texel fetch
    D3D11_SAMPLER_DESC samp = {};
    samp.Filter   = D3D11_FILTER_MIN_MAG_MIP_POINT;
    samp.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    samp.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    samp.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    hr = device->CreateSamplerState(&samp, m_pointSampler.GetAddressOf());
    if (FAILED(hr)) { SE_LOG_ERROR("FullscreenQuad: sampler creation failed"); return false; }

    SE_LOG_INFO("FullscreenQuad initialised");
    return true;
}

void FullscreenQuad::Shutdown()
{
    m_pointSampler.Reset();
    m_layout.Reset();
    m_ib.Reset();
    m_vb.Reset();
    m_perm = nullptr;
}

void FullscreenQuad::Draw(ID3D11DeviceContext* ctx)
{
    ctx->IASetInputLayout(m_layout.Get());
    UINT stride = sizeof(FSQuadVertex), offset = 0;
    ctx->IASetVertexBuffers(0, 1, m_vb.GetAddressOf(), &stride, &offset);
    ctx->IASetIndexBuffer(m_ib.Get(), DXGI_FORMAT_R16_UINT, 0);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    ctx->VSSetShader(m_perm->vs.Get(), nullptr, 0);
    ctx->PSSetShader(m_perm->ps.Get(), nullptr, 0);
    ctx->PSSetSamplers(0, 1, m_pointSampler.GetAddressOf());

    ctx->DrawIndexed(6, 0, 0);
}

void FullscreenQuad::DrawGeometryOnly(ID3D11DeviceContext* ctx)
{
    ctx->IASetInputLayout(m_layout.Get());
    UINT stride = sizeof(FSQuadVertex), offset = 0;
    ctx->IASetVertexBuffers(0, 1, m_vb.GetAddressOf(), &stride, &offset);
    ctx->IASetIndexBuffer(m_ib.Get(), DXGI_FORMAT_R16_UINT, 0);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    ctx->DrawIndexed(6, 0, 0);
}

} // namespace SE
