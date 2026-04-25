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
    void Init(ID3D11Device* device);

    // Returns a shared handle to the asset; loads on first request, returns
    // the cached instance on subsequent calls while any handle is alive.
    // Returns nullptr on load failure.
    AssetHandle<Mesh>      GetMesh   (const std::string&  path);
    AssetHandle<Texture2D> GetTexture(const std::wstring& path);

    uint32_t CachedMeshCount()    const;
    uint32_t CachedTextureCount() const;

private:
    ID3D11Device* m_device = nullptr;
    std::unordered_map<std::string,  std::weak_ptr<Mesh>>     m_meshes;
    std::unordered_map<std::wstring, std::weak_ptr<Texture2D>> m_textures;
};

} // namespace SE
