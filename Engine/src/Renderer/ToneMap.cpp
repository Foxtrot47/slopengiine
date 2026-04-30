#include "Engine/Renderer/ToneMap.h"
#include "Engine/Core/Logger.h"

namespace SE {

bool ToneMap::Init(ID3D11Device* device, ShaderLibrary& shaders)
{
    m_perm = shaders.Get(L"Shaders/ToneMap.hlsl");
    if (!m_perm) { SE_LOG_ERROR("ToneMap: failed to load shader"); return false; }

    if (!m_cb.Create(device))           return false;
    if (!m_quad.Init(device, shaders))  return false;

    return true;
}

void ToneMap::Apply(ID3D11DeviceContext* ctx,
                    ID3D11ShaderResourceView* hdrSRV,
                    uint32_t width, uint32_t height)
{
    CBData cb = { exposure, static_cast<int>(op), gammaCorrect ? 1 : 0, 0.0f };
    m_cb.Update(ctx, cb);
    m_cb.BindPS(ctx, 0);

    D3D11_VIEWPORT vp = {};
    vp.Width    = static_cast<float>(width);
    vp.Height   = static_cast<float>(height);
    vp.MaxDepth = 1.0f;
    ctx->RSSetViewports(1, &vp);

    ctx->PSSetShaderResources(0, 1, &hdrSRV);

    m_quad.Draw(ctx, m_perm);

    ID3D11ShaderResourceView* nullSRV = nullptr;
    ctx->PSSetShaderResources(0, 1, &nullSRV);
}

} // namespace SE
