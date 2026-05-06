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

# Copy pre-existing DDS files from source to build directory
add_custom_target(CopyPreExistingAssets
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/Assets"
        "${CMAKE_BINARY_DIR}/Assets"
    COMMENT "Copying source Assets to build directory"
)

# Texture conversion — converts non-DDS textures (TGA, PNG, HDR, EXR) to BC-compressed DDS
# Output goes to CMAKE_BINARY_DIR/Assets (build directory, alongside pre-existing DDS)
add_custom_target(ConvertTextures
    COMMAND powershell -NoProfile -ExecutionPolicy Bypass -File
        "${CMAKE_SOURCE_DIR}/Tools/ConvertTextures.ps1"
        -SourceDir "${CMAKE_SOURCE_DIR}/Assets"
        -OutputDir "${CMAKE_BINARY_DIR}/Assets"
        -ToolPath "${CMAKE_SOURCE_DIR}/Tools/texconv.exe"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    COMMENT "Converting textures to DDS..."
    DEPENDS CopyPreExistingAssets
)

# Copy final Assets (pre-existing + converted) to runtime directory
add_custom_command(TARGET Game POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_BINARY_DIR}/Assets"
        "$<TARGET_FILE_DIR:Game>/Assets"
    COMMENT "Copying Assets to runtime directory"
    DEPENDS ConvertTextures
)

add_dependencies(Game ConvertTextures)
