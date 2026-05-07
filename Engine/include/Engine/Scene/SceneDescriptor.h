#pragma once
#include <string>
#include <vector>
#include <array>

namespace SE {

struct SceneDescriptor
{
    std::string name = "Untitled";

    // Mesh
    struct MeshEntry {
        std::string path;
        std::array<float, 3> position = { 0.0f, 0.0f, 0.0f };
        std::array<float, 3> rotation = { 0.0f, 0.0f, 0.0f }; // euler degrees (pitch, yaw, roll)
        float scale = 1.0f;
    };
    MeshEntry mesh;

    // Skybox
    std::string skybox;

    // Optional equirectangular HDR panorama for SSR fallback reflections
    std::string reflectionPanorama;

    // Camera
    struct CameraDesc {
        std::array<float, 3> eye   = { 0.0f, 5.0f, -10.0f };
        float yaw   = 0.0f;
        float pitch = 0.0f;
        float nearZ = 0.15f;
        float farZ  = 4000.0f;
    };
    CameraDesc camera;

    // Directional light (sun)
    struct SunDesc {
        float elevation = 63.0f;
        float azimuth   = -134.6f;
        float intensity = 1.0f;
        std::array<float, 3> color = { 1.0f, 1.0f, 1.0f };
    };
    SunDesc sun;

    float iblIntensity = 1.0f;

    // Point lights
    struct PointLightDesc {
        std::array<float, 3> position = { 0.0f, 5.0f, 0.0f };
        std::array<float, 3> color    = { 1.0f, 1.0f, 1.0f };
        float radius      = 50.0f;
        bool  castShadow  = false;
    };
    std::vector<PointLightDesc> pointLights;

    // Physics
    struct PhysicsDesc {
        struct FloorDesc {
            std::array<float, 3> min = { -30.0f, -0.5f, -30.0f };
            std::array<float, 3> max = {  30.0f,  0.0f,  30.0f };
            float friction    = 0.6f;
            float restitution = 0.4f;
        };
        FloorDesc floor;
        std::array<float, 3> ballSpawn      = { 0.0f, 30.0f, 0.0f };
        float                ballRadius     = 1.0f;
        std::array<float, 3> characterSpawn = { 0.0f, 5.0f, 0.0f };
        bool                 gravity        = true;
    };
    PhysicsDesc physics;

    // Tone mapping
    struct ToneMappingDesc {
        int   op           = 1; // 0=Reinhard, 1=ACES
        float exposure     = 1.0f;
        bool  gammaCorrect = false;
    };
    ToneMappingDesc toneMapping;

    // Bloom
    struct BloomDesc {
        bool  enabled   = true;
        float threshold = 1.0f;
        float intensity = 0.1f;
        float scatter   = 0.7f;
    };
    BloomDesc bloom;

    // Optional array of scene objects with explicit geometry and PBR material paths.
    // Rendered in addition to the main mesh. Enables a fully JSON-driven PBR showcase.
    struct SceneObject
    {
        enum class Type { Sphere, Plane } type = Type::Sphere;

        std::array<float, 3> position = { 0.f, 0.f, 0.f };
        float radius    = 1.0f;   // sphere radius
        float halfSizeX = 10.0f;  // plane half-extents (plane only)
        float halfSizeZ = 10.0f;

        // PBR material texture paths (DDS, relative to working dir)
        std::string albedoPath;
        std::string normalPath;
        std::string roughnessPath;
        std::string metallicPath;

        std::array<float, 3> tint = { 1.f, 1.f, 1.f };
    };
    std::vector<SceneObject> objects;
};

} // namespace SE
