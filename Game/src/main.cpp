#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <DirectXMath.h>
#include "Engine/Core/Engine.h"
#include "Engine/Core/Logger.h"
#include "Engine/Renderer/VertexBuffer.h"
#include "Engine/Renderer/IndexBuffer.h"
#include "Engine/Renderer/ConstantBuffer.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;

struct Vertex
{
    float x, y, z;
    float r, g, b, a;
};

struct TransformCB
{
    XMFLOAT4X4 model;
    XMFLOAT4X4 view;
    XMFLOAT4X4 projection;
};

class TestScene : public SE::Engine
{
public:
    bool Setup()
    {
        ID3D11Device* device = GetRenderer().GetDevice();

        // ---- Shaders ----
        ComPtr<ID3DBlob> vsBlob, psBlob, errBlob;
        UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef SE_DEBUG
        flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
        HRESULT hr = D3DCompileFromFile(L"Assets/Shaders/Triangle.hlsl",
            nullptr, nullptr, "VS_Main", "vs_5_0", flags, 0, &vsBlob, &errBlob);
        if (FAILED(hr)) { if (errBlob) SE_LOG_ERROR("VS: %s", (char*)errBlob->GetBufferPointer()); return false; }

        hr = D3DCompileFromFile(L"Assets/Shaders/Triangle.hlsl",
            nullptr, nullptr, "PS_Main", "ps_5_0", flags, 0, &psBlob, &errBlob);
        if (FAILED(hr)) { if (errBlob) SE_LOG_ERROR("PS: %s", (char*)errBlob->GetBufferPointer()); return false; }

        SE_HR(device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &m_vs));
        SE_HR(device->CreatePixelShader(psBlob->GetBufferPointer(),  psBlob->GetBufferSize(), nullptr, &m_ps));

        // ---- Input layout ----
        D3D11_INPUT_ELEMENT_DESC layoutDesc[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,   0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };
        SE_HR(device->CreateInputLayout(layoutDesc, 2,
            vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &m_layout));

        // ---- Quad geometry ----
        Vertex verts[] =
        {
            { -0.5f,  0.5f, 0.0f,   1.0f, 0.3f, 0.3f, 1.0f },
            {  0.5f,  0.5f, 0.0f,   0.3f, 1.0f, 0.3f, 1.0f },
            { -0.5f, -0.5f, 0.0f,   0.3f, 0.3f, 1.0f, 1.0f },
            {  0.5f, -0.5f, 0.0f,   1.0f, 1.0f, 0.3f, 1.0f },
        };
        uint32_t indices[] = { 0, 1, 2,   1, 3, 2 };

        if (!m_vb.Create(device, verts, sizeof(verts), sizeof(Vertex))) return false;
        if (!m_ib.Create(device, indices, 6))                           return false;
        if (!m_transformCB.Create(device))                              return false;

        SE_LOG_INFO("TestScene ready");
        return true;
    }

protected:
    void OnUpdate() override
    {
        ID3D11DeviceContext* ctx = GetRenderer().GetContext();
        float t = static_cast<float>(GetClock().GetTotalTime());

        // Model: spin around Y axis over time.
        XMMATRIX model = XMMatrixRotationY(t);

        // View: camera sitting slightly back and above, looking at origin.
        XMMATRIX view  = XMMatrixLookAtLH(
            XMVectorSet(0.0f, 0.8f, -2.0f, 1.0f),
            XMVectorSet(0.0f, 0.0f,  0.0f, 1.0f),
            XMVectorSet(0.0f, 1.0f,  0.0f, 0.0f));

        // Projection: 60° FOV, 16:9, near=0.1, far=100.
        XMMATRIX proj  = XMMatrixPerspectiveFovLH(
            XMConvertToRadians(60.0f), 1280.0f / 720.0f, 0.1f, 100.0f);

        TransformCB cb;
        XMStoreFloat4x4(&cb.model,      model);
        XMStoreFloat4x4(&cb.view,       view);
        XMStoreFloat4x4(&cb.projection, proj);

        m_transformCB.Update(ctx, cb);
        m_transformCB.BindVS(ctx, 0);

        ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        ctx->IASetInputLayout(m_layout.Get());
        m_vb.Bind(ctx);
        m_ib.Bind(ctx);
        ctx->VSSetShader(m_vs.Get(), nullptr, 0);
        ctx->PSSetShader(m_ps.Get(), nullptr, 0);
        ctx->DrawIndexed(m_ib.GetCount(), 0, 0);
    }

private:
    ComPtr<ID3D11VertexShader>  m_vs;
    ComPtr<ID3D11PixelShader>   m_ps;
    ComPtr<ID3D11InputLayout>   m_layout;
    SE::VertexBuffer            m_vb;
    SE::IndexBuffer             m_ib;
    SE::ConstantBuffer<TransformCB> m_transformCB;
};

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    SE::WindowDesc desc;
    desc.title  = L"SlopEngine";
    desc.width  = 1280;
    desc.height = 720;

    TestScene scene;
    if (!scene.Initialize(desc)) return 1;
    if (!scene.Setup())          return 1;

    scene.Run();
    scene.Shutdown();
    return 0;
}
