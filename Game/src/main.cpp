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
#include "Engine/Input/ActionMap.h"

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

        // ---- Input bindings ----
        m_actions.Bind("RotFaster", VK_RIGHT);
        m_actions.Bind("RotFaster", 'D');
        m_actions.Bind("RotSlower", VK_LEFT);
        m_actions.Bind("RotSlower", 'A');
        m_actions.Bind("ScaleUp",   VK_UP);
        m_actions.Bind("ScaleDown", VK_DOWN);

        SE_LOG_INFO("TestScene ready — mossy stone wall");
        return true;
    }

protected:
    void OnUpdate() override
    {
        ID3D11DeviceContext* ctx = GetRenderer().GetContext();
        float dt = GetClock().GetDeltaTime();

        // ---- Action map update ----
        m_actions.Update(GetInput());
        m_rotAngle += dt * m_rotSpeed;

        if (m_actions.IsHeld("RotFaster"))  m_rotSpeed = min(3.0f, m_rotSpeed + dt * 1.5f);
        if (m_actions.IsHeld("RotSlower"))  m_rotSpeed = max(0.0f, m_rotSpeed - dt * 1.5f);
        if (m_actions.IsHeld("ScaleUp"))    m_scale    = min(0.1f, m_scale    + dt * 0.01f);
        if (m_actions.IsHeld("ScaleDown"))  m_scale    = max(0.001f, m_scale  - dt * 0.01f);

        // ---- Debug panel ----
        const SE::InputManager& input = GetInput();
        ImGui::Begin("Scene");
        ImGui::Text("%.1f fps  |  %.2f ms",
                    GetClock().GetFPS(), GetClock().GetDeltaTime() * 1000.0f);
        ImGui::Separator();
        ImGui::SliderFloat("Scale",     &m_scale,    0.001f, 0.1f, "%.4f");
        ImGui::SliderFloat("Rot Speed", &m_rotSpeed, 0.0f,   3.0f);
        ImGui::Separator();
        ImGui::Text("Mouse  %+d  %+d", input.GetMouseDeltaX(), input.GetMouseDeltaY());
        ImGui::Text("Actions  RotF:%d RotS:%d ScaleU:%d ScaleD:%d",
            m_actions.IsHeld("RotFaster"), m_actions.IsHeld("RotSlower"),
            m_actions.IsHeld("ScaleUp"),   m_actions.IsHeld("ScaleDown"));
        ImGui::End();

        XMMATRIX model = XMMatrixScaling(m_scale, m_scale, m_scale)
                       * XMMatrixRotationY(m_rotAngle);
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

    float          m_scale    = 0.02f;
    float          m_rotSpeed = 0.4f;
    float          m_rotAngle = 0.0f;
    SE::ActionMap  m_actions;
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
