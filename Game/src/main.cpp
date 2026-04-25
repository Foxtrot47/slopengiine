#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <DirectXMath.h>
#include <imgui.h>
#include <string>
#include <vector>
#include "Engine/Core/Engine.h"
#include "Engine/Core/Logger.h"
#include "Engine/Renderer/Mesh.h"
#include "Engine/Renderer/VertexBuffer.h"
#include "Engine/Renderer/IndexBuffer.h"
#include "Engine/Renderer/ConstantBuffer.h"
#include "Engine/Renderer/Texture2D.h"
#include "Engine/Renderer/SamplerState.h"
#include "Engine/Input/GamepadState.h"
#include "Engine/Assets/AssetManager.h"
#include "Engine/Physics/AABB.h"
#include "Engine/Physics/Sphere.h"
#include "Engine/Physics/Ray.h"
#include "Engine/Physics/Plane.h"
#include "Engine/Physics/Intersect.h"
#include "Engine/Scene/Scene.h"
#include "Engine/Scene/TransformComponent.h"
#include "Engine/Scene/CameraComponent.h"
#include "Engine/Scene/ArcballController.h"
#include "Engine/Scene/FPSController.h"
#include "Engine/Physics/RigidBodyComponent.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;

static void BuildSphereMesh(float radius, int rings, int segs,
    std::vector<SE::MeshVertex>& verts, std::vector<uint32_t>& indices)
{
    const float pi = XM_PI;
    for (int r = 0; r <= rings; ++r)
    {
        float phi = -pi * 0.5f + pi * r / rings;
        for (int s = 0; s <= segs; ++s)
        {
            float theta = XM_2PI * s / segs;
            float cp = cosf(phi), sp = sinf(phi);
            float ct = cosf(theta), st = sinf(theta);

            SE::MeshVertex v;
            v.x  = cp * ct * radius;
            v.y  = sp      * radius;
            v.z  = cp * st * radius;
            v.nx = cp * ct; v.ny = sp; v.nz = cp * st;
            v.u  = (float)s / segs;
            v.v  = (float)r / rings;
            v.tx = -st;      v.ty = 0.0f; v.tz = ct;   // d/dtheta normalised
            v.bx = -sp * ct; v.by = cp;   v.bz = -sp * st; // d/dphi normalised
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

struct PointLightData
{
    XMFLOAT3 position; float radius;
    XMFLOAT3 color;    float _pad;
};

struct PointLightCB
{
    PointLightData lights[8];
    int            numLights;
    XMFLOAT3       _pad;
};

struct MaterialParamsCB
{
    XMFLOAT3 albedoTint;  float roughnessScale;
    float    metallic;    float _pad[3];
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

        if (!m_transformCB.Create(device))   return false;
        if (!m_lightCB.Create(device))       return false;
        if (!m_pointLightCB.Create(device))  return false;
        if (!m_materialCB.Create(device))    return false;

        // ---- Anisotropic sampler ----
        if (!m_sampler.Create(device, { SE::FilterMode::Anisotropic, SE::AddressMode::Wrap }))
            return false;

        // ---- Load Sponza ----
        m_mesh = GetAssets().GetMesh("Assets/Sponza/Sponza.gltf");
        if (!m_mesh) return false;

        // Build per-submesh material handles; fall back to default 1×1 textures.
        const std::string& dir = m_mesh->GetDirectory();
        auto toWide = [](const std::string& s) -> std::wstring {
            return std::wstring(s.begin(), s.end());
        };

        m_subMats.resize(m_mesh->GetSubMeshCount());
        for (uint32_t i = 0; i < m_mesh->GetSubMeshCount(); ++i)
        {
            SE::SubMeshInfo info = m_mesh->GetSubMeshInfo(i);
            SubMat& mat = m_subMats[i];

            if (!info.albedoPath.empty())
                mat.albedo = GetAssets().GetTexture(toWide(dir + info.albedoPath));
            if (!mat.albedo)
                mat.albedo = GetAssets().GetDefaultWhite();

            if (!info.normalPath.empty())
                mat.normal = GetAssets().GetTexture(toWide(dir + info.normalPath));
            if (!mat.normal)
                mat.normal = GetAssets().GetDefaultNormal();

            if (!info.roughnessPath.empty())
                mat.roughness = GetAssets().GetTexture(toWide(dir + info.roughnessPath));
            if (!mat.roughness)
                mat.roughness = GetAssets().GetDefaultWhite();
        }

        // ---- Point lights — Sponza atrium ----
        m_pointLights[0].position[0] =  0.0f; m_pointLights[0].position[1] = 5.0f; m_pointLights[0].position[2] = 0.0f;
        m_pointLights[0].color[0] = 1.0f; m_pointLights[0].color[1] = 0.85f; m_pointLights[0].color[2] = 0.5f;
        m_pointLights[0].radius = 18.0f;

        m_pointLights[1].position[0] = 8.0f; m_pointLights[1].position[1] = 3.0f; m_pointLights[1].position[2] = 0.0f;
        m_pointLights[1].color[0] = 0.3f; m_pointLights[1].color[1] = 0.5f; m_pointLights[1].color[2] = 1.0f;
        m_pointLights[1].radius = 10.0f;

        // ---- Camera — orbiting Sponza atrium ----
        SE::Entity* camEntity = m_scene.CreateEntity("Camera");
        m_camera = camEntity->AddComponent<SE::CameraComponent>();
        m_camera->farZ = 5000.0f;

        m_arcball.target   = { 0.0f, 4.0f, 0.0f };
        m_arcball.distance = 22.0f;
        m_arcball.pitchDeg = -15.0f;
        m_arcball.Update(GetInput(), *m_camera);

        // ---- Fallback textures cached for debug geometry ----
        m_defaultWhite  = GetAssets().GetDefaultWhite();
        m_defaultNormal = GetAssets().GetDefaultNormal();

        // ---- Debug sphere mesh ----
        {
            std::vector<SE::MeshVertex> verts;
            std::vector<uint32_t>       idx;
            BuildSphereMesh(1.0f, 16, 16, verts, idx);
            m_sphereVB.Create(device, verts.data(),
                static_cast<uint32_t>(verts.size() * sizeof(SE::MeshVertex)),
                sizeof(SE::MeshVertex));
            m_sphereIB.Create(device, idx.data(),
                static_cast<uint32_t>(idx.size()));
        }

        // ---- Physics ball (M32 demo) ----
        SE::Entity* ballEntity = m_scene.CreateEntity("PhysBall");
        m_ballTransform  = ballEntity->AddComponent<SE::TransformComponent>();
        m_ballRigidBody  = ballEntity->AddComponent<SE::RigidBodyComponent>();
        m_ballTransform->scale = m_ballRadius;
        ResetBall();

        SE_LOG_INFO("TestScene ready — Sponza (%u submeshes)", m_mesh->GetSubMeshCount());
        return true;
    }

    void ResetBall()
    {
        m_ballTransform->position = m_ballSpawn;
        m_ballRigidBody->velocity = { 0.0f, 0.0f, 0.0f };
    }

protected:
    void OnUpdate() override
    {
        ID3D11DeviceContext* ctx = GetRenderer().GetContext();
        float dt = GetClock().GetDeltaTime();

        // ---- Tab: toggle camera mode ----
        if (GetInput().IsKeyPressed(VK_TAB))
        {
            m_fpsMode = !m_fpsMode;
            if (m_fpsMode)
            {
                m_fps.position = m_camera->eye;
                m_fps.yawDeg   = m_arcball.yawDeg;
                m_fps.pitchDeg = m_arcball.pitchDeg;
            }
        }

        bool imguiMouse = ImGui::GetIO().WantCaptureMouse;
        if (m_fpsMode)
            m_fps.Update(dt, GetInput(), *m_camera, GetWindow().GetHandle(), imguiMouse);
        else
            m_arcball.Update(GetInput(), *m_camera, imguiMouse);

        m_scene.Update(dt);

        // ---- Camera matrices ----
        XMMATRIX view  = m_camera->GetViewMatrix();
        float    aspect = static_cast<float>(GetWindow().GetWidth()) /
                          static_cast<float>(GetWindow().GetHeight());
        XMMATRIX proj  = m_camera->GetProjectionMatrix(aspect);

        // ---- Debug panel ----
        ImGui::Begin("Scene");
        ImGui::Text("%.1f fps  |  %.2f ms",
                    GetClock().GetFPS(), GetClock().GetDeltaTime() * 1000.0f);
        ImGui::Separator();
        ImGui::Text("Camera  [Tab to switch]");
        if (m_fpsMode)
        {
            ImGui::Text("  FPS — WASD move, RMB look");
            ImGui::SliderFloat("Move Speed",  &m_fps.moveSpeed,   1.0f, 40.0f);
        }
        else
        {
            ImGui::Text("  Arcball — LMB drag, wheel zoom");
            ImGui::DragFloat3("Target",   &m_arcball.target.x,   0.1f);
            ImGui::SliderFloat("Distance", &m_arcball.distance,   1.0f, 500.0f);
        }
        ImGui::SliderFloat("Far Z", &m_camera->farZ, 100.0f, 20000.0f, "%.0f");
        ImGui::Text("  Eye (%.1f, %.1f, %.1f)",
            m_camera->eye.x, m_camera->eye.y, m_camera->eye.z);
        ImGui::Separator();
        ImGui::Text("Assets  meshes:%u  textures:%u",
            GetAssets().CachedMeshCount(), GetAssets().CachedTextureCount());
        ImGui::Text("Submeshes: %u", m_mesh ? m_mesh->GetSubMeshCount() : 0);
        ImGui::Separator();
        ImGui::Text("Material (global)");
        ImGui::ColorEdit3("Albedo Tint",      m_matTint);
        ImGui::SliderFloat("Roughness Scale", &m_roughnessScale, 0.0f, 2.0f);
        ImGui::SliderFloat("Metallic",        &m_metallic,       0.0f, 1.0f);
        ImGui::Separator();
        ImGui::Text("Physics — Rigidbody (M32)");
        ImGui::DragFloat3("Spawn pos",    &m_ballSpawn.x,         1.0f);
        ImGui::SliderFloat("Radius",      &m_ballRadius,          0.1f, 10.0f);
        ImGui::Checkbox("Gravity",        &m_ballRigidBody->useGravity);
        ImGui::SliderFloat("Mass",        &m_ballRigidBody->mass, 0.1f, 10.0f);
        ImGui::Text("  pos  (%.1f, %.1f, %.1f)",
            m_ballTransform->position.x,
            m_ballTransform->position.y,
            m_ballTransform->position.z);
        ImGui::Text("  vel  (%.2f, %.2f, %.2f)",
            m_ballRigidBody->velocity.x,
            m_ballRigidBody->velocity.y,
            m_ballRigidBody->velocity.z);
        if (ImGui::Button("Reset"))  ResetBall();
        ImGui::SameLine();
        if (ImGui::Button("Launch up"))
            m_ballRigidBody->AddImpulse({ 0.0f, 20.0f, 0.0f });
        ImGui::Separator();
        ImGui::Text("Intersection tests");
        ImGui::DragFloat3("Sphere A pos", &m_sphereA.center.x, 0.05f, -20.0f, 20.0f);
        ImGui::DragFloat ("Sphere A r",   &m_sphereA.radius,   0.05f,  0.1f,  5.0f);
        ImGui::DragFloat3("Sphere B pos", &m_sphereB.center.x, 0.05f, -20.0f, 20.0f);
        ImGui::DragFloat ("Sphere B r",   &m_sphereB.radius,   0.05f,  0.1f,  5.0f);
        ImGui::Text("  A ∩ B: %s", SE::Intersects(m_sphereA, m_sphereB) ? "OVERLAP" : "clear");
        {
            float    physAspect = static_cast<float>(GetWindow().GetWidth()) /
                                   static_cast<float>(GetWindow().GetHeight());
            XMMATRIX vp    = XMMatrixMultiply(m_camera->GetViewMatrix(),
                                              m_camera->GetProjectionMatrix(physAspect));
            XMMATRIX invVP = XMMatrixInverse(nullptr, vp);
            SE::Ray   ray  = SE::Ray::FromNDC(0.0f, 0.0f, invVP);
            float     t    = 0.0f;
            bool      hit  = SE::Intersects(ray, m_sponzaAABB, t);
            ImGui::Text("  Crosshair->scene AABB: %s%s", hit ? "HIT  t=" : "miss",
                        hit ? std::to_string(t).c_str() : "");
        }
        ImGui::Separator();
        ImGui::Text("Lighting");
        ImGui::SliderFloat("Elevation", &m_lightElev,  -90.0f, 90.0f,  "%.1f deg");
        ImGui::SliderFloat("Azimuth",   &m_lightAzim, -180.0f, 180.0f, "%.1f deg");
        ImGui::ColorEdit3("Light Color",   m_lightColor);
        ImGui::ColorEdit3("Ambient Color", m_ambientColor);
        ImGui::SliderFloat("Shininess", &m_shininess, 1.0f, 256.0f, "%.0f");
        ImGui::Separator();
        ImGui::SliderInt("Active point lights", &m_numPointLights, 0, 8);
        for (int i = 0; i < m_numPointLights; ++i)
        {
            ImGui::PushID(i);
            char label[24];
            sprintf_s(label, "Point Light %d", i + 1);
            if (ImGui::CollapsingHeader(label))
            {
                ImGui::DragFloat3("Position", m_pointLights[i].position, 0.1f, -30.0f, 30.0f);
                ImGui::ColorEdit3("Color",    m_pointLights[i].color);
                ImGui::SliderFloat("Radius",  &m_pointLights[i].radius, 0.5f, 50.0f);
            }
            ImGui::PopID();
        }
        const SE::GamepadState& gp = GetInput().GetGamepad(0);
        if (gp.connected)
            ImGui::Text("Pad0  L(%.2f,%.2f) R(%.2f,%.2f) LT:%.2f RT:%.2f",
                gp.leftX, gp.leftY, gp.rightX, gp.rightY,
                gp.leftTrigger, gp.rightTrigger);
        else
            ImGui::TextDisabled("Pad0  not connected");
        ImGui::End();

        // ---- Light viewport indicators ----
        {
            XMMATRIX viewProj = XMMatrixMultiply(view, proj);
            float sw = static_cast<float>(GetWindow().GetWidth());
            float sh = static_cast<float>(GetWindow().GetHeight());
            ImDrawList* dl = ImGui::GetBackgroundDrawList();

            auto project = [&](XMVECTOR wp, float& sx, float& sy) -> bool
            {
                XMVECTOR cp = XMVector4Transform(wp, viewProj);
                float    cw = XMVectorGetW(cp);
                if (cw <= 0.0f) return false;
                sx = ( XMVectorGetX(cp) / cw * 0.5f + 0.5f) * sw;
                sy = (-XMVectorGetY(cp) / cw * 0.5f + 0.5f) * sh;
                return true;
            };

            {
                float er = XMConvertToRadians(m_lightElev);
                float ar = XMConvertToRadians(m_lightAzim);
                XMVECTOR dir = XMVectorSet(
                    cosf(er) * sinf(ar), sinf(er), cosf(er) * cosf(ar), 0.0f);
                XMVECTOR eyeV = XMVectorSet(
                    m_camera->eye.x, m_camera->eye.y, m_camera->eye.z, 1.0f);
                XMVECTOR sunPos = XMVectorSetW(
                    XMVectorAdd(eyeV, XMVectorScale(dir, 80.0f)), 1.0f);

                float sx, sy;
                if (project(sunPos, sx, sy))
                {
                    dl->AddCircleFilled({ sx, sy }, 10.0f, IM_COL32(255, 220, 50, 255));
                    for (int r = 0; r < 8; ++r)
                    {
                        float a = r * 3.14159f / 4.0f;
                        dl->AddLine({ sx + cosf(a) * 12.0f, sy + sinf(a) * 12.0f },
                                    { sx + cosf(a) * 17.0f, sy + sinf(a) * 17.0f },
                                    IM_COL32(255, 220, 50, 200), 1.5f);
                    }
                    dl->AddText({ sx + 14.0f, sy - 7.0f }, IM_COL32_WHITE, "Sun");
                }
            }

            for (int i = 0; i < m_numPointLights; ++i)
            {
                const auto& s = m_pointLights[i];
                float sx, sy;
                if (!project(XMVectorSet(s.position[0], s.position[1], s.position[2], 1.0f), sx, sy))
                    continue;
                ImU32 fill = ImGui::ColorConvertFloat4ToU32(
                    { s.color[0], s.color[1], s.color[2], 1.0f });
                dl->AddCircleFilled({ sx, sy }, 7.0f, fill);
                dl->AddCircle({ sx, sy }, 8.0f, IM_COL32(255, 255, 255, 200), 0, 1.5f);
                char tag[8];
                sprintf_s(tag, "PL%d", i + 1);
                dl->AddText({ sx + 11.0f, sy - 7.0f }, IM_COL32_WHITE, tag);
            }
        }

        // ---- Bind shared constant buffers + pipeline state ----
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
            lc.cameraPos    = m_camera->eye;
            lc._pad2        = 0.0f;
            m_lightCB.Update(ctx, lc);
            m_lightCB.BindPS(ctx, 1);
        }
        {
            PointLightCB pl = {};
            pl.numLights = m_numPointLights;
            for (int i = 0; i < m_numPointLights; ++i)
            {
                const auto& s = m_pointLights[i];
                pl.lights[i].position = { s.position[0], s.position[1], s.position[2] };
                pl.lights[i].radius   = s.radius;
                pl.lights[i].color    = { s.color[0], s.color[1], s.color[2] };
            }
            m_pointLightCB.Update(ctx, pl);
            m_pointLightCB.BindPS(ctx, 2);
        }
        {
            MaterialParamsCB mc;
            mc.albedoTint     = { m_matTint[0], m_matTint[1], m_matTint[2] };
            mc.roughnessScale = m_roughnessScale;
            mc.metallic       = m_metallic;
            mc._pad[0] = mc._pad[1] = mc._pad[2] = 0.0f;
            m_materialCB.Update(ctx, mc);
            m_materialCB.BindPS(ctx, 3);
        }

        m_sampler.BindPS(ctx, 0);
        ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        ctx->IASetInputLayout(m_layout.Get());
        ctx->VSSetShader(m_vs.Get(), nullptr, 0);
        ctx->PSSetShader(m_ps.Get(), nullptr, 0);

        // ---- Draw Sponza: identity world transform, per-submesh textures ----
        TransformCB cb;
        XMStoreFloat4x4(&cb.model,      XMMatrixIdentity());
        XMStoreFloat4x4(&cb.view,       view);
        XMStoreFloat4x4(&cb.projection, proj);
        m_transformCB.Update(ctx, cb);
        m_transformCB.BindVS(ctx, 0);

        for (uint32_t i = 0; i < m_mesh->GetSubMeshCount(); ++i)
        {
            m_subMats[i].albedo->BindPS(ctx, 0);
            m_subMats[i].roughness->BindPS(ctx, 1);
            m_subMats[i].normal->BindPS(ctx, 2);
            m_mesh->DrawSubMesh(ctx, i);
        }

        // ---- Draw physics ball ----
        {
            const auto& p = m_ballTransform->position;
            XMStoreFloat4x4(&cb.model,
                XMMatrixScaling(m_ballRadius, m_ballRadius, m_ballRadius) *
                XMMatrixTranslation(p.x, p.y, p.z));
            m_transformCB.Update(ctx, cb);
            m_transformCB.BindVS(ctx, 0);

            MaterialParamsCB bmc;
            bmc.albedoTint     = { 1.0f, 0.45f, 0.05f }; // orange
            bmc.roughnessScale = 0.7f;
            bmc.metallic       = 0.0f;
            bmc._pad[0] = bmc._pad[1] = bmc._pad[2] = 0.0f;
            m_materialCB.Update(ctx, bmc);
            m_materialCB.BindPS(ctx, 3);

            m_defaultWhite->BindPS(ctx, 0);
            m_defaultWhite->BindPS(ctx, 1);
            m_defaultNormal->BindPS(ctx, 2);

            m_sphereVB.Bind(ctx);
            m_sphereIB.Bind(ctx);
            ctx->DrawIndexed(m_sphereIB.GetCount(), 0, 0);
        }
    }

private:
    ComPtr<ID3D11VertexShader>      m_vs;
    ComPtr<ID3D11PixelShader>       m_ps;
    ComPtr<ID3D11InputLayout>       m_layout;
    SE::AssetHandle<SE::Mesh>       m_mesh;
    SE::ConstantBuffer<TransformCB>      m_transformCB;
    SE::ConstantBuffer<LightCB>          m_lightCB;
    SE::ConstantBuffer<PointLightCB>     m_pointLightCB;
    SE::ConstantBuffer<MaterialParamsCB> m_materialCB;
    SE::SamplerState                     m_sampler;
    SE::VertexBuffer                     m_sphereVB;
    SE::IndexBuffer                      m_sphereIB;
    SE::AssetHandle<SE::Texture2D>       m_defaultWhite;
    SE::AssetHandle<SE::Texture2D>       m_defaultNormal;

    struct SubMat
    {
        SE::AssetHandle<SE::Texture2D> albedo;
        SE::AssetHandle<SE::Texture2D> normal;
        SE::AssetHandle<SE::Texture2D> roughness;
    };
    std::vector<SubMat> m_subMats;

    SE::Scene            m_scene;
    SE::CameraComponent* m_camera  = nullptr;
    SE::ArcballController m_arcball;
    SE::FPSController     m_fps;
    bool                  m_fpsMode = false;

    float m_lightElev    =  40.0f;
    float m_lightAzim    =  30.0f;
    float m_shininess    =  64.0f;
    float m_lightColor[3]   = { 1.0f, 0.95f, 0.85f };
    float m_ambientColor[3] = { 0.06f, 0.06f, 0.08f };

    float m_matTint[3]      = { 1.0f, 1.0f, 1.0f };
    float m_roughnessScale  = 1.0f;
    float m_metallic        = 0.0f;

    struct PointLightSettings
    {
        float position[3]   = { 0.0f, 2.0f, 0.0f };
        float color[3]      = { 1.0f, 1.0f, 1.0f };
        float radius        = 10.0f;
    };

    int                m_numPointLights = 2;
    PointLightSettings m_pointLights[8];

    // Intersection test primitives (M29)
    SE::AABB   m_sponzaAABB = SE::AABB::FromCenterExtents({ 0,4,0 }, { 14.0f, 4.0f, 5.0f });
    SE::Sphere m_sphereA    = { { -2.0f, 2.0f, 0.0f }, 1.0f };
    SE::Sphere m_sphereB    = { {  2.0f, 2.0f, 0.0f }, 1.0f };

    // Rigidbody demo (M32)
    SE::TransformComponent*  m_ballTransform = nullptr;
    SE::RigidBodyComponent*  m_ballRigidBody = nullptr;
    DirectX::XMFLOAT3        m_ballSpawn     = { 0.0f, 30.0f, 0.0f };
    float                    m_ballRadius    = 1.0f;
};

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    SE::WindowDesc desc;
    desc.title  = L"FoxEngine — Sponza";
    desc.width  = 1280;
    desc.height = 720;

    TestScene scene;
    if (!scene.Initialize(desc)) return 1;
    if (!scene.Setup())          return 1;

    scene.Run();
    scene.Shutdown();
    return 0;
}
