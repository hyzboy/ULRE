#pragma once

#include <hgl/mtl/MaterialVariantKey.h>
#include <hgl/mtl/PipelineStateRow.h>
#include <hgl/mtl/SurfaceFragmentTemplate.h>
#include <hgl/mtl/SurfaceType.h>
#include <hgl/mtl/VertexProgramTemplate.h>
#include <string>
#include <string_view>

namespace hgl::graph {

struct VertexRouteResult
{
    const mtl::VertexProgramTemplate *tpl = nullptr;
    std::string miss_reason;
};

struct FragmentRouteResult
{
    const mtl::SurfaceFragmentTemplate *tpl = nullptr;
    std::string miss_reason;
};

struct PipelineStateResult
{
    const mtl::PipelineStateRow *row = nullptr;
    std::string miss_reason;
};

VertexRouteResult RouteVertexTemplate(const mtl::MaterialVariantKey &key);
FragmentRouteResult RouteFragmentTemplate(const mtl::MaterialVariantKey &key);
PipelineStateResult RoutePipelineState(const mtl::MaterialVariantKey &key);

/// Returns true if template_path has the "compositor/" prefix,
/// indicating it should be handled by key-derived generation rather than disk load.
bool IsCompositorTemplatePath(std::string_view template_path);

/// Returns the relative GLSL path for the surface function corresponding to surface.
/// Falls back to "surface/standard_surface.glsl" for unrecognised types.
std::string GetSurfaceFunctionPath(SurfaceType surface);

} // namespace hgl::graph
