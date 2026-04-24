#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include "Engine/Core/Engine.h"
#include "Engine/Core/Logger.h"
#include "Engine/Renderer/VertexBuffer.h"
#include "Engine/Renderer/IndexBuffer.h"

using Microsoft::WRL::ComPtr;

struct Vertex
{
    float x, y, z;
    float r, g, b, a;
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
        if (FAILED(hr))
        {
            if (errBlob) SE_LOG_ERROR("VS: %s", (char*)errBlob->GetBufferPointer());
            return false;
        }

        hr = D3DCompileFromFile(L"Assets/Shaders/Triangle.hlsl",
            nullptr, nullptr, "PS_Main", "ps_5_0", flags, 0, &psBlob, &errBlob);
        if (FAILED(hr))
        {
            if (errBlob) SE_LOG_ERROR("PS: %s", (char*)errBlob->GetBufferPointer());
            return false;
        }

        SE_HR(device->CreateVertexShader(vsBlob->GetBufferPointer(),
            vsBlob->GetBufferSize(), nullptr, &m_vs));
        SE_HR(device->CreatePixelShader(psBlob->GetBufferPointer(),
            psBlob->GetBufferSize(), nullptr, &m_ps));

        // ---- Input layout ----
        D3D11_INPUT_ELEMENT_DESC layoutDesc[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,   0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };
        SE_HR(device->CreateInputLayout(layoutDesc, 2,
            vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &m_layout));

        // ---- Quad geometry ----
        //  0----1
        //  |  / |
        //  | /  |
        //  2----3
        Vertex verts[] =
        {
            { -0.5f,  0.5f, 0.0f,   1.0f, 0.3f, 0.3f, 1.0f },  // 0 top-left     red
            {  0.5f,  0.5f, 0.0f,   0.3f, 1.0f, 0.3f, 1.0f },  // 1 top-right    green
            { -0.5f, -0.5f, 0.0f,   0.3f, 0.3f, 1.0f, 1.0f },  // 2 bottom-left  blue
            {  0.5f, -0.5f, 0.0f,   1.0f, 1.0f, 0.3f, 1.0f },  // 3 bottom-right yellow
        };

        uint32_t indices[] = { 0, 1, 2,   1, 3, 2 };

        if (!m_vb.Create(device, verts, sizeof(verts), sizeof(Vertex))) return false;
        if (!m_ib.Create(device, indices, 6))                           return false;

        SE_LOG_INFO("Quad ready");
        return true;
    }

protected:
    void OnUpdate() override
    {
        ID3D11DeviceContext* ctx = GetRenderer().GetContext();

        ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        ctx->IASetInputLayout(m_layout.Get());

        m_vb.Bind(ctx);
        m_ib.Bind(ctx);

        ctx->VSSetShader(m_vs.Get(), nullptr, 0);
        ctx->PSSetShader(m_ps.Get(), nullptr, 0);

        ctx->DrawIndexed(m_ib.GetCount(), 0, 0);
    }

private:
    ComPtr<ID3D11VertexShader> m_vs;
    ComPtr<ID3D11PixelShader>  m_ps;
    ComPtr<ID3D11InputLayout>  m_layout;
    SE::VertexBuffer           m_vb;
    SE::IndexBuffer            m_ib;
};

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    SE::WindowDesc desc;
    desc.title  = L"SlopEngine";
    desc.width  = 1280;
    desc.height = 720;

    TestScene scene;
    if (!scene.Initialize(desc))
        return 1;
    if (!scene.Setup())
        return 1;

    scene.Run();
    scene.Shutdown();
    return 0;
}
