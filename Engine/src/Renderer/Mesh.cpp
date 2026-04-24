#include "Engine/Renderer/Mesh.h"
#include "Engine/Core/Logger.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <vector>

namespace SE {

bool Mesh::Load(ID3D11Device* device, const char* path)
{
    Assimp::Importer importer;

    const aiScene* scene = importer.ReadFile(path,
        aiProcess_Triangulate        |
        aiProcess_GenSmoothNormals   |
        aiProcess_FlipUVs            |
        aiProcess_JoinIdenticalVertices |
        aiProcess_CalcTangentSpace);

    if (!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !scene->mRootNode)
    {
        SE_LOG_ERROR("Mesh::Load '%s': %s", path, importer.GetErrorString());
        return false;
    }

    m_subMeshes.reserve(scene->mNumMeshes);

    for (uint32_t m = 0; m < scene->mNumMeshes; ++m)
    {
        const aiMesh* mesh = scene->mMeshes[m];

        std::vector<MeshVertex> verts;
        verts.reserve(mesh->mNumVertices);

        for (uint32_t v = 0; v < mesh->mNumVertices; ++v)
        {
            MeshVertex vtx;
            vtx.x  = mesh->mVertices[v].x;
            vtx.y  = mesh->mVertices[v].y;
            vtx.z  = mesh->mVertices[v].z;
            vtx.nx = mesh->mNormals ? mesh->mNormals[v].x : 0.0f;
            vtx.ny = mesh->mNormals ? mesh->mNormals[v].y : 1.0f;
            vtx.nz = mesh->mNormals ? mesh->mNormals[v].z : 0.0f;
            if (mesh->mTextureCoords[0])
            {
                vtx.u = mesh->mTextureCoords[0][v].x;
                vtx.v = mesh->mTextureCoords[0][v].y;
            }
            else { vtx.u = vtx.v = 0.0f; }
            verts.push_back(vtx);
        }

        std::vector<uint32_t> indices;
        indices.reserve(mesh->mNumFaces * 3);
        for (uint32_t f = 0; f < mesh->mNumFaces; ++f)
        {
            const aiFace& face = mesh->mFaces[f];
            for (uint32_t i = 0; i < face.mNumIndices; ++i)
                indices.push_back(face.mIndices[i]);
        }

        SubMesh sm;
        if (!sm.vb.Create(device, verts.data(),
                          static_cast<uint32_t>(verts.size() * sizeof(MeshVertex)),
                          sizeof(MeshVertex)))
            return false;
        if (!sm.ib.Create(device, indices.data(),
                          static_cast<uint32_t>(indices.size())))
            return false;

        m_subMeshes.push_back(std::move(sm));
    }

    SE_LOG_INFO("Mesh loaded: '%s' — %u sub-mesh(es)", path, GetSubMeshCount());
    return true;
}

void Mesh::Draw(ID3D11DeviceContext* ctx) const
{
    for (const auto& sm : m_subMeshes)
    {
        sm.vb.Bind(ctx);
        sm.ib.Bind(ctx);
        ctx->DrawIndexed(sm.ib.GetCount(), 0, 0);
    }
}

} // namespace SE
