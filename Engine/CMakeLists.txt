file(GLOB_RECURSE ENGINE_SOURCES "src/*.cpp")

add_library(SlopEngine STATIC ${ENGINE_SOURCES})

target_include_directories(SlopEngine
    PUBLIC  include
    PRIVATE src
)

find_package(imgui CONFIG REQUIRED)

target_link_libraries(SlopEngine PUBLIC
    d3d11.lib
    dxgi.lib
    d3dcompiler.lib
    dxguid.lib
    assimp::assimp
    imgui::imgui
)

target_compile_definitions(SlopEngine PUBLIC
    UNICODE
    _UNICODE
    $<$<CONFIG:Debug>:SE_DEBUG>
    $<$<CONFIG:Release>:SE_RELEASE>
    $<$<CONFIG:RelWithDebInfo>:SE_PROFILE>
)

target_compile_options(SlopEngine PRIVATE
    /W4
    /WX
    /MP
)
