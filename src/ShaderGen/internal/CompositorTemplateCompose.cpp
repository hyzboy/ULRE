#include <hgl/shadergen/internal/CompositorTemplateCompose.h>

#include <hgl/shadergen/CompositorFeatureFlags.h>
#include <hgl/shadergen/ShaderWriter.h>
#include <hgl/shadergen/VertexAttribMacroMap.h>
#include <hgl/mtl/SkyLight.h>
#include <hgl/mtl/LightingModel.h>

namespace
{

using namespace hgl::graph;

static const char *GetSkyLightGLSLPath(const hgl::graph::mtl::SkyLightAmbientModel model)
{
    using hgl::graph::mtl::SkyLightAmbientModel;
    switch (model)
    {
        case SkyLightAmbientModel::Simple:             return "common/skylight_simple.glsl";
        case SkyLightAmbientModel::FakeAtmosphere:     return "common/skylight_fake_atm.glsl";
        case SkyLightAmbientModel::CubeMap:            return "common/skylight_cubemap.glsl";
        case SkyLightAmbientModel::SphericalHarmonics: return "common/skylight_sh.glsl";
        case SkyLightAmbientModel::IBL:                return "common/skylight_ibl.glsl";
        default:                                       return "common/skylight_simple.glsl";
    }
}

void EmitEnabledVertexAttribDefines(hgl::graph::ShaderWriter &writer,
                                    const hgl::graph::CompositorFeatureFlags &flags)
{
    for (size_t i = 0; i < static_cast<size_t>(hgl::graph::VertexAttrib::RANGE_SIZE); ++i)
    {
        const auto attrib = static_cast<hgl::graph::VertexAttrib>(i);

        if (flags.HasVertexAttrib(attrib))
            hgl::graph::EmitVertexAttribDefine(writer, attrib);
    }
}

std::string BuildForwardVertexEntry(const hgl::graph::CompositorFeatureFlags &f,
                                    const int shader_version)
{
    std::string out = "#version " + std::to_string(shader_version) + "\n\n";
    hgl::graph::ShaderWriter writer(out);

    writer.EmitCommentLine("BuildForwardVertexEntry.Begin");

    EmitEnabledVertexAttribDefines(writer, f);

    // Map PositionProviderId -> GLSL POSITION_KIND (0=None/procedural, 1=Vec2, 2=Vec3)
    // SSBO_PackedVec3 uses kind 0: vertex_fetch_ssbo.glsl provides FetchPosition().
    const int pos_kind = (f.position_provider == hgl::graph::PositionProviderId::VAB_Vec2) ? 1
                       : (f.position_provider == hgl::graph::PositionProviderId::PCG_FullscreenTriangle) ? 0
                       : (f.position_provider == hgl::graph::PositionProviderId::SSBO_PackedVec3) ? 0
                       : 2; // DirectVec3 and other VBO providers
    writer.EmitDefine("POSITION_KIND", std::to_string(pos_kind).c_str());
    if (f.has_direction) writer.EmitDefine("HAS_DIRECTION");

    // Phase C: emit GEOMETRY_FETCH_SSBO + per-attribute SSBO binding macros.
    bool any_ssbo = (f.position_provider == hgl::graph::PositionProviderId::SSBO_PackedVec3);
    for (const auto &p : f.attribute_providers)
        if (p != hgl::graph::AttributeProviderId::None) { any_ssbo = true; break; }

    if (any_ssbo)
    {
        writer.EmitDefine("GEOMETRY_FETCH_SSBO", "1");
        if (f.position_provider == hgl::graph::PositionProviderId::SSBO_PackedVec3)
        {
            writer.EmitDefine("POSITION_SSBO_SET", "VERTEXSTREAMS_SET");
            writer.EmitDefine("POSITION_SSBO_BINDING",
                std::to_string(hgl::graph::GetPositionSSBOBinding()).c_str());
        }
        for (size_t i = 0; i < f.attribute_providers.size(); ++i)
        {
            const auto semantic = static_cast<hgl::graph::AttributeSemantic>(i);
            if (f.GetAttributeProvider(semantic) != hgl::graph::AttributeProviderId::None)
            {
                const char *tag = hgl::graph::GetAttributeSemanticMacroTag(semantic);
                if (!tag)
                    continue;

                std::string macro = "FETCH_";
                macro += tag;
                macro += "_SSBO_BINDING";
                writer.EmitDefine(macro.c_str(), std::to_string(i).c_str());
            }
        }
    }

    writer.EmitInclude("common/vertex_input_position.glsl")
          .EmitInclude("compositor/vert_forward_ubo.glsl")
          .EmitInclude("compositor/vert_forward_main.glsl");

    writer.EmitCommentLine("BuildForwardVertexEntry.End");
    return out;
}

std::string BuildForwardFragmentEntry(const hgl::graph::CompositorFeatureFlags &f,
                                      const int shader_version)
{
    std::string out = "#version " + std::to_string(shader_version) + "\n\n";
    hgl::graph::ShaderWriter writer(out);

    writer.EmitCommentLine("BuildForwardFragmentEntry.Begin");

    EmitEnabledVertexAttribDefines(writer, f);

    if (f.enable_lighting) writer.EmitDefine("ENABLE_LIGHTING");
    if (f.needs_camera) writer.EmitDefine("NEEDS_CAMERA");
    if (f.needs_sky) writer.EmitDefine("NEEDS_SKY");
    if (f.alpha_masked) writer.EmitDefine("ALPHA_MODE_MASKED");
    if (f.alpha_dither) writer.EmitDefine("ALPHA_MODE_DITHER");
    if (f.has_texcoord) writer.EmitDefine("HAS_BILLBOARD_TEXCOORD");
    if (f.has_direction) writer.EmitDefine("HAS_DIRECTION");
    if (f.has_clip_pos) writer.EmitDefine("HAS_CLIP_POS");

    writer.EmitInclude("compositor/frag_forward_ubo.glsl");

    if (f.needs_sky)
        writer.EmitInclude(GetSkyLightGLSLPath(f.sky_ambient_model));

    if (f.enable_lighting)
        writer.EmitInclude(hgl::graph::mtl::GetLightingModelGLSLPath(f.lighting_model));

    writer.EmitInclude(f.surface_path)
          .EmitInclude("compositor/frag_forward_main.glsl");

    writer.EmitCommentLine("BuildForwardFragmentEntry.End");
    return out;
}

// Derive VS CompositorFeatureFlags from MaterialVariantKey fields.
CompositorFeatureFlags VSFeatureFlagsFromKey(const hgl::graph::mtl::MaterialVariantKey &key)
{
    CompositorFeatureFlags flags;
    flags.vertex_attrib_bits = key.vertex_attribute_feature_bits;

    // Propagate position_provider from key (already set correctly by routing layer).
    flags.position_provider = key.position_provider;

    // Phase C: propagate per-semantic attribute providers for SSBO pulling.
    flags.attribute_providers = key.attribute_providers;

    if (key.surface_type == hgl::graph::SurfaceType::Sky)
    {
        flags.has_direction = true;
        flags.vertex_attrib_bits = 0;
    }

    return flags;
}

std::string BuildIncludeOnlyVS(const char *include_path,
                               const int shader_version)
{
    std::string out = "#version " + std::to_string(shader_version) + "\n\n";
    hgl::graph::ShaderWriter(out).EmitInclude(include_path);
    return out;
}

std::string BuildForwardUnlitPaletteVS(const hgl::graph::mtl::MaterialVariantKey &,
                                       const int shader_version)
{
    return BuildIncludeOnlyVS("compositor/main_forward_unlit_palette.vert.glsl", shader_version);
}

std::string BuildVSFromKey(const hgl::graph::mtl::MaterialVariantKey &key,
                           const int shader_version)
{
    using GM = hgl::graph::mtl::GeometryMode;

    // 1. Billboard geometry modes: delegate to pre-built VS files.
    if (key.geometry_mode == GM::BillboardCameraFacing)
        return BuildIncludeOnlyVS("compositor/main_forward_billboard_dynamic.vert.glsl", shader_version);
    if (key.geometry_mode == GM::BillboardAxisLocked)
        return BuildIncludeOnlyVS("compositor/main_forward_billboard_fixed.vert.glsl", shader_version);

    // 2. Terrain: delegate to terrain VS file.
    if (key.surface_type == hgl::graph::SurfaceType::Terrain)
        return BuildIncludeOnlyVS("compositor/main_terrain_grid.vert.glsl", shader_version);

    // 3. VertexPaletteColor: Color vertex attrib + DebugShading.
    if (key.IsDebugShading() && key.HasVertexAttrib(hgl::graph::VertexAttrib::Color))
        return BuildForwardUnlitPaletteVS(key, shader_version);

    // 4. All other materials: derive flags from key and generate via template.
    return BuildForwardVertexEntry(VSFeatureFlagsFromKey(key), shader_version);
}

// Derive FS CompositorFeatureFlags from MaterialVariantKey fields.
CompositorFeatureFlags FSFeatureFlagsFromKey(const hgl::graph::mtl::MaterialVariantKey &key,
                                             hgl::graph::RenderAlphaMode blend,
                                             const std::string &surface_path)
{
    using ST = hgl::graph::SurfaceType;
    using GM = hgl::graph::mtl::GeometryMode;
    using RM = hgl::graph::RenderAlphaMode;
    using VA = hgl::graph::VertexAttrib;

    CompositorFeatureFlags flags;
    flags.surface_path = surface_path;
    flags.vertex_attrib_bits = key.vertex_attribute_feature_bits;

    if (blend == RM::Masked) flags.alpha_masked = true;
    if (blend == RM::Dither) flags.alpha_dither = true;

    // 1. Billboard: texcoord-based, no standard per-vertex varyings.
    if (key.geometry_mode == GM::BillboardCameraFacing
     || key.geometry_mode == GM::BillboardAxisLocked)
    {
        flags.has_texcoord = true;
        flags.vertex_attrib_bits = 0;
        return flags;
    }

    // 2. Terrain: normal varying + clip-pos for grid edge fade.
    if (key.surface_type == ST::Terrain)
    {
        flags.SetVertexAttrib(VA::Normal);
        flags.has_clip_pos = true;
        return flags;
    }

    // 3. Sky: direction-based shading, no standard per-vertex varyings.
    if (key.surface_type == ST::Sky)
    {
        flags.has_direction = true;
        flags.vertex_attrib_bits = 0;
        return flags;
    }

    // 4. Gizmo (DebugShading + no Color attrib): normal-based debug shading, needs camera UBO.
    if (key.IsDebugShading() && !key.HasVertexAttrib(VA::Color))
    {
        flags.needs_camera = true;
        return flags;
    }

    // 5. Lit 3D (not Unlit, not a 2D surface type).
    if (key.surface_type != ST::Unlit && !hgl::graph::Is2DSurfaceType(key.surface_type))
    {
        flags.enable_lighting = true;
        flags.lighting_model = key.lighting_model;
        flags.needs_camera = true;
        flags.needs_sky = true;
        flags.sky_ambient_model = key.sky_ambient_model;
    }

    return flags;
}

std::string BuildFSFromKey(const hgl::graph::mtl::MaterialVariantKey &key,
                           hgl::graph::RenderAlphaMode blend,
                           const std::string &surface_path,
                           const int shader_version)
{
    return BuildForwardFragmentEntry(FSFeatureFlagsFromKey(key, blend, surface_path), shader_version);
}

} // namespace

namespace hgl::graph::internal
{

std::string BuildVertexTemplateFromKey(const mtl::MaterialVariantKey &key,
                                       const int shader_version)
{
    return BuildVSFromKey(key, shader_version);
}

std::string BuildFragmentTemplateFromKey(const mtl::MaterialVariantKey &key,
                                         const RenderAlphaMode blend,
                                         const std::string &surface_path,
                                         const int shader_version)
{
    return BuildFSFromKey(key, blend, surface_path, shader_version);
}

} // namespace hgl::graph::internal
