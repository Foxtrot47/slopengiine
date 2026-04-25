#pragma once
#include <DirectXMath.h>
#include "Engine/Scene/CameraComponent.h"

namespace SE {
class InputManager;

class ArcballController
{
public:
    DirectX::XMFLOAT3 target      = { 0.0f, 0.0f, 0.0f };
    float             distance    = 10.0f;
    float             yawDeg      =  0.0f;
    float             pitchDeg    = -15.0f;
    float             sensitivity =  0.3f;
    float             zoomSpeed   =  1.0f;

    void Update(const InputManager& input, CameraComponent& cam, bool mouseBlocked = false);
};

} // namespace SE
