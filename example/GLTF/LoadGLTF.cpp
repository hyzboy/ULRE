#include "GLTFLoader.h"

#include <iostream>
#include <filesystem>

int main(int argc, char** argv)
{
    std::filesystem::path path = "res/model/Duck.glb";
    if (argc >= 2) {
        path = std::filesystem::path(argv[1]);
    } else {
        std::cout << "Usage: 01_LoadGLTF <path-to-gltf-or-glb>\n";
        std::cout << "No path given, trying default: " << path << "\n";
    }

    if (!gltf::LoadAndDumpGLTF(path)) {
        std::cerr << "Failed to load: " << path << "\n";
        return 1;
    }
    return 0;
}
