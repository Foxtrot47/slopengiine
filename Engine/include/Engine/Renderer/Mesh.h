#pragma once
#include <d3d11.h>
#include <vector>
#include <string>
#include "Engine/Renderer/VertexBuffer.h"
#include "Engine/Renderer/IndexBuffer.h"

namespace SE {

// Vertex layout used for all mesh geometry.
// Matches the POSITION/NORMAL/TEXCOORD input layout in Basic.hlsl.
struct MeshVertex
{
    float x,  y,  z;    // POSITION
    float nx, ny, nz;   // NORMAL
    float u,  v;        // TEXCOORD
    float tx, ty, tz;   // TANGENT
    float bx, by, bz;   // BINORMAL (bitangent)
};

// Per-submesh texture paths as extracted from the source file's material.
// Paths are relative to GetDirectory(). Empty string = no texture assigned.
struct SubMeshInfo
{
    std::string albedoPath;
    std::string normalPath;
    std::string roughnessPath;
};

class Mesh
{
public:
    bool Load(ID3D11Device* device, const char* path);
    void Draw(ID3D11DeviceContext* ctx) const;
    void DrawSubMesh(ID3D11DeviceContext* ctx, uint32_t index) const;

    uint32_t         GetSubMeshCount() const { return static_cast<uint32_t>(m_subMeshes.size()); }
    SubMeshInfo      GetSubMeshInfo(uint32_t index) const;
    const std::string& GetDirectory() const { return m_directory; }

private:
    struct SubMesh
    {
        VertexBuffer vb;
        IndexBuffer  ib;
        std::string  albedoPath;
        std::string  normalPath;
        std::string  roughnessPath;
    };
    std::vector<SubMesh> m_subMeshes;
    std::string          m_directory;
};

} // namespace SE
