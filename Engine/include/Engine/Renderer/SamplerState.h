#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <cstdint>

using Microsoft::WRL::ComPtr;

namespace SE {

enum class FilterMode
{
    Point,        // No filtering — nearest texel. Pixelated.
    Bilinear,     // Linear min/mag, point mip.
    Trilinear,    // Linear min/mag/mip. Smooth across mip boundaries.
    Anisotropic,  // Best quality for surfaces at oblique angles.
};

enum class AddressMode
{
    Wrap,    // Tile the texture.
    Clamp,   // Stretch the edge pixel beyond [0,1].
    Mirror,  // Tile with alternating flips.
    Border,  // Use a constant border colour (black).
};

struct SamplerDesc
{
    FilterMode  filter       = FilterMode::Bilinear;
    AddressMode addressU     = AddressMode::Wrap;
    AddressMode addressV     = AddressMode::Wrap;
    uint32_t    anisotropy   = 8;   // only used when filter == Anisotropic
};

class SamplerState
{
public:
    bool Create(ID3D11Device* device, const SamplerDesc& desc = {});

    void BindPS(ID3D11DeviceContext* ctx, uint32_t slot) const;
    void BindVS(ID3D11DeviceContext* ctx, uint32_t slot) const;

private:
    ComPtr<ID3D11SamplerState> m_sampler;
};

} // namespace SE
