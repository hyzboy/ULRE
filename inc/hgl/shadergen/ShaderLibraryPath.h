#pragma once

#include <string>

namespace hgl::graph
{
    // Global ShaderLibrary root used by ShaderGen components.
    // Default value is "ShaderLibrary" to preserve existing behavior.
    const std::string &GetShaderLibraryPath();

    // Overrides global ShaderLibrary root.
    // Empty input is ignored.
    void SetShaderLibraryPath(const std::string &path);
}
