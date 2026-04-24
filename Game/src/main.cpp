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
#include "Engine/Input/GamepadState.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;

struct TransformCB
{
    XMFLOAT4X4 model;
    XMFLOAT4X4 view;
    XMFLOAT4X4 projection;
};

struct LightCB
{
    XMFLOAT3 lightDir;    float shininess;
    XMFLOAT3 lightColor;  float _pad0;
    XMFLOAT3 ambientColor;float _pad1;
    XMFLOAT3 cameraPos;   float _pad2;
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
        if (!m_lightCB.Create(device))     return false;

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

        m_actions.BindGamepad("RotFaster", SE::GamepadButton::DpadRight);
        m_actions.BindGamepad("RotFaster", SE::GamepadButton::RightShoulder);
        m_actions.BindGamepad("RotSlower", SE::GamepadButton::DpadLeft);
        m_actions.BindGamepad("RotSlower", SE::GamepadButton::LeftShoulder);
        m_actions.BindGamepad("ScaleUp",   SE::GamepadButton::DpadUp);
        m_actions.BindGamepad("ScaleDown", SE::GamepadButton::DpadDown);

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

        // ---- Analog gamepad axes ----
        const SE::GamepadState& gp = GetInput().GetGamepad(0);
        if (gp.connected)
        {
            // Left stick X: direct rotation speed control
            if (fabsf(gp.leftX) > 0.0f)
                m_rotSpeed = max(0.0f, min(3.0f, m_rotSpeed + gp.leftX * dt * 3.0f));
            // Triggers: scale up/down
            m_scale = max(0.001f, min(0.1f, m_scale + (gp.rightTrigger - gp.leftTrigger) * dt * 0.05f));
        }

        // ---- Debug panel ----
        ImGui::Begin("Scene");
        ImGui::Text("%.1f fps  |  %.2f ms",
                    GetClock().GetFPS(), GetClock().GetDeltaTime() * 1000.0f);
        ImGui::Separator();
        ImGui::SliderFloat("Scale",     &m_scale,    0.001f, 0.1f, "%.4f");
        ImGui::SliderFloat("Rot Speed", &m_rotSpeed, 0.0f,   3.0f);
        ImGui::Separator();
        ImGui::Text("Lighting");
        ImGui::SliderFloat("Elevation", &m_lightElev,  -90.0f, 90.0f,  "%.1f deg");
        ImGui::SliderFloat("Azimuth",   &m_lightAzim, -180.0f, 180.0f, "%.1f deg");
        ImGui::ColorEdit3("Light Color",   m_lightColor);
        ImGui::ColorEdit3("Ambient Color", m_ambientColor);
        ImGui::SliderFloat("Shininess",    &m_shininess, 1.0f, 256.0f, "%.0f");
        if (gp.connected)
            ImGui::Text("Pad0  L(%.2f,%.2f) R(%.2f,%.2f) LT:%.2f RT:%.2f",
                gp.leftX, gp.leftY, gp.rightX, gp.rightY,
                gp.leftTrigger, gp.rightTrigger);
        else
            ImGui::TextDisabled("Pad0  not connected");
        ImGui::End();

        XMMATRIX model = XMMatrixScaling(m_scale, m_scale, m_scale)
                       * XMMatrixRotationY(m_rotAngle);
        XMMATRIX view  = XMMatrixLookAtLH(
            XMVectorSet(0.0f, 2.0f, -10.0f, 1.0f),
            XMVectorSet(0.0f, 2.0f,   0.0f, 1.0f),
            XMVectorSet(0.0f, 1.0f,   0.0f, 0.0f));
        float aspect   = static_cast<float>(GetWindow().GetWidth()) /
                         static_cast<float>(GetWindow().GetHeight());
        XMMATRIX proj  = XMMatrixPerspectiveFovLH(
            XMConvertToRadians(60.0f), aspect, 0.1f, 100.0f);

        TransformCB cb;
        XMStoreFloat4x4(&cb.model,      model);
        XMStoreFloat4x4(&cb.view,       view);
        XMStoreFloat4x4(&cb.projection, proj);
        m_transformCB.Update(ctx, cb);
        m_transformCB.BindVS(ctx, 0);

        // ---- Light constant buffer ----
        {
            float elevRad = XMConvertToRadians(m_lightElev);
            float azimRad = XMConvertToRadians(m_lightAzim);
            LightCB lc;
            lc.lightDir     = { cosf(elevRad) * sinf(azimRad), sinf(elevRad), cosf(elevRad) * cosf(azimRad) };
            lc.shininess    = m_shininess;
            lc.lightColor   = { m_lightColor[0],   m_lightColor[1],   m_lightColor[2] };
            lc._pad0        = 0.0f;
            lc.ambientColor = { m_ambientColor[0], m_ambientColor[1], m_ambientColor[2] };
            lc._pad1        = 0.0f;
            lc.cameraPos    = { 0.0f, 2.0f, -10.0f };
            lc._pad2        = 0.0f;
            m_lightCB.Update(ctx, lc);
            m_lightCB.BindPS(ctx, 1);
        }

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
    SE::ConstantBuffer<LightCB>     m_lightCB;
    SE::Texture2D                   m_texture;
    SE::SamplerState                m_sampler;

    float          m_scale    = 0.02f;
    float          m_rotSpeed = 0.4f;
    float          m_rotAngle = 0.0f;
    SE::ActionMap  m_actions;

    float m_lightElev    =  35.0f;
    float m_lightAzim    =  45.0f;
    float m_shininess    =  64.0f;
    float m_lightColor[3]   = { 1.0f, 0.95f, 0.85f };
    float m_ambientColor[3] = { 0.08f, 0.08f, 0.12f };
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
