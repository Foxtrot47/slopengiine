#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <cstdint>

using Microsoft::WRL::ComPtr;

namespace SE {

class Texture2D
{
public:
    // Load a PNG / JPG / BMP / TIFF from disk via WIC. Generates a full mip chain.
    bool LoadFromFile(ID3D11Device* device, ID3D11DeviceContext* ctx, const wchar_t* path);

    // Create directly from a raw RGBA8 pixel buffer. Generates a full mip chain.
    bool CreateFromMemory(ID3D11Device* device, ID3D11DeviceContext* ctx,
                          const uint8_t* rgba, uint32_t width, uint32_t height);

    // Bind the SRV to the pixel shader.
    void BindPS(ID3D11DeviceContext* ctx, uint32_t slot) const;

    uint32_t GetWidth()  const { return m_width; }
    uint32_t GetHeight() const { return m_height; }
    bool     IsValid()   const { return m_srv != nullptr; }

private:
    bool CreateSRV(ID3D11Device* device, ID3D11DeviceContext* ctx,
                   const uint8_t* rgba, uint32_t width, uint32_t height);

    ComPtr<ID3D11Texture2D>          m_texture;
    ComPtr<ID3D11ShaderResourceView> m_srv;
    uint32_t m_width  = 0;
    uint32_t m_height = 0;
};

} // namespace SE
