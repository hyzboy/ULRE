#pragma once

#include <string>

namespace hgl::graph
{
class ShaderCreateInfo;

// Emit sampler2D declarations/getters for the given shader stage.
// This is the phase-1 simple path only: sampler2DArray/atlas stay on legacy GLSL.
std::string EmitSimpleSamplerGLSL(const ShaderCreateInfo &shader);
}