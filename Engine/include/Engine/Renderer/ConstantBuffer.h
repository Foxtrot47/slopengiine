#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <cstdint>
#include <cstring>
#include "Engine/Core/Logger.h"

using Microsoft::WRL::ComPtr;

namespace SE {

template<typename T>
class ConstantBuffer
{
public:
    bool Create(ID3D11Device* device)
    {
        // D3D11 requires cbuffer size to be a multiple of 16 bytes.
        constexpr UINT k_size = (sizeof(T) + 15u) & ~15u;

        D3D11_BUFFER_DESC bd = {};
        bd.Usage             = D3D11_USAGE_DYNAMIC;
        bd.ByteWidth         = k_size;
        bd.BindFlags         = D3D11_BIND_CONSTANT_BUFFER;
        bd.CPUAccessFlags    = D3D11_CPU_ACCESS_WRITE;

        HRESULT hr = device->CreateBuffer(&bd, nullptr, &m_buffer);
        if (FAILED(hr))
        {
            SE_LOG_ERROR("ConstantBuffer::Create failed: 0x%08X", hr);
            return false;
        }
        return true;
    }

    // Uploads new data to the GPU. Call once per frame before drawing.
    void Update(ID3D11DeviceContext* ctx, const T& data)
    {
        D3D11_MAPPED_SUBRESOURCE mapped = {};
        ctx->Map(m_buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        memcpy(mapped.pData, &data, sizeof(T));
        ctx->Unmap(m_buffer.Get(), 0);
    }

    void BindVS(ID3D11DeviceContext* ctx, uint32_t slot) const
    {
        ctx->VSSetConstantBuffers(slot, 1, m_buffer.GetAddressOf());
    }

    void BindPS(ID3D11DeviceContext* ctx, uint32_t slot) const
    {
        ctx->PSSetConstantBuffers(slot, 1, m_buffer.GetAddressOf());
    }

private:
    ComPtr<ID3D11Buffer> m_buffer;
};

} // namespace SE
