#pragma once

#include <string>
#include <hgl/common/ShaderStageDef.h>

namespace hgl::graph
{
class MaterialDescriptorDB;

// Emit the MaterialInstanceTexture SSBO struct, buffer layout, getter functions
// (GetMITLayer_*), and GetMaterialInstanceID() — only for sampler2DArray slots.
// Returns empty string when no array slots exist.
std::string EmitMaterialInstanceTextureGLSL(const MaterialDescriptorDB &mdi, ShaderStage stage);
}
