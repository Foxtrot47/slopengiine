file(GLOB_RECURSE ENGINE_SOURCES "src/*.cpp")

add_library(SlopEngine STATIC ${ENGINE_SOURCES})

target_include_directories(SlopEngine
    PUBLIC  include
    PRIVATE src
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
