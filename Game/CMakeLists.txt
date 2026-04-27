file(GLOB_RECURSE GAME_SOURCES "src/*.cpp")

add_executable(Game WIN32 ${GAME_SOURCES})

target_link_libraries(Game PRIVATE FoxEngine)

target_compile_definitions(Game PRIVATE
    UNICODE
    _UNICODE
)

target_compile_options(Game PRIVATE
    /W4
    /WX
    /MP
)

set_target_properties(Game PROPERTIES
    OUTPUT_NAME "TestGame"
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:Game>"
)

# Engine-internal shaders — part of the engine, not user assets.
add_custom_command(TARGET Game POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/Engine/Shaders"
        "$<TARGET_FILE_DIR:Game>/Shaders"
    COMMENT "Copying Engine shaders"
)

# User-facing runtime assets (textures, models, audio, etc.)
add_custom_command(TARGET Game POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/Assets"
        "$<TARGET_FILE_DIR:Game>/Assets"
    COMMENT "Copying Assets"
)
