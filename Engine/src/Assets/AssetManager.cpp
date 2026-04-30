#include "Engine/Assets/AssetManager.h"
#include "Engine/Core/Logger.h"

namespace SE {

void AssetManager::Init(ID3D11Device* device, ID3D11DeviceContext* ctx)
{
    m_device  = device;
    m_context = ctx;
    SE_LOG_INFO("AssetManager initialised");
}

AssetHandle<Mesh> AssetManager::GetMesh(const std::string& path)
{
    auto it = m_meshes.find(path);
    if (it != m_meshes.end())
        if (auto h = it->second.lock()) return h;

    auto mesh = std::make_shared<Mesh>();
    if (!mesh->Load(m_device, path.c_str()))
    {
        SE_LOG_ERROR("AssetManager: failed to load mesh '%s'", path.c_str());
        return nullptr;
    }
    m_meshes[path] = mesh;
    SE_LOG_INFO("AssetManager: loaded mesh '%s'", path.c_str());
    return mesh;
}

AssetHandle<Texture2D> AssetManager::GetTexture(const std::wstring& path)
{
    auto it = m_textures.find(path);
    if (it != m_textures.end())
        if (auto h = it->second.lock()) return h;

    auto tex = std::make_shared<Texture2D>();
    bool ok  = false;

    // Route by extension — .dds via DirectXTex, everything else via WIC.
    bool isDDS = path.size() >= 4 &&
                 _wcsicmp(path.c_str() + path.size() - 4, L".dds") == 0;
    if (isDDS)
        ok = tex->LoadFromDDS(m_device, path.c_str());
    else
        ok = tex->LoadFromFile(m_device, m_context, path.c_str());

    if (!ok)
    {
        SE_LOG_ERROR("AssetManager: failed to load texture");
        return nullptr;
    }
    m_textures[path] = tex;
    return tex;
}

AssetHandle<Texture2D> AssetManager::GetDefaultWhite()
{
    if (auto h = m_defaultWhite.lock()) return h;
    auto tex = std::make_shared<Texture2D>();
    uint8_t px[4] = { 255, 255, 255, 255 };
    tex->CreateFromMemory(m_device, m_context, px, 1, 1);
    m_defaultWhite = tex;
    return tex;
}

AssetHandle<Texture2D> AssetManager::GetDefaultNormal()
{
    if (auto h = m_defaultNormal.lock()) return h;
    auto tex = std::make_shared<Texture2D>();
    uint8_t px[4] = { 128, 128, 255, 255 };
    tex->CreateFromMemory(m_device, m_context, px, 1, 1);
    m_defaultNormal = tex;
    return tex;
}

uint32_t AssetManager::CachedMeshCount() const
{
    uint32_t n = 0;
    for (auto& [k, w] : m_meshes)
        if (!w.expired()) ++n;
    return n;
}

uint32_t AssetManager::CachedTextureCount() const
{
    uint32_t n = 0;
    for (auto& [k, w] : m_textures)
        if (!w.expired()) ++n;
    return n;
}

} // namespace SE
