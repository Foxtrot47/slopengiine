#include "Engine/Renderer/VertexBuffer.h"
#include "Engine/Core/Logger.h"

namespace SE {

bool VertexBuffer::Create(ID3D11Device* device, const void* data,
                           uint32_t byteSize, uint32_t stride)
{
    m_stride = stride;

    D3D11_BUFFER_DESC bd = {};
    bd.Usage             = D3D11_USAGE_IMMUTABLE;
    bd.ByteWidth         = byteSize;
    bd.BindFlags         = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA init = {};
    init.pSysMem = data;

    HRESULT hr = device->CreateBuffer(&bd, &init, &m_buffer);
    if (FAILED(hr))
    {
        SE_LOG_ERROR("VertexBuffer::Create failed: 0x%08X", hr);
        return false;
    }
    return true;
}

void VertexBuffer::Bind(ID3D11DeviceContext* ctx, uint32_t slot) const
{
    UINT offset = 0;
    ctx->IASetVertexBuffers(slot, 1, m_buffer.GetAddressOf(), &m_stride, &offset);
}

} // namespace SE
