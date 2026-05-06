option(BUILD_SHARED_LIBS "Build shared libraries" ON)

file(GLOB_RECURSE ENGINE_SOURCES "src/*.cpp")

add_library(FoxEngine STATIC ${ENGINE_SOURCES})

target_include_directories(FoxEngine
    PUBLIC  include
    PRIVATE src
)

find_package(assimp CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(directxtex CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

target_link_libraries(FoxEngine PUBLIC
    d3d11.lib
    dxgi.lib
    d3dcompiler.lib
    dxguid.lib
    assimp::assimp
    imgui::imgui
    Microsoft::DirectXTex
    nlohmann_json::nlohmann_json
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
