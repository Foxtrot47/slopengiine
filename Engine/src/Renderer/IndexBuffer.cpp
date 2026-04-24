#include "Engine/Renderer/IndexBuffer.h"
#include "Engine/Core/Logger.h"

namespace SE {

bool IndexBuffer::Create(ID3D11Device* device, const uint32_t* indices, uint32_t count)
{
    m_count = count;

    D3D11_BUFFER_DESC bd = {};
    bd.Usage             = D3D11_USAGE_IMMUTABLE;
    bd.ByteWidth         = sizeof(uint32_t) * count;
    bd.BindFlags         = D3D11_BIND_INDEX_BUFFER;

    D3D11_SUBRESOURCE_DATA init = {};
    init.pSysMem = indices;

    HRESULT hr = device->CreateBuffer(&bd, &init, &m_buffer);
    if (FAILED(hr))
    {
        SE_LOG_ERROR("IndexBuffer::Create failed: 0x%08X", hr);
        return false;
    }
    return true;
}

void IndexBuffer::Bind(ID3D11DeviceContext* ctx) const
{
    ctx->IASetIndexBuffer(m_buffer.Get(), DXGI_FORMAT_R32_UINT, 0);
}

} // namespace SE
