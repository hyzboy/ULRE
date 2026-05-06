#pragma once

#include <hgl/mtl/MaterialVariantKey.h>

#include <string>

namespace hgl::graph::internal {

// Inject key-derived preprocessor defines into compositor GLSL after #version.
std::string InjectCompositorKeyDefines(const std::string &source,
                                       const mtl::MaterialVariantKey &key);

} // namespace hgl::graph::internal
