#include "Engine/Scene/TransformComponent.h"
#include <algorithm>

namespace SE {

void TransformComponent::SetParent(TransformComponent* newParent)
{
    Unparent();
    parent = newParent;
    if (parent)
        parent->children.push_back(this);
}

void TransformComponent::Unparent()
{
    if (!parent) return;
    auto& siblings = parent->children;
    siblings.erase(std::remove(siblings.begin(), siblings.end(), this), siblings.end());
    parent = nullptr;
}

} // namespace SE
