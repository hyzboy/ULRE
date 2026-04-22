#pragma once

#include <hgl/mtl/SurfaceType.h>
#include <string>
#include <string_view>

namespace hgl::graph {

/// Returns true if template_path has the "compositor/" prefix,
/// indicating it should be handled by key-derived generation rather than disk load.
bool IsCompositorTemplatePath(std::string_view template_path);

/// Returns the relative GLSL path for the surface function corresponding to surface.
/// Falls back to "surface/standard_surface.glsl" for unrecognised types.
std::string GetSurfaceFunctionPath(SurfaceType surface);

} // namespace hgl::graph
