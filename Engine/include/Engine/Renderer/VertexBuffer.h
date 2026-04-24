#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <cstdint>

using Microsoft::WRL::ComPtr;

namespace SE {

class VertexBuffer
{
public:
    // data     : raw vertex data
    // byteSize : total size in bytes
    // stride   : size of one vertex in bytes
    bool Create(ID3D11Device* device, const void* data, uint32_t byteSize, uint32_t stride);

    void Bind(ID3D11DeviceContext* ctx, uint32_t slot = 0) const;

    uint32_t GetStride() const { return m_stride; }

private:
    ComPtr<ID3D11Buffer> m_buffer;
    uint32_t             m_stride = 0;
};

} // namespace SE
