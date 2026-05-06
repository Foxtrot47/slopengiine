#pragma once
#include <string>
#include <vector>
#include "Engine/Scene/SceneDescriptor.h"

namespace SE {

class SceneLoader
{
public:
    // Load a scene descriptor from a JSON file. Returns true on success.
    static bool LoadFromFile(const std::string& path, SceneDescriptor& out);

    // Scan a directory for .json scene files. Returns list of file paths.
    static std::vector<std::string> ScanSceneDirectory(const std::string& directory);
};

} // namespace SE
