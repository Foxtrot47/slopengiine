#include "Engine/Renderer/Material.h"

namespace SE {

bool Material::Create(ID3D11Device* device)
{
    return m_cb.Create(device);
}

bool Material::LoadAlbedo(ID3D11Device* device, ID3D11DeviceContext* ctx, const wchar_t* path)
{
    return m_albedo.LoadFromFile(device, ctx, path);
}

bool Material::LoadRoughness(ID3D11Device* device, ID3D11DeviceContext* ctx, const wchar_t* path)
{
    return m_roughness.LoadFromFile(device, ctx, path);
}

bool Material::LoadNormal(ID3D11Device* device, ID3D11DeviceContext* ctx, const wchar_t* path)
{
    return m_normal.LoadFromFile(device, ctx, path);
}

void Material::Bind(ID3D11DeviceContext* ctx)
{
    MaterialData d;
    d.albedoTint     = { albedoTint[0], albedoTint[1], albedoTint[2] };
    d.roughnessScale = roughnessScale;
    d.metallic       = metallic;
    d._pad           = {};
    m_cb.Update(ctx, d);
    m_cb.BindPS(ctx, 3);

    m_albedo.BindPS(ctx, 0);
    m_roughness.BindPS(ctx, 1);
    m_normal.BindPS(ctx, 2);
}

} // namespace SE
