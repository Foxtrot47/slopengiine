#include <windows.h>
#include "Engine/Core/Engine.h"

int WINAPI WinMain(HINSTANCE /*hInstance*/, HINSTANCE /*hPrevInstance*/,
                   LPSTR /*lpCmdLine*/, int /*nCmdShow*/)
{
    SE::Engine engine;

    if (!engine.Initialize())
        return 1;

    engine.Shutdown();
    return 0;
}
