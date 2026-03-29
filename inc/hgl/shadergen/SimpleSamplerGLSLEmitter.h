#pragma once

#include <string>
#include <hgl/common/ShaderStageDef.h>

namespace hgl::graph
{
class MaterialDescriptorInfo;

// Emit sampler2D declarations/getters for the given shader stage.
// This is the phase-1 simple path only: sampler2DArray/atlas stay on legacy GLSL.
std::string EmitSimpleSamplerGLSL(const MaterialDescriptorInfo &mdi, ShaderStage stage);

// Emit the MaterialInstanceTexture SSBO struct, buffer layout, getter, and
// _ULRE_InitTextureLayerIndices() — only for sampler slots that are sampler2DArray.
// Returns empty string when no array slots exist.
std::string EmitMaterialInstanceTextureGLSL(const MaterialDescriptorInfo &mdi, ShaderStage stage);
}