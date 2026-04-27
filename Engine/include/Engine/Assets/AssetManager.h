#pragma once
#include <d3d11.h>
#include <memory>
#include <string>
#include <unordered_map>
#include "Engine/Renderer/Mesh.h"
#include "Engine/Renderer/Texture2D.h"

namespace SE {

template<typename T>
using AssetHandle = std::shared_ptr<T>;

class AssetManager
{
public:
    void Init(ID3D11Device* device, ID3D11DeviceContext* ctx);

    // Returns a shared handle to the asset; loads on first request, returns
    // the cached instance on subsequent calls while any handle is alive.
    // Returns nullptr on load failure.
    AssetHandle<Mesh>      GetMesh   (const std::string&  path);
    AssetHandle<Texture2D> GetTexture(const std::wstring& path);

    // Fallback 1×1 textures for submeshes missing a particular map.
    AssetHandle<Texture2D> GetDefaultWhite();   // flat white albedo / roughness
    AssetHandle<Texture2D> GetDefaultNormal();  // flat tangent-space normal (128,128,255)

    uint32_t CachedMeshCount()    const;
    uint32_t CachedTextureCount() const;

private:
    ID3D11Device*        m_device  = nullptr;
    ID3D11DeviceContext* m_context = nullptr;
    std::unordered_map<std::string,  std::weak_ptr<Mesh>>     m_meshes;
    std::unordered_map<std::wstring, std::weak_ptr<Texture2D>> m_textures;
    std::weak_ptr<Texture2D> m_defaultWhite;
    std::weak_ptr<Texture2D> m_defaultNormal;
};

} // namespace SE
