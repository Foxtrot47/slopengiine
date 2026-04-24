#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <DirectXMath.h>
#include <imgui.h>
#include "Engine/Core/Engine.h"
#include "Engine/Core/Logger.h"
#include "Engine/Renderer/Mesh.h"
#include "Engine/Renderer/ConstantBuffer.h"
#include "Engine/Renderer/Texture2D.h"
#include "Engine/Renderer/SamplerState.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;

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
        HRESULT hr = D3DCompileFromFile(L"Shaders/Basic.hlsl",
            nullptr, nullptr, "VS_Main", "vs_5_0", flags, 0, &vsBlob, &errBlob);
        if (FAILED(hr)) { if (errBlob) SE_LOG_ERROR("VS: %s", (char*)errBlob->GetBufferPointer()); return false; }

        hr = D3DCompileFromFile(L"Shaders/Basic.hlsl",
            nullptr, nullptr, "PS_Main", "ps_5_0", flags, 0, &psBlob, &errBlob);
        if (FAILED(hr)) { if (errBlob) SE_LOG_ERROR("PS: %s", (char*)errBlob->GetBufferPointer()); return false; }

        SE_HR(device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &m_vs));
        SE_HR(device->CreatePixelShader(psBlob->GetBufferPointer(),  psBlob->GetBufferSize(), nullptr, &m_ps));

        // ---- Input layout — must match MeshVertex exactly ----
        D3D11_INPUT_ELEMENT_DESC layoutDesc[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };
        SE_HR(device->CreateInputLayout(layoutDesc, 3,
            vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &m_layout));

        if (!m_transformCB.Create(device)) return false;

        // ---- Load FBX mesh ----
        if (!m_mesh.Load(device, "Assets/Meshes/Mossy_Stone_Wall_ukhgdfyga_Low.fbx"))
            return false;

        // ---- Load BaseColor texture ----
        if (!m_texture.LoadFromFile(device,
            L"Assets/Textures/Mossy_Stone_Wall_ukhgdfyga_Low_1K_BaseColor.jpg"))
            return false;

        // ---- Anisotropic sampler — this is the game-quality default ----
        if (!m_sampler.Create(device, { SE::FilterMode::Anisotropic, SE::AddressMode::Wrap }))
            return false;

        SE_LOG_INFO("TestScene ready — mossy stone wall");
        return true;
    }

protected:
    void OnUpdate() override
    {
        ID3D11DeviceContext* ctx = GetRenderer().GetContext();
        float t = static_cast<float>(GetClock().GetTotalTime());

        // ---- Debug panel ----
        const SE::InputManager& input = GetInput();
        ImGui::Begin("Scene");
        ImGui::Text("%.1f fps  |  %.2f ms",
                    GetClock().GetFPS(), GetClock().GetDeltaTime() * 1000.0f);
        ImGui::Separator();
        ImGui::SliderFloat("Scale",     &m_scale,    0.001f, 0.1f,  "%.4f");
        ImGui::SliderFloat("Rot Speed", &m_rotSpeed, 0.0f,   3.0f);
        ImGui::Separator();
        ImGui::Text("Mouse delta  %+d  %+d", input.GetMouseDeltaX(), input.GetMouseDeltaY());
        ImGui::Text("WASD  %d %d %d %d",
            input.IsKeyDown('W'), input.IsKeyDown('A'),
            input.IsKeyDown('S'), input.IsKeyDown('D'));
        ImGui::End();

        XMMATRIX model = XMMatrixScaling(m_scale, m_scale, m_scale)
                       * XMMatrixRotationY(t * m_rotSpeed);
        XMMATRIX view  = XMMatrixLookAtLH(
            XMVectorSet(0.0f, 2.0f, -10.0f, 1.0f),
            XMVectorSet(0.0f, 2.0f,   0.0f, 1.0f),
            XMVectorSet(0.0f, 1.0f,   0.0f, 0.0f));
        XMMATRIX proj  = XMMatrixPerspectiveFovLH(
            XMConvertToRadians(60.0f), 1280.0f / 720.0f, 0.1f, 100.0f);

        TransformCB cb;
        XMStoreFloat4x4(&cb.model,      model);
        XMStoreFloat4x4(&cb.view,       view);
        XMStoreFloat4x4(&cb.projection, proj);
        m_transformCB.Update(ctx, cb);
        m_transformCB.BindVS(ctx, 0);

        m_texture.BindPS(ctx, 0);
        m_sampler.BindPS(ctx, 0);

        ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        ctx->IASetInputLayout(m_layout.Get());
        ctx->VSSetShader(m_vs.Get(), nullptr, 0);
        ctx->PSSetShader(m_ps.Get(), nullptr, 0);

        m_mesh.Draw(ctx);
    }

private:
    ComPtr<ID3D11VertexShader>      m_vs;
    ComPtr<ID3D11PixelShader>       m_ps;
    ComPtr<ID3D11InputLayout>       m_layout;
    SE::Mesh                        m_mesh;
    SE::ConstantBuffer<TransformCB> m_transformCB;
    SE::Texture2D                   m_texture;
    SE::SamplerState                m_sampler;

    float m_scale    = 0.02f;
    float m_rotSpeed = 0.4f;
};

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    SE::WindowDesc desc;
    desc.title  = L"FoxEngine";
    desc.width  = 1280;
    desc.height = 720;

    TestScene scene;
    if (!scene.Initialize(desc)) return 1;
    if (!scene.Setup())          return 1;

    scene.Run();
    scene.Shutdown();
    return 0;
}
