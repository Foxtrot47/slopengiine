#include "Engine/Renderer/RenderStateCache.h"
#include "Engine/Core/Logger.h"
#include <cstring>

namespace SE {

void RenderStateCache::Init(ID3D11Device* device)
{
    m_device = device;
}

void RenderStateCache::Clear()
{
    m_rs.clear();
    m_ds.clear();
    m_bs.clear();
}

ID3D11RasterizerState* RenderStateCache::GetRasterizerState(const D3D11_RASTERIZER_DESC& desc)
{
    for (auto& [d, s] : m_rs)
        if (memcmp(&d, &desc, sizeof(desc)) == 0)
            return s.Get();

    Microsoft::WRL::ComPtr<ID3D11RasterizerState> s;
    SE_HR(m_device->CreateRasterizerState(&desc, &s));
    if (!s) return nullptr;
    m_rs.push_back({ desc, std::move(s) });
    return m_rs.back().second.Get();
}

ID3D11DepthStencilState* RenderStateCache::GetDepthStencilState(const D3D11_DEPTH_STENCIL_DESC& desc)
{
    for (auto& [d, s] : m_ds)
        if (memcmp(&d, &desc, sizeof(desc)) == 0)
            return s.Get();

    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> s;
    SE_HR(m_device->CreateDepthStencilState(&desc, &s));
    if (!s) return nullptr;
    m_ds.push_back({ desc, std::move(s) });
    return m_ds.back().second.Get();
}

ID3D11BlendState* RenderStateCache::GetBlendState(const D3D11_BLEND_DESC& desc)
{
    for (auto& [d, s] : m_bs)
        if (memcmp(&d, &desc, sizeof(desc)) == 0)
            return s.Get();

    Microsoft::WRL::ComPtr<ID3D11BlendState> s;
    SE_HR(m_device->CreateBlendState(&desc, &s));
    if (!s) return nullptr;
    m_bs.push_back({ desc, std::move(s) });
    return m_bs.back().second.Get();
}

} // namespace SE
