#include <hgl/shadergen/CompositorTemplateRouter.h>
#include <hgl/shadergen/RegistryQuery.h>
#include <hgl/mtl/SurfaceType.h>

namespace hgl::graph {

namespace {

struct SurfaceFunctionRoute
{
    SurfaceType  surface;
    const char  *path;
};

static const SurfaceFunctionRoute kSurfaceFunctionRoutes[] = {
    {SurfaceType::PureColor2D,  "surface/unlit_color3d_surface.glsl"},
    {SurfaceType::VertexColor2D,"surface/unlit_vertexcolor_surface.glsl"},
    {SurfaceType::PureTexture2D,"surface/2d/puretexture2d_surface.glsl"},
    {SurfaceType::Text2D,       "surface/2d/text2d_surface.glsl"},
    {SurfaceType::Standard,     "surface/standard_surface.glsl"},
    {SurfaceType::Unlit,        "surface/unlit_color3d_surface.glsl"},
    {SurfaceType::Skin,         "surface/skin_surface.glsl"},
    {SurfaceType::Hair,         "surface/hair_surface.glsl"},
    {SurfaceType::Cloth,        "surface/cloth_surface.glsl"},
    {SurfaceType::Eye,          "surface/eye_surface.glsl"},
    {SurfaceType::Foliage,      "surface/foliage_surface.glsl"},
    {SurfaceType::ClearCoat,    "surface/clearcoat_surface.glsl"},
    {SurfaceType::Water,        "surface/water_surface.glsl"},
    {SurfaceType::Terrain,      "surface/terrain_surface.glsl"},
    {SurfaceType::Sky,          "surface/sky_surface.glsl"},
};

} // anonymous namespace

VertexRouteResult RouteVertexTemplate(const mtl::MaterialVariantKey &key)
{
    VertexRouteResult result;
    result.tpl = mtl::FindVertexProgramTemplate(key, &result.miss_reason);
    return result;
}

FragmentRouteResult RouteFragmentTemplate(const mtl::MaterialVariantKey &key)
{
    FragmentRouteResult result;
    result.tpl = mtl::FindSurfaceFragmentTemplate(key, &result.miss_reason);
    return result;
}

PipelineStateResult RoutePipelineState(const mtl::MaterialVariantKey &key)
{
    PipelineStateResult result;
    result.row = mtl::FindPipelineStateRow(key, &result.miss_reason);
    return result;
}

bool IsCompositorTemplatePath(std::string_view template_path)
{
    return template_path.substr(0, 11) == "compositor/";
}

std::string GetSurfaceFunctionPath(SurfaceType surface)
{
    for (const auto &route : kSurfaceFunctionRoutes)
    {
        if (route.surface == surface)
            return route.path;
    }
    return "surface/standard_surface.glsl";
}

} // namespace hgl::graph
