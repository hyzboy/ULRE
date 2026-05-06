#pragma once

#include <hgl/mtl/MaterialVariantKey.h>
#include <hgl/mtl/RenderAlphaMode.h>

#include <string>

namespace hgl::graph::internal
{

// Compose a vertex-stage compositor GLSL entry source from a material variant key.
std::string BuildVertexTemplateFromKey(const mtl::MaterialVariantKey &key,
                                       int shader_version = 450);

// Compose a fragment-stage compositor GLSL entry source from a material variant key.
std::string BuildFragmentTemplateFromKey(const mtl::MaterialVariantKey &key,
                                         RenderAlphaMode blend,
                                         const std::string &surface_path,
                                         int shader_version = 450);

} // namespace hgl::graph::internal
