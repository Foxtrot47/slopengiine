#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <cstdint>
#include "Engine/Renderer/ShaderLibrary.h"
#include "Engine/Renderer/ConstantBuffer.h"
#include "Engine/Renderer/FullscreenQuad.h"

using Microsoft::WRL::ComPtr;

namespace SE {

class ToneMap
{
public:
    enum class Operator { Reinhard = 0, ACES = 1 };

    bool Init(ID3D11Device* device, ShaderLibrary& shaders);

    // Draw tone-mapped HDR scene to the currently-bound render target.
    // Caller must bind the destination RTV before calling (e.g. BindBackBuffer).
    // Unbinds the HDR SRV from t0 on return.
    void Apply(ID3D11DeviceContext* ctx,
               ID3D11ShaderResourceView* hdrSRV,
               uint32_t width, uint32_t height);

    float    exposure     = 1.0f;
    Operator op           = Operator::ACES;
    bool     gammaCorrect = false; // keep off until textures are sRGB-linearised (M52)

private:
    struct CBData
    {
        float exposure;
        int   op;
        int   gammaCorrect;
        float _pad;
    };

    const ShaderPermutation*   m_perm = nullptr;
    ConstantBuffer<CBData>     m_cb;
    FullscreenQuad             m_quad;
};

} // namespace SE
