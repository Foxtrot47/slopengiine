#pragma once
#include <DirectXMath.h>
#include <vector>
#include <algorithm>
#include <cstdint>

namespace SE {

struct RenderItem
{
    DirectX::XMMATRIX  model;
    uint32_t           meshIndex;     // index into an external mesh/material array
    uint32_t           subMeshIndex;
    float              sortDepth;     // camera-space Z for sorting
    bool               transparent;
};

class RenderQueue
{
public:
    void Clear() { m_items.clear(); }

    void Push(const RenderItem& item) { m_items.push_back(item); }

    // Sort: opaque front-to-back (lower depth first), transparent back-to-front (higher depth first).
    void Sort()
    {
        std::sort(m_items.begin(), m_items.end(),
            [](const RenderItem& a, const RenderItem& b)
            {
                if (a.transparent != b.transparent)
                    return !a.transparent; // opaques first
                if (a.transparent)
                    return a.sortDepth > b.sortDepth; // back-to-front
                return a.sortDepth < b.sortDepth;     // front-to-back
            });
    }

    const std::vector<RenderItem>& Items() const { return m_items; }
    size_t Size() const { return m_items.size(); }

private:
    std::vector<RenderItem> m_items;
};

} // namespace SE
