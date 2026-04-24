#include <windows.h>
#include "Engine/Core/Engine.h"

int WINAPI WinMain(HINSTANCE /*hInstance*/, HINSTANCE /*hPrevInstance*/,
                   LPSTR /*lpCmdLine*/, int /*nCmdShow*/)
{
    SE::WindowDesc desc;
    desc.title  = L"SlopEngine";
    desc.width  = 1280;
    desc.height = 720;

    SE::Engine engine;
    if (!engine.Initialize(desc))
        return 1;

    engine.Run();
    engine.Shutdown();
    return 0;
}
