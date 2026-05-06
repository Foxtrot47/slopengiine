#include "Engine/Scene/SceneLoader.h"
#include "Engine/Core/Logger.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>

namespace SE {

using json = nlohmann::json;

// Helper to read an array of 3 floats from JSON
static std::array<float, 3> ReadFloat3(const json& j, const std::string& key, std::array<float, 3> def)
{
    if (!j.contains(key) || !j[key].is_array() || j[key].size() < 3) return def;
    return { j[key][0].get<float>(), j[key][1].get<float>(), j[key][2].get<float>() };
}

bool SceneLoader::LoadFromFile(const std::string& path, SceneDescriptor& out)
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        SE_LOG_ERROR("SceneLoader: Failed to open '%s'", path.c_str());
        return false;
    }

    json root;
    try
    {
        root = json::parse(file);
    }
    catch (const json::parse_error& e)
    {
        SE_LOG_ERROR("SceneLoader: JSON parse error in '%s': %s", path.c_str(), e.what());
        return false;
    }

    out = SceneDescriptor{}; // reset to defaults

    // Name
    if (root.contains("name")) out.name = root["name"].get<std::string>();

    // Mesh
    if (root.contains("mesh"))
    {
        auto& m = root["mesh"];
        if (m.contains("path"))     out.mesh.path = m["path"].get<std::string>();
        if (m.contains("transform"))
        {
            auto& t = m["transform"];
            out.mesh.position = ReadFloat3(t, "position", out.mesh.position);
            out.mesh.rotation = ReadFloat3(t, "rotation", out.mesh.rotation);
            if (t.contains("scale")) out.mesh.scale = t["scale"].get<float>();
        }
    }

    // Skybox
    if (root.contains("skybox")) out.skybox = root["skybox"].get<std::string>();

    // Camera
    if (root.contains("camera"))
    {
        auto& c = root["camera"];
        out.camera.eye = ReadFloat3(c, "eye", out.camera.eye);
        if (c.contains("yaw"))   out.camera.yaw   = c["yaw"].get<float>();
        if (c.contains("pitch")) out.camera.pitch  = c["pitch"].get<float>();
        if (c.contains("nearZ")) out.camera.nearZ  = c["nearZ"].get<float>();
        if (c.contains("farZ"))  out.camera.farZ   = c["farZ"].get<float>();
    }

    // Sun
    if (root.contains("sun"))
    {
        auto& s = root["sun"];
        if (s.contains("elevation")) out.sun.elevation = s["elevation"].get<float>();
        if (s.contains("azimuth"))   out.sun.azimuth   = s["azimuth"].get<float>();
        if (s.contains("intensity")) out.sun.intensity  = s["intensity"].get<float>();
        out.sun.color = ReadFloat3(s, "color", out.sun.color);
    }

    // IBL
    if (root.contains("iblIntensity")) out.iblIntensity = root["iblIntensity"].get<float>();

    // Point lights
    if (root.contains("pointLights") && root["pointLights"].is_array())
    {
        for (auto& pl : root["pointLights"])
        {
            SceneDescriptor::PointLightDesc desc;
            desc.position   = ReadFloat3(pl, "position", desc.position);
            desc.color      = ReadFloat3(pl, "color",    desc.color);
            if (pl.contains("radius"))     desc.radius     = pl["radius"].get<float>();
            if (pl.contains("castShadow")) desc.castShadow = pl["castShadow"].get<bool>();
            out.pointLights.push_back(desc);
        }
    }

    // Physics
    if (root.contains("physics"))
    {
        auto& p = root["physics"];
        if (p.contains("floor"))
        {
            auto& f = p["floor"];
            out.physics.floor.min = ReadFloat3(f, "min", out.physics.floor.min);
            out.physics.floor.max = ReadFloat3(f, "max", out.physics.floor.max);
            if (f.contains("friction"))    out.physics.floor.friction    = f["friction"].get<float>();
            if (f.contains("restitution")) out.physics.floor.restitution = f["restitution"].get<float>();
        }
        out.physics.ballSpawn      = ReadFloat3(p, "ballSpawn",      out.physics.ballSpawn);
        out.physics.characterSpawn = ReadFloat3(p, "characterSpawn", out.physics.characterSpawn);
        if (p.contains("ballRadius")) out.physics.ballRadius = p["ballRadius"].get<float>();
        if (p.contains("gravity"))    out.physics.gravity    = p["gravity"].get<bool>();
    }

    // Tone mapping
    if (root.contains("toneMapping"))
    {
        auto& tm = root["toneMapping"];
        if (tm.contains("operator"))
        {
            std::string op = tm["operator"].get<std::string>();
            out.toneMapping.op = (op == "Reinhard") ? 0 : 1;
        }
        if (tm.contains("exposure"))     out.toneMapping.exposure     = tm["exposure"].get<float>();
        if (tm.contains("gammaCorrect")) out.toneMapping.gammaCorrect = tm["gammaCorrect"].get<bool>();
    }

    // Bloom
    if (root.contains("bloom"))
    {
        auto& b = root["bloom"];
        if (b.contains("enabled"))   out.bloom.enabled   = b["enabled"].get<bool>();
        if (b.contains("threshold")) out.bloom.threshold = b["threshold"].get<float>();
        if (b.contains("intensity")) out.bloom.intensity = b["intensity"].get<float>();
        if (b.contains("scatter"))   out.bloom.scatter   = b["scatter"].get<float>();
    }

    SE_LOG_INFO("SceneLoader: Loaded '%s' (%s)", out.name.c_str(), path.c_str());
    return true;
}

std::vector<std::string> SceneLoader::ScanSceneDirectory(const std::string& directory)
{
    std::vector<std::string> results;
    namespace fs = std::filesystem;

    std::error_code ec;
    if (!fs::exists(directory, ec)) return results;

    for (auto& entry : fs::directory_iterator(directory, ec))
    {
        if (entry.is_regular_file() && entry.path().extension() == ".json")
            results.push_back(entry.path().string());
    }

    std::sort(results.begin(), results.end());
    return results;
}

} // namespace SE
