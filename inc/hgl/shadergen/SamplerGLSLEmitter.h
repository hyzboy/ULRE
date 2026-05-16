#pragma once

#include <string>
#include <hgl/common/ShaderStageDef.h>

namespace hgl::graph
{
class MaterialDescriptorDB;

// Emit sampler2D declarations/getters for the given shader stage.
// Covers the unified simple-sampler path; array slots are handled separately
// by EmitMaterialInstanceTextureGLSL below.
std::string EmitSimpleSamplerGLSL(const MaterialDescriptorDB &mdi, ShaderStage stage);

// Emit the MaterialInstanceTexture SSBO struct, buffer layout, getter, and
// _ULRE_InitTextureLayerIndices() — only for sampler slots that are sampler2DArray.
// Returns empty string when no array slots exist.
std::string EmitMaterialInstanceTextureGLSL(const MaterialDescriptorDB &mdi, ShaderStage stage);
}
