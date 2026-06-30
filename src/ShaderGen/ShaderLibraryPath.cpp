#include <hgl/shadergen/ShaderLibraryPath.h>

namespace hgl::graph
{
    namespace
    {
        std::string g_shader_library_path = "ShaderLibrary";
    }

    const std::string &GetShaderLibraryPath()
    {
        return g_shader_library_path;
    }

    void SetShaderLibraryPath(const std::string &path)
    {
        if (path.empty())
            return;

        g_shader_library_path = path;
    }
}
