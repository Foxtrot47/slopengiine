#include <windows.h>
#include <DirectXMath.h>
#include <imgui.h>
#include <algorithm>
#include <filesystem>
#include "Engine/Core/Engine.h"
#include "Engine/Core/Logger.h"
#include "Engine/Assets/AssetManager.h"
#include "Engine/Physics/Plane.h"
#include "Engine/Physics/Ray.h"
#include "Engine/Physics/OBB.h"
#include "Engine/Physics/CharacterController.h"
#include "Engine/Scene/Scene.h"
#include "Engine/Scene/SceneDescriptor.h"
#include "Engine/Scene/SceneLoader.h"
#include "Engine/Scene/TransformComponent.h"
#include "Engine/Scene/Camera/CameraComponent.h"
#include "Engine/Scene/Camera/CameraController.h"
#include "Engine/Physics/RigidBodyComponent.h"
#include "Engine/Physics/PhysicsWorld.h"
#include "Engine/Renderer/LightEnvironment.h"
#include "Engine/Renderer/ForwardPipeline.h"
#include "Engine/Renderer/SkyboxRenderer.h"
#include "Engine/Renderer/ShadowMap.h"
#include "Engine/Renderer/RenderTarget.h"
#include "Engine/Renderer/ToneMap.h"
#include "Engine/Renderer/PointShadowMap.h"
#include "Engine/Renderer/Bloom.h"
#include "Engine/Input/GamepadState.h"

using namespace DirectX;

class TestScene : public SE::Engine
{
public:
    bool Setup(const std::string& scenePath = "")
    {
        ID3D11Device* device = GetRenderer().GetDevice();

        if (!m_skybox.Init(device, GetRenderer().GetStateCache(), GetShaders())) return false;
        if (!m_lights.Init(device))                return false;
        if (!m_pipeline.Init(device, GetAssets(), GetShaders())) return false;
        if (!m_shadowMap.Init(device, GetShaders(), 2048)) return false;

        // Forward HDR render target
        if (!m_forwardHDR_RT.Init(device, GetWindow().GetWidth(), GetWindow().GetHeight(),
                DXGI_FORMAT_R16G16B16A16_FLOAT, /*withDepth=*/true))
            return false;

        // Tone mapping
        if (!m_toneMap.Init(device, GetShaders())) return false;

        // Point shadow maps — 2 slots, 256x256 per cube face
        for (int i = 0; i < 2; ++i)
            if (!m_pointShadowMaps[i].Init(device, GetShaders(), 256)) return false;

        // Bloom
        if (!m_bloom.Init(device, GetShaders(),
                GetWindow().GetWidth(), GetWindow().GetHeight())) return false;

        // Scan available scenes
        m_sceneFiles = SE::SceneLoader::ScanSceneDirectory("Assets/Scenes");
        if (m_sceneFiles.empty())
        {
            SE_LOG_ERROR("No scene files found in Assets/Scenes/");
            return false;
        }

        // Determine which scene to load
        std::string targetScene = scenePath;
        if (targetScene.empty() && !m_sceneFiles.empty())
            targetScene = m_sceneFiles[0];

        if (!ApplyScene(targetScene))
            return false;

        SE_LOG_INFO("TestScene ready — %s", m_currentDesc.name.c_str());
        return true;
    }

    bool ApplyScene(const std::string& scenePath)
    {
        SE::SceneDescriptor desc;
        if (!SE::SceneLoader::LoadFromFile(scenePath, desc))
            return false;

        // Track current scene
        m_currentScenePath = scenePath;
        m_currentDesc      = desc;
        for (int i = 0; i < (int)m_sceneFiles.size(); ++i)
            if (m_sceneFiles[i] == scenePath) { m_selectedScene = i; break; }

        ID3D11Device* device = GetRenderer().GetDevice();

        // --- Skybox ---
        std::wstring skyboxW(desc.skybox.begin(), desc.skybox.end());
        if (!desc.skybox.empty())
            m_skybox.LoadPanorama(device, skyboxW.c_str());

        // --- Mesh ---
        m_mesh.reset();
        m_subMats.clear();
        if (!desc.mesh.path.empty())
        {
            m_mesh = GetAssets().GetMesh(desc.mesh.path);
            if (m_mesh)
                m_subMats = m_pipeline.LoadMeshMaterials(GetAssets(), *m_mesh);
            else
                SE_LOG_WARN("Failed to load mesh: %s", desc.mesh.path.c_str());
        }

        // --- Lights ---
        m_lights.elevDeg        = desc.sun.elevation;
        m_lights.azimDeg        = desc.sun.azimuth;
        m_lights.lightIntensity = desc.sun.intensity;
        m_lights.lightColor[0]  = desc.sun.color[0];
        m_lights.lightColor[1]  = desc.sun.color[1];
        m_lights.lightColor[2]  = desc.sun.color[2];
        m_lights.iblIntensity   = desc.iblIntensity;

        m_lights.numLights = (int)desc.pointLights.size();
        for (int i = 0; i < m_lights.numLights && i < 8; ++i)
        {
            auto& pl = desc.pointLights[i];
            m_lights.lights[i].position = { pl.position[0], pl.position[1], pl.position[2] };
            m_lights.lights[i].color    = { pl.color[0], pl.color[1], pl.color[2] };
            m_lights.lights[i].radius   = pl.radius;
            m_lightCastsShadow[i]       = pl.castShadow;
        }

        // --- Scene entities (rebuild) ---
        m_scene = SE::Scene{};
        SE::Entity* camEnt = m_scene.CreateEntity("Camera");
        m_camera           = camEnt->AddComponent<SE::CameraComponent>();
        m_camera->nearZ    = desc.camera.nearZ;
        m_camera->farZ     = desc.camera.farZ;
        m_camCtrl.freeFly.eye      = { desc.camera.eye[0], desc.camera.eye[1], desc.camera.eye[2] };
        m_camCtrl.freeFly.yawDeg   = desc.camera.yaw;
        m_camCtrl.freeFly.pitchDeg = desc.camera.pitch;
        m_camCtrl.Update(0.0f, GetInput(), *m_camera, GetWindow().GetHandle());

        // Mesh entity with transform
        SE::Entity* meshEnt = m_scene.CreateEntity("SceneMesh");
        m_bistroTransform          = meshEnt->AddComponent<SE::TransformComponent>();
        m_bistroTransform->position = { desc.mesh.position[0], desc.mesh.position[1], desc.mesh.position[2] };
        m_bistroTransform->eulerDeg = { desc.mesh.rotation[0], desc.mesh.rotation[1], desc.mesh.rotation[2] };
        m_bistroTransform->scale    = desc.mesh.scale;

        // Physics ball entity
        SE::Entity* ballEnt = m_scene.CreateEntity("PhysBall");
        m_ballTransform     = ballEnt->AddComponent<SE::TransformComponent>();
        m_ballRigidBody     = ballEnt->AddComponent<SE::RigidBodyComponent>();
        m_ballRadius        = desc.physics.ballRadius;
        m_ballTransform->scale = m_ballRadius;
        m_ballSpawn = { desc.physics.ballSpawn[0], desc.physics.ballSpawn[1], desc.physics.ballSpawn[2] };
        ResetBall();

        // --- Physics world (rebuild) ---
        m_physicsWorld = SE::PhysicsWorld{};
        m_physicsWorld.AddSphere(m_ballTransform, m_ballRigidBody, m_ballRadius);
        m_floorY = desc.physics.floor.max[1];
        m_obbFloor = SE::OBB::FromAABB(
            { desc.physics.floor.min[0], desc.physics.floor.min[1], desc.physics.floor.min[2] },
            { desc.physics.floor.max[0], desc.physics.floor.max[1], desc.physics.floor.max[2] });
        m_physicsWorld.AddStaticOBB(m_obbFloor, desc.physics.floor.friction, desc.physics.floor.restitution);

        // Character controller
        m_cc = SE::CharacterController{};
        m_cc.position = { desc.physics.characterSpawn[0], desc.physics.characterSpawn[1], desc.physics.characterSpawn[2] };
        m_gravityEnabled        = desc.physics.gravity;
        m_cc.gravityEnabled     = desc.physics.gravity;
        m_ballRigidBody->useGravity = desc.physics.gravity;

        // --- Post-process settings ---
        m_toneMap.op           = static_cast<SE::ToneMap::Operator>(desc.toneMapping.op);
        m_toneMap.exposure     = desc.toneMapping.exposure;
        m_toneMap.gammaCorrect = desc.toneMapping.gammaCorrect;
        m_bloom.enabled        = desc.bloom.enabled;
        m_bloom.threshold      = desc.bloom.threshold;
        m_bloom.intensity      = desc.bloom.intensity;
        m_bloom.scatter        = desc.bloom.scatter;

        // Update window title
        std::wstring title = L"FoxEngine " + std::wstring(desc.name.begin(), desc.name.end());
        SetWindowTextW(GetWindow().GetHandle(), title.c_str());

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
        // Handle pending scene load (deferred to avoid mid-frame state issues)
        if (!m_pendingSceneLoad.empty())
        {
            ApplyScene(m_pendingSceneLoad);
            m_pendingSceneLoad.clear();
        }

        ID3D11DeviceContext* ctx = GetRenderer().GetContext();
        float dt     = GetClock().GetDeltaTime();
        float aspect = (float)GetWindow().GetWidth() / (float)GetWindow().GetHeight();

        bool          mouseBlocked = ImGui::GetIO().WantCaptureMouse;
        SE::CameraController::Mode prevMode = m_camCtrl.GetMode();

        m_camCtrl.Update(dt, GetInput(), *m_camera,
                         GetWindow().GetHandle(), mouseBlocked);

        SE::CameraController::Mode mode = m_camCtrl.GetMode();

        // Init character position when first entering FPS mode.
        if (prevMode == SE::CameraController::Mode::FreeFly &&
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

        // Use bistro entity's transform for mesh world matrix
        m_meshWorld = m_bistroTransform->GetLocalMatrix();

        // Shadow pass
        {
            float er = XMConvertToRadians(m_lights.elevDeg);
            float ar = XMConvertToRadians(m_lights.azimDeg);
            XMFLOAT3 lightDir = { cosf(er) * sinf(ar), sinf(er), cosf(er) * cosf(ar) };
            SE::AABB raw = m_mesh ? m_mesh->GetBounds() : SE::AABB{};
            float s = m_bistroTransform->scale;
            SE::AABB bounds = {
                { raw.min.x * s, raw.min.y * s, raw.min.z * s },
                { raw.max.x * s, raw.max.y * s, raw.max.z * s }
            };
            m_shadowMap.UpdateLightMatrix(lightDir, bounds);
            m_shadowMap.BeginShadowPass(ctx);
            if (m_mesh) m_shadowMap.DrawMesh(ctx, *m_mesh, m_meshWorld);
            m_shadowMap.DrawSphere(ctx, m_ballTransform->position, m_ballRadius);
            m_shadowMap.EndShadowPass(ctx);
        }

        // Point shadow passes
        {
            int numCasters = 0;
            for (int i = 0; i < 2 && i < m_lights.numLights; ++i)
                if (m_lightCastsShadow[i]) numCasters = i + 1; else break;
            for (int li = 0; li < numCasters; ++li)
            {
                auto& light = m_lights.lights[li];
                for (int face = 0; face < 6; ++face)
                {
                    m_pointShadowMaps[li].BeginFace(ctx, face, light.position, light.radius);
                    if (m_mesh) m_pointShadowMaps[li].DrawMesh(ctx, *m_mesh, m_meshWorld);
                    m_pointShadowMaps[li].EndFace(ctx);
                }
            }
        }

        // --- Forward path (HDR + PBR + IBL) ---
        {
            ID3D11RenderTargetView* rtv = m_forwardHDR_RT.GetRTV();
            ID3D11DepthStencilView* dsv = m_forwardHDR_RT.GetDSV();
            const float black[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
            ctx->OMSetRenderTargets(1, &rtv, dsv);
            ctx->ClearRenderTargetView(rtv, black);
            ctx->ClearDepthStencilView(dsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
            D3D11_VIEWPORT vp = {};
            vp.Width = (float)m_forwardHDR_RT.GetWidth();
            vp.Height = (float)m_forwardHDR_RT.GetHeight();
            vp.MaxDepth = 1.0f;
            ctx->RSSetViewports(1, &vp);
        }

        m_skybox.Draw(ctx, view, proj);

        // Collect point shadow SRVs
        int numPtShadows = 0;
        for (int i = 0; i < 2 && i < m_lights.numLights; ++i)
            if (m_lightCastsShadow[i]) numPtShadows = i + 1; else break;
        ID3D11ShaderResourceView* ptSRVs[2] = { nullptr, nullptr };
        for (int i = 0; i < numPtShadows; ++i)
            ptSRVs[i] = m_pointShadowMaps[i].GetSRV();

        m_shadowMap.BindForLitPass(ctx);
        m_lights.BindPS(ctx, m_camera->eye, m_shadowMap.GetLightViewProj());
        m_pipeline.Begin(ctx, view, proj);
        m_pipeline.BindEnvironment(ctx, m_skybox.GetPanoramaSRV());
        m_pipeline.BindPointShadows(ctx, ptSRVs[0], ptSRVs[1], numPtShadows, m_pointShadowBias);
        m_pipeline.SetMaterialParams(ctx,
            { m_matTint[0], m_matTint[1], m_matTint[2] }, m_roughnessScale, m_metallic,
            m_debugShadow ? 1.0f : 0.0f);
        if (m_mesh)
        {
            m_pipeline.SubmitMesh(*m_mesh, m_meshWorld, m_subMats);
            m_pipeline.Flush(ctx);
        }
        m_pipeline.DrawSphere(ctx, m_ballTransform->position, m_ballRadius, { 1.0f, 0.45f, 0.05f });
        m_shadowMap.Unbind(ctx);

        // Unbind point shadow SRVs
        ID3D11ShaderResourceView* nullSRVs[2] = { nullptr, nullptr };
        ctx->PSSetShaderResources(5, 2, nullSRVs);

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
            m_pipeline.DrawWireBox(ctx, m_obbFloor.GetWorldMatrix(), { 1.0f, 0.85f, 0.1f });

            XMFLOAT3 bottom = { m_cc.position.x, m_cc.position.y + m_cc.radius, m_cc.position.z };
            m_pipeline.DrawWireSphere(ctx, bottom, m_cc.radius, { 1.0f, 0.4f, 0.8f });
            {
                XMFLOAT3 col = m_cc.isGrounded ? XMFLOAT3{ 1.0f, 0.0f, 1.0f }
                                               : XMFLOAT3{ 0.25f, 0.0f, 0.25f };
                XMFLOAT3 tip = { bottom.x + m_cc.contactNormal.x,
                                 bottom.y + m_cc.contactNormal.y,
                                 bottom.z + m_cc.contactNormal.z };
                m_pipeline.DrawLine(ctx, bottom, tip, col);
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

    void OnPostProcess() override
    {
        auto* ctx = GetRenderer().GetContext();
        uint32_t w = GetWindow().GetWidth(), h = GetWindow().GetHeight();

        // Resize HDR render target when the window changes size.
        if (m_forwardHDR_RT.GetWidth() != w || m_forwardHDR_RT.GetHeight() != h)
        {
            auto* dev = GetRenderer().GetDevice();
            m_forwardHDR_RT.Shutdown();
            m_forwardHDR_RT.Init(dev, w, h, DXGI_FORMAT_R16G16B16A16_FLOAT, true);
            m_bloom.Resize(dev, w, h);
        }

        SE::RenderTarget& hdrRT = m_forwardHDR_RT;

        if (m_bloom.enabled)
            m_bloom.Apply(ctx, hdrRT);

        GetRenderer().BindBackBuffer(ctx);
        m_toneMap.Apply(ctx, hdrRT.GetSRV(), w, h);
    }

private:
    void DrawUI(XMMATRIX view, XMMATRIX proj)
    {
        // HUD — FPS + asset stats rendered directly on screen (no window chrome)
        {
            ImDrawList* dl = ImGui::GetForegroundDrawList();
            char buf[128];
            sprintf_s(buf, "%.1f fps  |  %.2f ms",
                GetClock().GetFPS(), GetClock().GetDeltaTime() * 1000.0f);
            dl->AddText(ImVec2(10.0f, 10.0f), IM_COL32(255, 255, 255, 220), buf);
            sprintf_s(buf, "meshes:%u  textures:%u  submeshes:%u",
                GetAssets().CachedMeshCount(), GetAssets().CachedTextureCount(),
                m_mesh ? m_mesh->GetSubMeshCount() : 0u);
            dl->AddText(ImVec2(10.0f, 26.0f), IM_COL32(200, 200, 200, 180), buf);
        }

        // --- Scene Picker ---
        ImGui::Begin("Scene");
        if (!m_sceneFiles.empty())
        {
            auto getFilename = [](const std::string& path) -> std::string {
                auto pos = path.find_last_of("\\/");
                return (pos != std::string::npos) ? path.substr(pos + 1) : path;
            };

            if (ImGui::BeginCombo("Scene File", getFilename(m_sceneFiles[m_selectedScene]).c_str()))
            {
                for (int i = 0; i < (int)m_sceneFiles.size(); ++i)
                {
                    bool selected = (i == m_selectedScene);
                    if (ImGui::Selectable(getFilename(m_sceneFiles[i]).c_str(), selected))
                    {
                        if (i != m_selectedScene)
                        {
                            m_pendingSceneLoad = m_sceneFiles[i];
                        }
                    }
                    if (selected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            if (ImGui::Button("Reload"))
                m_pendingSceneLoad = m_sceneFiles[m_selectedScene];
            ImGui::SameLine();
            if (ImGui::Button("Rescan"))
                m_sceneFiles = SE::SceneLoader::ScanSceneDirectory("Assets/Scenes");
        }
        ImGui::Text("Active: %s", m_currentDesc.name.c_str());
        ImGui::End();

        // --- Camera ---
        ImGui::Begin("Camera");
        ImGui::Text("[Tab] to switch mode");
        if (m_camCtrl.GetMode() == SE::CameraController::Mode::FPS)
        {
            ImGui::Text("FPS — WASD move  RMB look  Space jump");
            ImGui::SliderFloat("Move Speed", &m_cc.moveSpeed, 1.0f, 20.0f);
            ImGui::SliderFloat("Jump Speed", &m_cc.jumpSpeed, 2.0f, 20.0f);
            ImGui::Text("pos (%.1f, %.1f, %.1f)  vy=%.2f  %s",
                m_cc.position.x, m_cc.position.y, m_cc.position.z,
                m_cc.velY, m_cc.isGrounded ? "GROUNDED" : "air");
            ImGui::Text("contact n (%.2f, %.2f, %.2f)  pvxz (%.1f, %.1f)",
                m_cc.contactNormal.x, m_cc.contactNormal.y, m_cc.contactNormal.z,
                m_cc.physVelX, m_cc.physVelZ);
        }
        else
        {
            ImGui::Text("Free Fly — WASD/QE move  RMB look  Shift fast");
            ImGui::SliderFloat("Move Speed",  &m_camCtrl.freeFly.moveSpeed,   1.0f, 200.0f);
            ImGui::SliderFloat("Sensitivity", &m_camCtrl.freeFly.sensitivity, 0.05f, 0.5f);
        }
        ImGui::SliderFloat("Near Z", &m_camera->nearZ, 0.001f, 10.0f, "%.3f");
        ImGui::SliderFloat("Far Z", &m_camera->farZ, 100.0f, 20000.0f, "%.0f");
        ImGui::Text("Eye (%.1f, %.1f, %.1f)",
            m_camera->eye.x, m_camera->eye.y, m_camera->eye.z);
        ImGui::End();

        // --- Material ---
        ImGui::Begin("Material");
        ImGui::ColorEdit3("Albedo Tint",      m_matTint);
        ImGui::SliderFloat("Roughness Scale", &m_roughnessScale, 0.0f, 2.0f);
        ImGui::SliderFloat("Metallic",        &m_metallic,       0.0f, 1.0f);
        ImGui::End();

        // --- Physics ---
        ImGui::Begin("Physics");
        ImGui::Checkbox("Show Colliders", &m_showColliders);
        ImGui::SameLine();
        ImGui::Checkbox("Raycast", &m_castRay);
        if (ImGui::Checkbox("Gravity", &m_gravityEnabled))
        {
            m_ballRigidBody->useGravity = m_gravityEnabled;
            m_cc.gravityEnabled         = m_gravityEnabled;
        }
        ImGui::Separator();
        ImGui::Text("Ball");
        ImGui::DragFloat3("Spawn pos",    &m_ballSpawn.x,                1.0f);
        ImGui::SliderFloat("Radius",      &m_ballRadius,                 0.1f, 10.0f);
        ImGui::SliderFloat("Mass",        &m_ballRigidBody->mass,        0.1f, 10.0f);
        ImGui::SliderFloat("Restitution", &m_ballRigidBody->restitution, 0.0f, 1.0f);
        ImGui::SliderFloat("Friction",    &m_ballRigidBody->friction,    0.0f, 1.0f);
        if (ImGui::SliderFloat("Floor Y", &m_floorY, -50.0f, 50.0f))
        {
            m_physicsWorld.Clear();
            m_physicsWorld.AddSphere(m_ballTransform, m_ballRigidBody, m_ballRadius);
            m_obbFloor = SE::OBB::FromAABB({ -30.0f, m_floorY - 0.5f, -30.0f },
                                            {  30.0f, m_floorY,        30.0f });
            m_physicsWorld.AddStaticOBB(m_obbFloor, 0.6f, 0.4f);
        }
        ImGui::Text("pos (%.1f, %.1f, %.1f)",
            m_ballTransform->position.x, m_ballTransform->position.y, m_ballTransform->position.z);
        ImGui::Text("vel (%.2f, %.2f, %.2f)",
            m_ballRigidBody->velocity.x, m_ballRigidBody->velocity.y, m_ballRigidBody->velocity.z);
        if (ImGui::Button("Reset"))     ResetBall();
        ImGui::SameLine();
        if (ImGui::Button("Launch up")) m_ballRigidBody->AddImpulse({ 0.0f, 20.0f, 0.0f });
        if (m_castRay)
        {
            ImGui::Separator();
            ImGui::Text("Raycast");
            if (m_rayHitValid)
            {
                const char* what =
                    m_rayHit.kind == SE::PhysicsWorld::RaycastHit::Kind::Sphere ? "Sphere" :
                    m_rayHit.kind == SE::PhysicsWorld::RaycastHit::Kind::OBB    ? "OBB"    : "Floor";
                ImGui::Text("Hit: %s  t=%.2f", what, m_rayHit.t);
                ImGui::Text("pos  (%.1f, %.1f, %.1f)",
                    m_rayHit.point.x,  m_rayHit.point.y,  m_rayHit.point.z);
                ImGui::Text("norm (%.2f, %.2f, %.2f)",
                    m_rayHit.normal.x, m_rayHit.normal.y, m_rayHit.normal.z);
            }
            else
            {
                ImGui::TextDisabled("no hit");
            }
        }
        ImGui::End();

        // --- Lighting ---
        ImGui::Begin("Lighting");
        ImGui::Text("Sun");
        ImGui::SliderFloat("Elevation",    &m_lights.elevDeg,  -90.0f, 90.0f,  "%.1f deg");
        ImGui::SliderFloat("Azimuth",      &m_lights.azimDeg, -180.0f, 180.0f, "%.1f deg");
        ImGui::SliderFloat("Intensity",    &m_lights.lightIntensity, 0.0f, 10.0f, "%.2f");
        ImGui::ColorEdit3("Light Color",   m_lights.lightColor);
        ImGui::SliderFloat("IBL Intensity", &m_lights.iblIntensity, 0.0f, 5.0f, "%.2f");
        ImGui::Separator();
        ImGui::Text("Light Debug");
        ImGui::Checkbox("Show Shadow Factor", &m_debugShadow);
        int dlm = (int)m_lights.debugLightMode;
        ImGui::RadioButton("Normal",    &dlm, 0); ImGui::SameLine();
        ImGui::RadioButton("Force Lit", &dlm, 1); ImGui::SameLine();
        ImGui::RadioButton("NdotL",     &dlm, 2);
        m_lights.debugLightMode = (float)dlm;
        ImGui::Separator();
        ImGui::Text("Tone Mapping");
        ImGui::SliderFloat("Exposure",     &m_toneMap.exposure, 0.01f, 10.0f, "%.2f");
        {
            int op = static_cast<int>(m_toneMap.op);
            ImGui::RadioButton("Reinhard", &op, 0); ImGui::SameLine();
            ImGui::RadioButton("ACES",     &op, 1);
            m_toneMap.op = static_cast<SE::ToneMap::Operator>(op);
        }
        ImGui::Checkbox("Gamma Correct", &m_toneMap.gammaCorrect);
        ImGui::Separator();
        ImGui::Text("Bloom");
        ImGui::Checkbox("Enable Bloom", &m_bloom.enabled);
        if (m_bloom.enabled)
        {
            ImGui::SliderFloat("Threshold", &m_bloom.threshold, 0.0f, 4.0f,  "%.2f");
            ImGui::SliderFloat("Strength",  &m_bloom.intensity, 0.0f, 0.5f,  "%.3f");
            ImGui::SliderFloat("Scatter",   &m_bloom.scatter,   0.0f, 1.0f,  "%.2f");
        }
        ImGui::Separator();
        ImGui::SliderFloat("Shadow Bias", &m_pointShadowBias, 0.001f, 0.1f, "%.4f");
        ImGui::Separator();
        ImGui::SliderInt("Active", &m_lights.numLights, 0, 8);
        for (int i = 0; i < m_lights.numLights; ++i)
        {
            ImGui::PushID(i);
            char label[24];
            sprintf_s(label, "Point Light %d", i + 1);
            if (ImGui::CollapsingHeader(label))
            {
                ImGui::DragFloat3("Position", &m_lights.lights[i].position.x, 1.0f, -1000.0f, 1000.0f);
                ImGui::ColorEdit3("Color",    &m_lights.lights[i].color.x);
                ImGui::SliderFloat("Radius",  &m_lights.lights[i].radius, 0.5f, 500.0f);
                if (i < 2)
                    ImGui::Checkbox("Cast Shadow", &m_lightCastsShadow[i]);
            }
            ImGui::PopID();
        }
        ImGui::End();

        // --- Input ---
        ImGui::Begin("Input");
        const SE::GamepadState& gp = GetInput().GetGamepad(0);
        if (gp.connected)
        {
            ImGui::Text("Controller 0  connected");
            ImGui::Separator();
            ImGui::Text("Left  stick  (%.2f, %.2f)", gp.leftX,  gp.leftY);
            ImGui::Text("Right stick  (%.2f, %.2f)", gp.rightX, gp.rightY);
            ImGui::Text("LT  %.2f   RT  %.2f",       gp.leftTrigger, gp.rightTrigger);
        }
        else
        {
            ImGui::TextDisabled("Controller 0  not connected");
        }
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

    SE::SkyboxRenderer                       m_skybox;
    SE::LightEnvironment                     m_lights;
    SE::ForwardPipeline                      m_pipeline;
    SE::ShadowMap                            m_shadowMap;
    SE::AssetHandle<SE::Mesh>                m_mesh;
    std::vector<SE::ForwardPipeline::SubMat> m_subMats;

    SE::Scene            m_scene;
    SE::CameraComponent* m_camera  = nullptr;
    SE::CameraController m_camCtrl;
    SE::TransformComponent* m_bistroTransform = nullptr;

    float m_matTint[3]     = { 1.0f, 1.0f, 1.0f };
    float m_roughnessScale = 1.0f;
    float m_metallic       = 0.0f;

    SE::PhysicsWorld        m_physicsWorld;
    SE::TransformComponent* m_ballTransform = nullptr;
    SE::RigidBodyComponent* m_ballRigidBody = nullptr;
    XMFLOAT3                m_ballSpawn     = { 0.0f, 30.0f, 0.0f };
    float                   m_ballRadius    = 1.0f;
    float                   m_floorY        = 0.0f;
    SE::OBB                 m_obbFloor;
    SE::CharacterController m_cc;

    bool                         m_gravityEnabled    = true;
    bool                         m_showColliders     = false;
    bool                         m_castRay           = false;
    bool                         m_debugShadow       = false;
    DirectX::XMMATRIX            m_meshWorld         = DirectX::XMMatrixIdentity();
    SE::RenderTarget             m_forwardHDR_RT;
    SE::ToneMap                  m_toneMap;
    SE::PointShadowMap           m_pointShadowMaps[2];
    SE::Bloom                    m_bloom;
    bool                         m_lightCastsShadow[8] = { true };
    float                        m_pointShadowBias       = 0.015f;
    SE::PhysicsWorld::RaycastHit m_rayHit        = {};
    bool                         m_rayHitValid   = false;

    // Scene loader state
    std::vector<std::string> m_sceneFiles;
    int                      m_selectedScene    = 0;
    std::string              m_currentScenePath;
    SE::SceneDescriptor      m_currentDesc;
    std::string              m_pendingSceneLoad;
};

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR lpCmdLine, int)
{
    SE::WindowDesc desc;
    desc.title  = L"FoxEngine";
    desc.width  = 1280;
    desc.height = 720;

    // Parse --scene argument
    std::string scenePath;
    if (lpCmdLine && strlen(lpCmdLine) > 0)
    {
        std::string args(lpCmdLine);
        auto pos = args.find("--scene");
        if (pos != std::string::npos)
        {
            pos += 7; // skip "--scene"
            while (pos < args.size() && (args[pos] == ' ' || args[pos] == '=')) ++pos;
            auto end = args.find(' ', pos);
            scenePath = args.substr(pos, end - pos);
            // If just a filename, prepend Assets/Scenes/
            if (scenePath.find('/') == std::string::npos && scenePath.find('\\') == std::string::npos)
                scenePath = "Assets/Scenes/" + scenePath;
        }
    }

    TestScene scene;
    if (!scene.Initialize(desc)) return 1;
    if (!scene.Setup(scenePath)) return 1;
    scene.Run();
    scene.Shutdown();
    return 0;
}
