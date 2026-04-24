file(GLOB_RECURSE GAME_SOURCES "src/*.cpp")

add_executable(Game WIN32 ${GAME_SOURCES})

target_link_libraries(Game PRIVATE SlopEngine)

target_compile_definitions(Game PRIVATE
    UNICODE
    _UNICODE
)

target_compile_options(Game PRIVATE
    /W4
    /WX
    /MP
)

set_target_properties(Game PROPERTIES OUTPUT_NAME "SlopGame")
