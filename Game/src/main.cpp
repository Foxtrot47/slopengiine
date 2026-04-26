#include <windows.h>
#include <DirectXMath.h>
#include <imgui.h>
#include "Engine/Core/Engine.h"
#include "Engine/Core/Logger.h"
#include "Engine/Assets/AssetManager.h"
#include "Engine/Physics/Plane.h"
#include "Engine/Physics/Ray.h"
#include "Engine/Physics/OBB.h"
#include "Engine/Physics/CharacterController.h"
#include "Engine/Scene/Scene.h"
#include "Engine/Scene/TransformComponent.h"
#include "Engine/Scene/Camera/CameraComponent.h"
#include "Engine/Scene/Camera/CameraController.h"
#include "Engine/Physics/RigidBodyComponent.h"
#include "Engine/Physics/PhysicsWorld.h"
#include "Engine/Renderer/LightEnvironment.h"
#include "Engine/Renderer/ForwardPipeline.h"
#include "Engine/Input/GamepadState.h"

using namespace DirectX;

class TestScene : public SE::Engine
{
public:
    bool Setup()
    {
        ID3D11Device* device = GetRenderer().GetDevice();

        if (!m_lights.Init(device))                return false;
        if (!m_pipeline.Init(device, GetAssets())) return false;

        m_mesh    = GetAssets().GetMesh("Assets/Sponza/Sponza.gltf");
        if (!m_mesh) return false;
        m_subMats = m_pipeline.LoadMeshMaterials(GetAssets(), *m_mesh);

        m_lights.numLights = 2;
        m_lights.lights[0] = { { 0.0f, 5.0f, 0.0f }, { 1.0f, 0.85f, 0.5f }, 18.0f };
        m_lights.lights[1] = { { 8.0f, 3.0f, 0.0f }, { 0.3f, 0.5f,  1.0f }, 10.0f };

        SE::Entity* camEnt       = m_scene.CreateEntity("Camera");
        m_camera                 = camEnt->AddComponent<SE::CameraComponent>();
        m_camera->farZ           = 5000.0f;
        m_camCtrl.orbit.target   = { 0.0f, 4.0f, 0.0f };
        m_camCtrl.orbit.distance = 22.0f;
        m_camCtrl.orbit.pitchDeg = -15.0f;
        m_camCtrl.Update(0.0f, GetInput(), *m_camera, GetWindow().GetHandle());

        SE::Entity* ballEnt        = m_scene.CreateEntity("PhysBall");
        m_ballTransform            = ballEnt->AddComponent<SE::TransformComponent>();
        m_ballRigidBody            = ballEnt->AddComponent<SE::RigidBodyComponent>();
        m_ballTransform->scale     = m_ballRadius;
        ResetBall();
        m_physicsWorld.AddSphere(m_ballTransform, m_ballRigidBody, m_ballRadius);
        m_physicsWorld.AddStaticPlane(
            SE::Plane::FromPointNormal({ 0.0f, m_floorY, 0.0f }, { 0.0f, 1.0f, 0.0f }),
            0.6f, 0.4f);

        // Test OBBs for M35/M36.
        m_obbA = SE::OBB::FromAABB({ 2.0f, 0.0f, -2.0f }, { 6.0f, 4.0f, 2.0f });
        m_obbB = SE::OBB::MakeRotatedY({ -5.0f, 2.0f, 3.0f }, { 3.0f, 2.0f, 1.0f }, 30.0f);
        m_obbC = SE::OBB::FromAABB({ -3.0f, 0.0f, -1.0f }, { -1.0f, 0.28f, 1.0f }); // step-up test
        m_physicsWorld.AddStaticOBB(m_obbA, 0.5f, 0.4f);
        m_physicsWorld.AddStaticOBB(m_obbB, 0.5f, 0.4f);
        m_physicsWorld.AddStaticOBB(m_obbC, 0.5f, 0.4f);

        // Character controller starts at eye level above scene centre.
        m_cc.position = { 0.0f, 5.0f, 0.0f };

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
        float dt     = GetClock().GetDeltaTime();
        float aspect = (float)GetWindow().GetWidth() / (float)GetWindow().GetHeight();

        bool          mouseBlocked = ImGui::GetIO().WantCaptureMouse;
        SE::CameraController::Mode prevMode = m_camCtrl.GetMode();

        m_camCtrl.Update(dt, GetInput(), *m_camera,
                         GetWindow().GetHandle(), mouseBlocked);

        SE::CameraController::Mode mode = m_camCtrl.GetMode();

        // Init character position when first entering FPS mode.
        if (prevMode == SE::CameraController::Mode::Orbit &&
            mode     == SE::CameraController::Mode::FPS)
        {
            m_cc.position = { m_camera->eye.x,
                              m_camera->eye.y - m_cc.eyeHeight,
                              m_camera->eye.z };
            m_cc.velY = 0.0f;
        }

        // Drive character + camera in FPS mode.
        if (mode == SE::CameraController::Mode::FPS)
        {
            auto& inp = GetInput();
            float yawRad = XMConvertToRadians(m_camCtrl.fps.yawDeg);
            XMFLOAT3 fwd   = { sinf(yawRad), 0.0f,  cosf(yawRad) };
            XMFLOAT3 right = { cosf(yawRad), 0.0f, -sinf(yawRad) };

            XMFLOAT3 wishVel = { 0.0f, 0.0f, 0.0f };
            if (inp.IsKeyDown('W')) { wishVel.x += fwd.x;   wishVel.z += fwd.z; }
            if (inp.IsKeyDown('S')) { wishVel.x -= fwd.x;   wishVel.z -= fwd.z; }
            if (inp.IsKeyDown('D')) { wishVel.x += right.x; wishVel.z += right.z; }
            if (inp.IsKeyDown('A')) { wishVel.x -= right.x; wishVel.z -= right.z; }

            // Normalise diagonal, then scale to moveSpeed.
            float len2 = wishVel.x * wishVel.x + wishVel.z * wishVel.z;
            if (len2 > 1.0f) {
                float inv = 1.0f / sqrtf(len2);
                wishVel.x *= inv; wishVel.z *= inv;
            }
            wishVel.x *= m_cc.moveSpeed;
            wishVel.z *= m_cc.moveSpeed;

            if (inp.IsKeyPressed(VK_SPACE)) m_cc.Jump();

            m_physicsWorld.StepCharacter(m_cc, wishVel, dt);

            // Sync camera to character eye.
            float pitchRad = XMConvertToRadians(m_camCtrl.fps.pitchDeg);
            XMFLOAT3 eye  = m_cc.GetEyePosition();
            XMFLOAT3 look = {
                sinf(yawRad) * cosf(pitchRad),
                sinf(pitchRad),
                cosf(yawRad) * cosf(pitchRad)
            };
            m_camera->eye    = eye;
            m_camera->target = { eye.x + look.x, eye.y + look.y, eye.z + look.z };
            m_camera->up     = { 0.0f, 1.0f, 0.0f };
        }

        m_scene.Update(dt);
        m_physicsWorld.Step(dt);

        XMMATRIX view = m_camera->GetViewMatrix();
        XMMATRIX proj = m_camera->GetProjectionMatrix(aspect);

        DrawUI(view, proj);

        m_lights.BindPS(ctx, m_camera->eye);
        m_pipeline.Begin(ctx, view, proj);
        m_pipeline.SetMaterialParams(ctx,
            { m_matTint[0], m_matTint[1], m_matTint[2] }, m_roughnessScale, m_metallic);
        m_pipeline.DrawMesh(ctx, *m_mesh, XMMatrixIdentity(), m_subMats);
        m_pipeline.DrawSphere(ctx, m_ballTransform->position, m_ballRadius, { 1.0f, 0.45f, 0.05f });

        if (m_castRay)
        {
            SE::Ray ray;
            XMVECTOR eye = XMLoadFloat3(&m_camera->eye);
            XMVECTOR dir = XMVector3Normalize(
                XMVectorSubtract(XMLoadFloat3(&m_camera->target), eye));
            XMStoreFloat3(&ray.origin,    eye);
            XMStoreFloat3(&ray.direction, dir);
            m_rayHitValid = m_physicsWorld.Raycast(ray, m_rayHit);
        }
        else
        {
            m_rayHitValid = false;
        }

        if (m_showColliders)
        {
            m_pipeline.DrawWireSphere(ctx, m_ballTransform->position, m_ballRadius,
                                      { 0.0f, 1.0f, 0.2f });
            m_pipeline.DrawWireDisc(ctx, { 0.0f, m_floorY, 0.0f }, 20.0f,
                                    { 1.0f, 0.85f, 0.1f });
            m_pipeline.DrawWireBox(ctx, m_obbA.GetWorldMatrix(), { 0.3f, 0.7f, 1.0f });
            m_pipeline.DrawWireBox(ctx, m_obbB.GetWorldMatrix(), { 0.3f, 0.7f, 1.0f });
            m_pipeline.DrawWireBox(ctx, m_obbC.GetWorldMatrix(), { 0.3f, 0.7f, 1.0f });

            // Character capsule bottom+top cap markers.
            if (m_camCtrl.GetMode() == SE::CameraController::Mode::Orbit)
            {
                XMFLOAT3 bottom = { m_cc.position.x, m_cc.position.y + m_cc.radius, m_cc.position.z };
                XMFLOAT3 top    = { m_cc.position.x, m_cc.position.y + m_cc.height - m_cc.radius, m_cc.position.z };
                m_pipeline.DrawWireSphere(ctx, bottom, m_cc.radius, { 1.0f, 0.4f, 0.8f });
                m_pipeline.DrawWireSphere(ctx, top,    m_cc.radius, { 1.0f, 0.4f, 0.8f });
            }
        }

        if (m_rayHitValid)
        {
            m_pipeline.DrawWireSphere(ctx, m_rayHit.point, 0.15f, { 1.0f, 1.0f, 1.0f });
            m_pipeline.DrawLine(ctx, m_rayHit.point,
                { m_rayHit.point.x + m_rayHit.normal.x,
                  m_rayHit.point.y + m_rayHit.normal.y,
                  m_rayHit.point.z + m_rayHit.normal.z },
                { 1.0f, 1.0f, 0.0f });
        }
    }

private:
    void DrawUI(XMMATRIX view, XMMATRIX proj)
    {
        ImGui::Begin("Scene");
        ImGui::Text("%.1f fps  |  %.2f ms",
                    GetClock().GetFPS(), GetClock().GetDeltaTime() * 1000.0f);
        ImGui::Separator();

        ImGui::Text("Camera  [Tab to switch]");
        if (m_camCtrl.GetMode() == SE::CameraController::Mode::FPS)
        {
            ImGui::Text("  FPS — WASD move, RMB look, Space jump");
            ImGui::SliderFloat("Move Speed", &m_cc.moveSpeed,  1.0f, 20.0f);
            ImGui::SliderFloat("Jump Speed", &m_cc.jumpSpeed,  2.0f, 20.0f);
            ImGui::Text("  pos (%.1f, %.1f, %.1f)  vy=%.2f  %s",
                m_cc.position.x, m_cc.position.y, m_cc.position.z,
                m_cc.velY, m_cc.isGrounded ? "GROUNDED" : "air");
        }
        else
        {
            ImGui::Text("  Orbit — LMB drag, wheel zoom");
            ImGui::DragFloat3("Target",    &m_camCtrl.orbit.target.x,  0.1f);
            ImGui::SliderFloat("Distance", &m_camCtrl.orbit.distance,  1.0f, 500.0f);
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

        if (ImGui::CollapsingHeader("Physics", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Checkbox("Show Colliders", &m_showColliders);
            ImGui::SameLine();
            ImGui::Checkbox("Raycast", &m_castRay);

            if (ImGui::Checkbox("Gravity", &m_gravityEnabled))
            {
                m_ballRigidBody->useGravity = m_gravityEnabled;
                m_cc.gravityEnabled         = m_gravityEnabled;
            }
            ImGui::DragFloat3("Spawn pos",    &m_ballSpawn.x,                1.0f);
            ImGui::SliderFloat("Radius",      &m_ballRadius,                 0.1f, 10.0f);
            ImGui::SliderFloat("Mass",        &m_ballRigidBody->mass,        0.1f, 10.0f);
            ImGui::SliderFloat("Restitution", &m_ballRigidBody->restitution, 0.0f, 1.0f);
            ImGui::SliderFloat("Friction",    &m_ballRigidBody->friction,    0.0f, 1.0f);
            if (ImGui::SliderFloat("Floor Y", &m_floorY, -50.0f, 50.0f))
            {
                m_physicsWorld.Clear();
                m_physicsWorld.AddSphere(m_ballTransform, m_ballRigidBody, m_ballRadius);
                m_physicsWorld.AddStaticPlane(
                    SE::Plane::FromPointNormal({ 0.0f, m_floorY, 0.0f }, { 0.0f, 1.0f, 0.0f }),
                    0.6f, 0.4f);
                m_physicsWorld.AddStaticOBB(m_obbA, 0.5f, 0.4f);
                m_physicsWorld.AddStaticOBB(m_obbB, 0.5f, 0.4f);
                m_physicsWorld.AddStaticOBB(m_obbC, 0.5f, 0.4f);
            }
            ImGui::Text("  pos (%.1f, %.1f, %.1f)",
                m_ballTransform->position.x, m_ballTransform->position.y, m_ballTransform->position.z);
            ImGui::Text("  vel (%.2f, %.2f, %.2f)",
                m_ballRigidBody->velocity.x, m_ballRigidBody->velocity.y, m_ballRigidBody->velocity.z);
            if (ImGui::Button("Reset"))     ResetBall();
            ImGui::SameLine();
            if (ImGui::Button("Launch up")) m_ballRigidBody->AddImpulse({ 0.0f, 20.0f, 0.0f });

            if (m_castRay)
            {
                ImGui::Spacing();
                if (m_rayHitValid)
                {
                    const char* what =
                        m_rayHit.kind == SE::PhysicsWorld::RaycastHit::Kind::Sphere ? "Ball" :
                        m_rayHit.kind == SE::PhysicsWorld::RaycastHit::Kind::OBB    ? "OBB"  : "Floor";
                    ImGui::Text("  Hit: %s  t=%.2f", what, m_rayHit.t);
                    ImGui::Text("  pos  (%.1f, %.1f, %.1f)",
                        m_rayHit.point.x,  m_rayHit.point.y,  m_rayHit.point.z);
                    ImGui::Text("  norm (%.2f, %.2f, %.2f)",
                        m_rayHit.normal.x, m_rayHit.normal.y, m_rayHit.normal.z);
                }
                else
                {
                    ImGui::TextDisabled("  no hit");
                }
            }
        }

        ImGui::Text("Lighting");
        ImGui::SliderFloat("Elevation",    &m_lights.elevDeg,  -90.0f, 90.0f,  "%.1f deg");
        ImGui::SliderFloat("Azimuth",      &m_lights.azimDeg, -180.0f, 180.0f, "%.1f deg");
        ImGui::ColorEdit3("Light Color",   m_lights.lightColor);
        ImGui::ColorEdit3("Ambient Color", m_lights.ambientColor);
        ImGui::SliderFloat("Shininess",    &m_lights.shininess, 1.0f, 256.0f, "%.0f");
        ImGui::Separator();
        ImGui::SliderInt("Active point lights", &m_lights.numLights, 0, 8);
        for (int i = 0; i < m_lights.numLights; ++i)
        {
            ImGui::PushID(i);
            char label[24];
            sprintf_s(label, "Point Light %d", i + 1);
            if (ImGui::CollapsingHeader(label))
            {
                ImGui::DragFloat3("Position", &m_lights.lights[i].position.x, 0.1f, -30.0f, 30.0f);
                ImGui::ColorEdit3("Color",    &m_lights.lights[i].color.x);
                ImGui::SliderFloat("Radius",  &m_lights.lights[i].radius, 0.5f, 50.0f);
            }
            ImGui::PopID();
        }
        ImGui::Separator();

        const SE::GamepadState& gp = GetInput().GetGamepad(0);
        if (gp.connected)
            ImGui::Text("Pad0  L(%.2f,%.2f) R(%.2f,%.2f) LT:%.2f RT:%.2f",
                gp.leftX, gp.leftY, gp.rightX, gp.rightY, gp.leftTrigger, gp.rightTrigger);
        else
            ImGui::TextDisabled("Pad0  not connected");
        ImGui::End();

        // Viewport light indicators
        {
            XMMATRIX    vp = XMMatrixMultiply(view, proj);
            float       sw = (float)GetWindow().GetWidth();
            float       sh = (float)GetWindow().GetHeight();
            ImDrawList* dl = ImGui::GetBackgroundDrawList();

            auto project = [&](XMVECTOR wp, float& sx, float& sy) -> bool
            {
                XMVECTOR cp = XMVector4Transform(wp, vp);
                float    cw = XMVectorGetW(cp);
                if (cw <= 0.0f) return false;
                sx = ( XMVectorGetX(cp) / cw * 0.5f + 0.5f) * sw;
                sy = (-XMVectorGetY(cp) / cw * 0.5f + 0.5f) * sh;
                return true;
            };

            // Sun
            {
                float er = XMConvertToRadians(m_lights.elevDeg);
                float ar = XMConvertToRadians(m_lights.azimDeg);
                XMVECTOR dir    = XMVectorSet(cosf(er) * sinf(ar), sinf(er), cosf(er) * cosf(ar), 0.0f);
                XMVECTOR eyeV   = XMVectorSet(m_camera->eye.x, m_camera->eye.y, m_camera->eye.z, 1.0f);
                XMVECTOR sunPos = XMVectorSetW(XMVectorAdd(eyeV, XMVectorScale(dir, 80.0f)), 1.0f);
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

            // Point lights
            for (int i = 0; i < m_lights.numLights; ++i)
            {
                const auto& pl = m_lights.lights[i];
                float sx, sy;
                if (!project(XMVectorSet(pl.position.x, pl.position.y, pl.position.z, 1.0f), sx, sy))
                    continue;
                ImU32 fill = ImGui::ColorConvertFloat4ToU32({ pl.color.x, pl.color.y, pl.color.z, 1.0f });
                dl->AddCircleFilled({ sx, sy }, 7.0f, fill);
                dl->AddCircle({ sx, sy }, 8.0f, IM_COL32(255, 255, 255, 200), 0, 1.5f);
                char tag[8];
                sprintf_s(tag, "PL%d", i + 1);
                dl->AddText({ sx + 11.0f, sy - 7.0f }, IM_COL32_WHITE, tag);
            }
        }
    }

    SE::LightEnvironment                     m_lights;
    SE::ForwardPipeline                      m_pipeline;
    SE::AssetHandle<SE::Mesh>                m_mesh;
    std::vector<SE::ForwardPipeline::SubMat> m_subMats;

    SE::Scene            m_scene;
    SE::CameraComponent* m_camera  = nullptr;
    SE::CameraController m_camCtrl;

    float m_matTint[3]     = { 1.0f, 1.0f, 1.0f };
    float m_roughnessScale = 1.0f;
    float m_metallic       = 0.0f;

    SE::PhysicsWorld        m_physicsWorld;
    SE::TransformComponent* m_ballTransform = nullptr;
    SE::RigidBodyComponent* m_ballRigidBody = nullptr;
    XMFLOAT3                m_ballSpawn     = { 0.0f, 30.0f, 0.0f };
    float                   m_ballRadius    = 1.0f;
    float                   m_floorY        = 0.0f;
    SE::OBB                 m_obbA;
    SE::OBB                 m_obbB;
    SE::OBB                 m_obbC; // low step for step-up test
    SE::CharacterController m_cc;

    bool                         m_gravityEnabled = true;
    bool                         m_showColliders  = true;
    bool                         m_castRay        = false;
    SE::PhysicsWorld::RaycastHit m_rayHit        = {};
    bool                         m_rayHitValid   = false;
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
