#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <cstdint>

using Microsoft::WRL::ComPtr;

namespace SE {

class IndexBuffer
{
public:
    bool Create(ID3D11Device* device, const uint32_t* indices, uint32_t count);

    void Bind(ID3D11DeviceContext* ctx) const;

    uint32_t GetCount() const { return m_count; }

private:
    ComPtr<ID3D11Buffer> m_buffer;
    uint32_t             m_count = 0;
};

} // namespace SE
