#include <hgl/shadergen/CompositorAssembler.h>
#include <hgl/shadergen/CompositorTemplateRouter.h>
#include <hgl/shadergen/internal/GLSLSourceUtils.h>
#include <hgl/shadergen/CompositorFeatureFlags.h>
#include <hgl/shadergen/ShaderWriter.h>
#include <hgl/shadergen/ShaderLibraryPath.h>
#include <hgl/shadergen/VertexAttribMacroMap.h>
#include <hgl/mtl/MaterialVariantDesc.h>
#include <hgl/mtl/MaterialVariantKey.h>
#include <hgl/mtl/SkyLight.h>
#include <hgl/mtl/LightingModel.h>
#include <algorithm>
#include <cstdio>

namespace
{
    static const char *GetSkyLightGLSLPath(const hgl::graph::mtl::SkyLightAmbientModel model)
    {
        using hgl::graph::mtl::SkyLightAmbientModel;
        switch (model)
        {
            case SkyLightAmbientModel::Simple:              return "common/skylight_simple.glsl";
            case SkyLightAmbientModel::FakeAtmosphere:      return "common/skylight_fake_atm.glsl";
            case SkyLightAmbientModel::CubeMap:             return "common/skylight_cubemap.glsl";
            case SkyLightAmbientModel::SphericalHarmonics:  return "common/skylight_sh.glsl";
            case SkyLightAmbientModel::IBL:                 return "common/skylight_ibl.glsl";
            default:                                        return "common/skylight_simple.glsl";
        }
    }

    void EmitEnabledVertexAttribDefines(hgl::graph::ShaderWriter &writer, const hgl::graph::CompositorFeatureFlags &flags)
    {
        for(size_t i = 0; i < static_cast<size_t>(hgl::graph::VertexAttrib::RANGE_SIZE); ++i)
        {
            const auto attrib = static_cast<hgl::graph::VertexAttrib>(i);

            if(flags.HasVertexAttrib(attrib))
                hgl::graph::EmitVertexAttribDefine(writer, attrib);
        }
    }

    std::string BuildForwardVertexEntry(const hgl::graph::CompositorFeatureFlags &f)
    {
        std::string out = "#version 450\n\n";
        hgl::graph::ShaderWriter writer(out);

        writer.EmitCommentLine("BuildForwardVertexEntry.Begin");

        EmitEnabledVertexAttribDefines(writer, f);

        if (f.vert_input_2d)    writer.EmitDefine("VERT_INPUT_2D");
        if (f.has_direction)    writer.EmitDefine("HAS_DIRECTION");

        writer.EmitInclude("compositor/vert_forward_ubo.glsl")
              .EmitInclude("compositor/vert_forward_main.glsl");

        writer.EmitCommentLine("BuildForwardVertexEntry.End");
        return out;
    }

    std::string BuildForwardFragmentEntry(const hgl::graph::CompositorFeatureFlags &f)
    {
        std::string out = "#version 450\n\n";
        hgl::graph::ShaderWriter writer(out);

        writer.EmitCommentLine("BuildForwardFragmentEntry.Begin");

        EmitEnabledVertexAttribDefines(writer, f);

        if (f.enable_lighting)  writer.EmitDefine("ENABLE_LIGHTING");
        if (f.needs_camera)     writer.EmitDefine("NEEDS_CAMERA");
        if (f.needs_sky)        writer.EmitDefine("NEEDS_SKY");
        if (f.alpha_masked)     writer.EmitDefine("ALPHA_MODE_MASKED");
        if (f.alpha_dither)     writer.EmitDefine("ALPHA_MODE_DITHER");
        if (f.has_texcoord)     writer.EmitDefine("HAS_BILLBOARD_TEXCOORD");
        if (f.has_direction)    writer.EmitDefine("HAS_DIRECTION");
        if (f.has_clip_pos)     writer.EmitDefine("HAS_CLIP_POS");

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

    std::string BuildForwardUnlitPaletteVS(const hgl::graph::mtl::MaterialVariantKey &)
    {
        std::string out = "#version 450\n\n";
        hgl::graph::ShaderWriter writer(out);
        writer.EmitCommentLine("BuildForwardUnlitPaletteVS.Begin");
        writer.EmitInclude("compositor/vert_forward_ubo.glsl")
              .EmitInclude("common/ubo_color_palette.glsl")
              .NewLine()
              .EmitLine("layout(location=POSITION_LOCATION) in vec3 Position;")
              .EmitLine("layout(location=COLOR_LOCATION) in uint ColorIndex;")
              .NewLine()
              .EmitDefine("VARYING_STAGE_VERT")
              .EmitDefine("HAS_COLOR")
              .EmitInclude("common/varying_interface.glsl")
              .NewLine()
              .EmitLine("void main()")
              .BeginBlock()
                  .EmitLine("fragMaterialInstanceID = GetMaterialInstanceID();")
                  .EmitLine("mat4 transform_mat = GetTransform();")
                  .EmitLine("vec4 worldPos = transform_mat * vec4(Position, 1.0);")
                  .NewLine()
                  .EmitLine("fragVertexColor = unpackUnorm4x8(color_palette.color[ColorIndex]);")
                  .NewLine()
                  .EmitLine("gl_Position = camera.vp * worldPos;")
              .EndBlock();
        writer.EmitCommentLine("BuildForwardUnlitPaletteVS.End");
        return out;
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Unified key-based VS/FS generators (replaces the old route-table system)
    // ─────────────────────────────────────────────────────────────────────────

    /// Derive VS CompositorFeatureFlags from MaterialVariantKey fields.
    /// Does NOT cover Billboard / Terrain / Palette (handled as special cases in BuildVSFromKey).
    hgl::graph::CompositorFeatureFlags VSFeatureFlagsFromKey(const hgl::graph::mtl::MaterialVariantKey &key)
    {
        hgl::graph::CompositorFeatureFlags flags;
        flags.vertex_attrib_bits = key.vertex_attribute_feature_bits;

        if (hgl::graph::Is2DSurfaceType(key.surface_type))
        {
            flags.vert_input_2d = true;
        }
        else if (key.surface_type == hgl::graph::SurfaceType::Sky)
        {
            flags.has_direction      = true;
            flags.vertex_attrib_bits = 0;
        }
        // Vec2 position: 3D-space mesh whose Position vertex attrib is vec2 (e.g. VertexLuminance2D).
        // Set by PopulateVariantKeyVertexAttribBits when it detects a VAT_VEC2 Position entry.
        else if (key.IsVec2Position())
        {
            flags.vert_input_2d = true;
        }

        return flags;
    }

    /// Build a single-include VS wrapper: for geometry modes whose VS logic lives in a .glsl file.
    std::string BuildIncludeOnlyVS(const char *include_path)
    {
        std::string out = "#version 450\n\n";
        hgl::graph::ShaderWriter(out).EmitInclude(include_path);
        return out;
    }

    /// Unified VS generator: derives complete GLSL from MaterialVariantKey fields alone.
    std::string BuildVSFromKey(const hgl::graph::mtl::MaterialVariantKey &key)
    {
        using GM = hgl::graph::mtl::GeometryMode;

        // 1. Billboard geometry modes: delegate to pre-built VS files.
        if (key.geometry_mode == GM::BillboardCameraFacing)
            return BuildIncludeOnlyVS("compositor/main_forward_billboard_dynamic.vert.glsl");
        if (key.geometry_mode == GM::BillboardAxisLocked)
            return BuildIncludeOnlyVS("compositor/main_forward_billboard_fixed.vert.glsl");

        // 2. Terrain: delegate to terrain VS file.
        if (key.surface_type == hgl::graph::SurfaceType::Terrain)
            return BuildIncludeOnlyVS("compositor/main_terrain_grid.vert.glsl");

        // 3. VertexPaletteColor: Color vertex attrib + DebugShading.
        //    TODO (improvement item 6): move inline GLSL to a .glsl file and remove this special case.
        if (key.IsDebugShading() && key.HasVertexAttrib(hgl::graph::VertexAttrib::Color))
            return BuildForwardUnlitPaletteVS(key);

        // 4. All other materials: derive flags from key and generate via template.
        return BuildForwardVertexEntry(VSFeatureFlagsFromKey(key));
    }

    /// Derive FS CompositorFeatureFlags from MaterialVariantKey fields.
    hgl::graph::CompositorFeatureFlags FSFeatureFlagsFromKey(const hgl::graph::mtl::MaterialVariantKey &key,
                                                              hgl::graph::RenderAlphaMode blend,
                                                              const std::string &surface_path)
    {
        using ST = hgl::graph::SurfaceType;
        using GM = hgl::graph::mtl::GeometryMode;
        using RM = hgl::graph::RenderAlphaMode;
        using VA = hgl::graph::VertexAttrib;

        hgl::graph::CompositorFeatureFlags flags;
        flags.surface_path       = surface_path;
        flags.vertex_attrib_bits = key.vertex_attribute_feature_bits;

        if (blend == RM::Masked) flags.alpha_masked = true;
        if (blend == RM::Dither) flags.alpha_dither = true;

        // 1. Billboard: texcoord-based, no standard per-vertex varyings.
        if (key.geometry_mode == GM::BillboardCameraFacing
         || key.geometry_mode == GM::BillboardAxisLocked)
        {
            flags.has_texcoord       = true;
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
            flags.has_direction      = true;
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
            flags.enable_lighting   = true;
            flags.lighting_model    = key.lighting_model;
            flags.needs_camera      = true;
            flags.needs_sky         = true;
            flags.sky_ambient_model = key.sky_ambient_model;
        }

        return flags;
    }

    /// Unified FS generator: derives complete GLSL from MaterialVariantKey fields alone.
    std::string BuildFSFromKey(const hgl::graph::mtl::MaterialVariantKey &key,
                               hgl::graph::RenderAlphaMode blend,
                               const std::string &surface_path)
    {
        return BuildForwardFragmentEntry(FSFeatureFlagsFromKey(key, blend, surface_path));
    }

    std::string BuildReadFailureMessage(const char *stage,
                                                const std::string &template_path,
                                                const std::string &file_path,
                                                const std::string &reason)
    {
        std::string msg;
        msg.reserve(256 + reason.size());
        msg += "[CompositorAssembler] ";
        msg += stage;
        msg += " template load failed. template=";
        msg += template_path.empty() ? "<auto-route>" : template_path;
        msg += " file=";
        msg += file_path;
        msg += " reason=";
        msg += reason;
        return msg;
    }

    std::string BuildPreprocessFailureMessage(const char *stage,
                                                      const std::string &template_path,
                                                      const std::string &detail,
                                                      const std::string &glsl_source)
    {
        std::string msg;
        msg.reserve(384 + detail.size() + std::min<size_t>(glsl_source.size(), 2048));
        msg += "[CompositorAssembler] ";
        msg += stage;
        msg += " preprocess failed. template=";
        msg += template_path.empty() ? "<auto-route>" : template_path;
        msg += " detail=";
        msg += detail;
        msg += "\n[";
        msg += stage;
        msg += " GLSL first 80 lines]\n";
        msg += hgl::graph::internal::BuildGLSLPreviewFirstLines(glsl_source, 80);
        return msg;
    }
}

namespace hgl::graph
{
    CompositorAssembler::CompositorAssembler()
        : CompositorAssembler(GetShaderLibraryPath())
    {}

    CompositorAssembler::CompositorAssembler(const std::string &shader_library_path)
        : shader_lib_path_(shader_library_path)
    {}

    std::string CompositorAssembler::InjectDefines(const std::string &source, const mtl::MaterialVariantKey &key) const
    {
        std::string defines;
        {
            char buf[128] = {};
            const uint32 shadow_mode = 0u;
            std::snprintf(buf,
                          sizeof(buf),
                          "#define SURFACE_TYPE %d\n"
                          "#define SHADOW_MODE %u\n",
                          static_cast<int>(key.surface_type),
                          shadow_mode);
            defines += buf;
        }

        for (uint8 i = 0; i < uint8(mtl::SamplerSlotCount); ++i)
        {
            const mtl::SamplerSlot slot = static_cast<mtl::SamplerSlot>(i);
            if (key.GetTextureSourceMode(slot) == mtl::TextureSourceMode::Array)
            {
                defines += "#define TEXTURE_ARRAY_MODE\n";
                break;
            }
        }

        if(defines.empty())
            return source;

        return hgl::graph::internal::InjectAfterVersion(source, defines);
    }

    CompositorAssembler::AssembleResult CompositorAssembler::Assemble(
        const mtl::MaterialVariantKey  &key,
        const mtl::MaterialVariantDesc &desc
    ) const
    {
        AssembleResult result{};

        const std::string surface_rel = desc.surface_function_path.empty()
            ? hgl::graph::GetSurfaceFunctionPath(key.surface_type)
            : desc.surface_function_path;

        // VS: non-compositor custom path (e.g. 2D shader files) → ReadTextFile;
        //     empty or compositor/ prefix → key-derived generation.
        std::string vs_source;
        if (!desc.vs_template_path.empty() && !hgl::graph::IsCompositorTemplatePath(desc.vs_template_path))
        {
            const std::string full_path = shader_lib_path_ + "/" + desc.vs_template_path;
            std::string read_error;
            if (!hgl::graph::internal::ReadTextFile(full_path, vs_source, read_error))
            {
                result.error_message = BuildReadFailureMessage(
                    "VS", desc.vs_template_path, full_path, read_error);
                result.success = false;
                return result;
            }
        }
        else
        {
            vs_source = BuildVSFromKey(key);
        }

        // FS: same routing logic.
        std::string fs_source;
        if (!desc.fs_template_path.empty() && !hgl::graph::IsCompositorTemplatePath(desc.fs_template_path))
        {
            const std::string full_path = shader_lib_path_ + "/" + desc.fs_template_path;
            std::string read_error;
            if (!hgl::graph::internal::ReadTextFile(full_path, fs_source, read_error))
            {
                result.error_message = BuildReadFailureMessage(
                    "FS", desc.fs_template_path, full_path, read_error);
                result.success = false;
                return result;
            }
        }
        else
        {
            fs_source = BuildFSFromKey(key, key.blend_mode, surface_rel);
        }

        if (vs_source.empty())
        {
            result.error_message = BuildPreprocessFailureMessage(
                "VS", desc.vs_template_path, "BuildVSFromKey produced empty source", vs_source);
            result.success = false;
            return result;
        }

        if (fs_source.empty())
        {
            result.error_message = BuildPreprocessFailureMessage(
                "FS", desc.fs_template_path, "BuildFSFromKey produced empty source", fs_source);
            result.success = false;
            return result;
        }

        vs_source = InjectDefines(vs_source, key);
        fs_source = InjectDefines(fs_source, key);

        result.vertex_glsl   = std::move(vs_source);
        result.fragment_glsl = std::move(fs_source);
        result.success       = true;
        return result;
    }

    std::vector<PassType> CompositorAssembler::GetPassTypesForBlendMode(RenderAlphaMode blend)
    {
        switch (blend)
        {
        case RenderAlphaMode::Opaque:
            return { PassType::ForwardOpaque, PassType::ShadowOpaque, PassType::EarlyZSolid };

        case RenderAlphaMode::Masked:
            return { PassType::ForwardMasked, PassType::ShadowMasked, PassType::EarlyZMasked };

        case RenderAlphaMode::Transparent:
            // 透明物体无阴影、无 EarlyZ（从后往前排序第 8 Pass 渲染）
            return { PassType::ForwardTransparent };

        case RenderAlphaMode::Dither:
            // Dither 小批目使用 ShadowOpaque（不需要 alpha 阴影）
            return { PassType::ForwardDither, PassType::ShadowOpaque };

        case RenderAlphaMode::AlphaToCoverage:
            // A2C 阴影和 Masked 相同——需要 alpha discard 避免阴影漏光
            return { PassType::ForwardA2C, PassType::ShadowMasked };

        default:
            return { PassType::ForwardOpaque };
        }
    }
}
