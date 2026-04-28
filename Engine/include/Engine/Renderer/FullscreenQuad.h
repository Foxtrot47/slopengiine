#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include "Engine/Renderer/ShaderLibrary.h"

using Microsoft::WRL::ComPtr;

namespace SE {

class FullscreenQuad
{
public:
    bool Init(ID3D11Device* device, ShaderLibrary& shaders);
    void Shutdown();

    // Bind the passthrough shader + point sampler, then draw quad.
    // Caller must bind the source SRV to t0 before calling.
    void Draw(ID3D11DeviceContext* ctx);

private:
    ComPtr<ID3D11Buffer>       m_vb;
    ComPtr<ID3D11Buffer>       m_ib;
    ComPtr<ID3D11InputLayout>  m_layout;
    ComPtr<ID3D11SamplerState> m_pointSampler;
    const ShaderPermutation*   m_perm = nullptr;
};

} // namespace SE
