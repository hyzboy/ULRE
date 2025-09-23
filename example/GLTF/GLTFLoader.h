#pragma once

#include <filesystem>

namespace gltf {
    // Load a glTF/glb and dump key information to the console
    // Returns true on success
    bool LoadAndDumpGLTF(const std::filesystem::path& filePath);
}
