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

# Engine HLSL shaders
add_custom_command(TARGET Game POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/Engine/Shaders"
        "$<TARGET_FILE_DIR:Game>/Shaders"
    COMMENT "Copying Engine shaders"
)

# Bistro root files
add_custom_command(TARGET Game POST_BUILD
    COMMAND powershell -NoProfile -ExecutionPolicy Bypass -Command
        "New-Item -Force -ItemType Directory '$<TARGET_FILE_DIR:Game>/Assets/Bistro' | Out-Null; Get-ChildItem -Path '${CMAKE_SOURCE_DIR}/Assets/Bistro' -File | Copy-Item -Destination '$<TARGET_FILE_DIR:Game>/Assets/Bistro' -Force"
    COMMENT "Copying Assets"
)
