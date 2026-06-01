#include "Engine/Renderer/Mesh.h"
#include "Engine/Core/Logger.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/material.h>
#include <assimp/GltfMaterial.h>
#include <vector>

namespace SE {

bool Mesh::Load(ID3D11Device* device, const char* path)
{
    Assimp::Importer importer;

    const aiScene* scene = importer.ReadFile(path,
        aiProcess_Triangulate          |
        aiProcess_GenSmoothNormals     |
        aiProcess_ConvertToLeftHanded  |
        aiProcess_FlipUVs              |
        aiProcess_JoinIdenticalVertices |
        aiProcess_CalcTangentSpace);

    if (!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !scene->mRootNode)
    {
        SE_LOG_ERROR("Mesh::Load '%s': %s", path, importer.GetErrorString());
        return false;
    }

    // Extract directory so callers can resolve relative texture paths.
    {
        std::string p(path);
        auto pos = p.find_last_of("/\\");
        m_directory = (pos != std::string::npos) ? p.substr(0, pos + 1) : "";
    }

    m_subMeshes.reserve(scene->mNumMeshes);
    m_bounds = AABB{}; // reset to invalid

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
            vtx.tx = mesh->mTangents   ? mesh->mTangents[v].x   : 1.0f;
            vtx.ty = mesh->mTangents   ? mesh->mTangents[v].y   : 0.0f;
            vtx.tz = mesh->mTangents   ? mesh->mTangents[v].z   : 0.0f;
            vtx.bx = mesh->mBitangents ? mesh->mBitangents[v].x : 0.0f;
            vtx.by = mesh->mBitangents ? mesh->mBitangents[v].y : 1.0f;
            vtx.bz = mesh->mBitangents ? mesh->mBitangents[v].z : 0.0f;
            verts.push_back(vtx);
            m_bounds.Expand({ vtx.x, vtx.y, vtx.z });
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

        // Extract material texture paths (glTF PBR slots).
        if (mesh->mMaterialIndex < scene->mNumMaterials)
        {
            const aiMaterial* mat = scene->mMaterials[mesh->mMaterialIndex];
            aiString s;

            auto tryGet = [&](aiTextureType t) -> std::string {
                if (mat->GetTexture(t, 0, &s) == AI_SUCCESS &&
                    s.length > 0 && s.C_Str()[0] != '*')
                    return s.C_Str();
                return {};
            };

            // Base color: prefer PBR slot, fall back to legacy diffuse.
            sm.albedoPath = tryGet(aiTextureType_BASE_COLOR);
            if (sm.albedoPath.empty())
                sm.albedoPath = tryGet(aiTextureType_DIFFUSE);

            // Normal map.
            sm.normalPath = tryGet(aiTextureType_NORMALS);

            // Roughness: glTF metallic-roughness texture (G = roughness).
            sm.roughnessPath = tryGet(aiTextureType_DIFFUSE_ROUGHNESS);
            if (sm.roughnessPath.empty())
                sm.roughnessPath = tryGet(aiTextureType_UNKNOWN);

            // Alpha mode detection
            aiString alphaStr;
            if (mat->Get(AI_MATKEY_GLTF_ALPHAMODE, alphaStr) == AI_SUCCESS)
            {
                if (strcmp(alphaStr.C_Str(), "MASK") == 0)
                    sm.alphaMode = AlphaMode::Cutout;
                else if (strcmp(alphaStr.C_Str(), "BLEND") == 0)
                    sm.alphaMode = AlphaMode::Transparent;
            }
            else
            {
                // FBX fallback: if an opacity texture exists or opacity < 1, treat as cutout
                aiString opacityTex;
                float opacity = 1.0f;
                mat->Get(AI_MATKEY_OPACITY, opacity);

                if (mat->GetTexture(aiTextureType_OPACITY, 0, &opacityTex) == AI_SUCCESS)
                    sm.alphaMode = AlphaMode::Cutout;
                else if (opacity < 0.99f)
                    sm.alphaMode = AlphaMode::Transparent;
            }

            // Alpha cutoff (glTF ALPHACUTOFF property)
            float cutoff = 0.5f;
            if (mat->Get(AI_MATKEY_GLTF_ALPHACUTOFF, cutoff) == AI_SUCCESS)
                sm.alphaCutoff = cutoff;
        }

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

void Mesh::DrawSubMesh(ID3D11DeviceContext* ctx, uint32_t index) const
{
    const SubMesh& sm = m_subMeshes[index];
    sm.vb.Bind(ctx);
    sm.ib.Bind(ctx);
    ctx->DrawIndexed(sm.ib.GetCount(), 0, 0);
}

SubMeshInfo Mesh::GetSubMeshInfo(uint32_t index) const
{
    const SubMesh& sm = m_subMeshes[index];
    SubMeshInfo info;
    info.albedoPath    = sm.albedoPath;
    info.normalPath    = sm.normalPath;
    info.roughnessPath = sm.roughnessPath;
    info.alphaMode     = sm.alphaMode;
    info.alphaCutoff   = sm.alphaCutoff;
    return info;
}

} // namespace SE
