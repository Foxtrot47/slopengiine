#include "Engine/Renderer/SamplerState.h"
#include "Engine/Core/Logger.h"

namespace SE {

static D3D11_FILTER ToD3D(FilterMode f)
{
    switch (f)
    {
    case FilterMode::Point:       return D3D11_FILTER_MIN_MAG_MIP_POINT;
    case FilterMode::Bilinear:    return D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
    case FilterMode::Trilinear:   return D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    case FilterMode::Anisotropic: return D3D11_FILTER_ANISOTROPIC;
    default:                      return D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    }
}

static D3D11_TEXTURE_ADDRESS_MODE ToD3D(AddressMode a)
{
    switch (a)
    {
    case AddressMode::Wrap:   return D3D11_TEXTURE_ADDRESS_WRAP;
    case AddressMode::Clamp:  return D3D11_TEXTURE_ADDRESS_CLAMP;
    case AddressMode::Mirror: return D3D11_TEXTURE_ADDRESS_MIRROR;
    case AddressMode::Border: return D3D11_TEXTURE_ADDRESS_BORDER;
    default:                  return D3D11_TEXTURE_ADDRESS_WRAP;
    }
}

bool SamplerState::Create(ID3D11Device* device, const SamplerDesc& desc)
{
    D3D11_SAMPLER_DESC sd = {};
    sd.Filter        = ToD3D(desc.filter);
    sd.AddressU      = ToD3D(desc.addressU);
    sd.AddressV      = ToD3D(desc.addressV);
    sd.AddressW      = ToD3D(desc.addressU); // W mirrors U for 3D textures
    sd.MaxAnisotropy = desc.anisotropy;
    sd.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sd.MinLOD        = 0.0f;
    sd.MaxLOD        = D3D11_FLOAT32_MAX;

    HRESULT hr = device->CreateSamplerState(&sd, &m_sampler);
    if (FAILED(hr))
    {
        SE_LOG_ERROR("SamplerState::Create failed: 0x%08X", hr);
        return false;
    }
    return true;
}

void SamplerState::BindPS(ID3D11DeviceContext* ctx, uint32_t slot) const
{
    ctx->PSSetSamplers(slot, 1, m_sampler.GetAddressOf());
}

void SamplerState::BindVS(ID3D11DeviceContext* ctx, uint32_t slot) const
{
    ctx->VSSetSamplers(slot, 1, m_sampler.GetAddressOf());
}

} // namespace SE
