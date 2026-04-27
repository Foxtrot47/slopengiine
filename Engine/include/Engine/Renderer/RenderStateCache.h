#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <vector>
#include <utility>

namespace SE {

// Lazy-creates and deduplicates D3D11 immutable state objects.
// Keyed by bitwise comparison of the desc struct — callers must zero-init descs before filling.
class RenderStateCache
{
public:
    void Init(ID3D11Device* device);
    void Clear();

    ID3D11RasterizerState*   GetRasterizerState(const D3D11_RASTERIZER_DESC& desc);
    ID3D11DepthStencilState* GetDepthStencilState(const D3D11_DEPTH_STENCIL_DESC& desc);
    ID3D11BlendState*        GetBlendState(const D3D11_BLEND_DESC& desc);

private:
    ID3D11Device* m_device = nullptr;

    std::vector<std::pair<D3D11_RASTERIZER_DESC,    Microsoft::WRL::ComPtr<ID3D11RasterizerState>>>   m_rs;
    std::vector<std::pair<D3D11_DEPTH_STENCIL_DESC, Microsoft::WRL::ComPtr<ID3D11DepthStencilState>>> m_ds;
    std::vector<std::pair<D3D11_BLEND_DESC,         Microsoft::WRL::ComPtr<ID3D11BlendState>>>        m_bs;
};

} // namespace SE
