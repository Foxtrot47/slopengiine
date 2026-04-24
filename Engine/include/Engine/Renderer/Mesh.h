#pragma once
#include <d3d11.h>
#include <vector>
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

class Mesh
{
public:
    bool Load(ID3D11Device* device, const char* path);
    void Draw(ID3D11DeviceContext* ctx) const;

    uint32_t GetSubMeshCount() const { return static_cast<uint32_t>(m_subMeshes.size()); }

private:
    struct SubMesh
    {
        VertexBuffer vb;
        IndexBuffer  ib;
    };
    std::vector<SubMesh> m_subMeshes;
};

} // namespace SE
