file(GLOB_RECURSE ENGINE_SOURCES "src/*.cpp")

add_library(FoxEngine STATIC ${ENGINE_SOURCES})

target_include_directories(FoxEngine
    PUBLIC  include
    PRIVATE src
)

find_package(imgui CONFIG REQUIRED)

target_link_libraries(FoxEngine PUBLIC
    d3d11.lib
    dxgi.lib
    d3dcompiler.lib
    dxguid.lib
    assimp::assimp
    imgui::imgui
)

target_compile_definitions(FoxEngine PUBLIC
    UNICODE
    _UNICODE
    $<$<CONFIG:Debug>:SE_DEBUG>
    $<$<CONFIG:Release>:SE_RELEASE>
    $<$<CONFIG:RelWithDebInfo>:SE_PROFILE>
)

target_compile_options(FoxEngine PRIVATE
    /W4
    /WX
    /MP
)
