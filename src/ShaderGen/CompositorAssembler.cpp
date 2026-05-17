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
#include "BuiltinVariantEntry.h"
#include <algorithm>
#include <atomic>
#include <cstdio>

namespace
{
    #if defined(ULRE_SHADERGEN_VERBOSE)
    constexpr bool kCompositorAssemblerVerbose = true;
    #else
    constexpr bool kCompositorAssemblerVerbose = false;
    #endif

    // Thread-local shader version for #version directive (default 450)
    thread_local int g_shader_version = 450;

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

    std::string FormatVertexAttribBitsForLog(const uint32_t bits)
    {
        std::string text;
        bool first = true;

        for (size_t i = 0; i < static_cast<size_t>(hgl::graph::VertexAttrib::RANGE_SIZE); ++i)
        {
            const auto attrib = static_cast<hgl::graph::VertexAttrib>(i);
            if ((bits & (1u << static_cast<uint32_t>(attrib))) == 0)
                continue;

            if (!first)
                text += ",";

            const char *name = hgl::graph::GetVertexAttribName(attrib);
            text += name ? name : "<unnamed>";
            first = false;
        }

        return first ? "None" : text;
    }

    std::string FormatRowVSFeaturesForLog(const hgl::graph::mtl::MaterialVariantRow &row)
    {
        std::string text = FormatVertexAttribBitsForLog(0);
        bool has_any = false;
        text.clear();

        for (size_t i = 0; i < static_cast<size_t>(hgl::graph::VertexAttrib::RANGE_SIZE); ++i)
        {
            const auto attrib = static_cast<hgl::graph::VertexAttrib>(i);
            if (!row.vs_features.HasVertexAttrib(attrib))
                continue;

            if (has_any)
                text += ",";

            const char *name = hgl::graph::GetVertexAttribName(attrib);
            text += name ? name : "<unnamed>";
            has_any = true;
        }

        if (!has_any)
            text = "None";

        if (row.vs_features.has_direction)
            text += (text == "None" ? "Direction" : ",Direction");

        return text;
    }

    void LogVSAssemblyPath(const char *source_tag,
                           const hgl::graph::mtl::MaterialVariantKey &key,
                           const hgl::graph::mtl::MaterialVariantDesc &desc,
                           const hgl::graph::mtl::MaterialVariantRow *row)
    {
        if (!kCompositorAssemblerVerbose)
            return;

        if (row)
        {
            std::fprintf(stderr,
                         "[CompositorAssembler][VS] path=%s variant='%s' row='%s' policy=%s vs_template='%s' row_vs_features=[%s] key_va_bits=[%s] pos_provider=%u blend=%u pass=%u\n",
                         source_tag,
                         desc.variant_name.c_str(),
                         row->name ? row->name : "",
                         hgl::graph::mtl::GetVertexTransformPolicyName(row->vertex_policy),
                         row->vs_template_path ? row->vs_template_path : "",
                         FormatRowVSFeaturesForLog(*row).c_str(),
                         FormatVertexAttribBitsForLog(key.vertex_attribute_feature_bits).c_str(),
                         static_cast<unsigned>(key.position_provider),
                         static_cast<unsigned>(key.blend_mode),
                         static_cast<unsigned>(key.pass_hint));
            return;
        }

        std::fprintf(stderr,
                     "[CompositorAssembler][VS] path=%s variant='%s' row=<none> key_va_bits=[%s] pos_provider=%u blend=%u pass=%u\n",
                     source_tag,
                     desc.variant_name.c_str(),
                     FormatVertexAttribBitsForLog(key.vertex_attribute_feature_bits).c_str(),
                     static_cast<unsigned>(key.position_provider),
                     static_cast<unsigned>(key.blend_mode),
                     static_cast<unsigned>(key.pass_hint));
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
        std::string out = "#version " + std::to_string(g_shader_version) + "\n\n";
        hgl::graph::ShaderWriter writer(out);

        writer.EmitCommentLine("BuildForwardVertexEntry.Begin");

        EmitEnabledVertexAttribDefines(writer, f);

        // Map PositionProviderId -> GLSL POSITION_KIND (0=None/procedural, 1=Vec2, 2=Vec3)
        const int pos_kind = (f.position_provider == hgl::graph::PositionProviderId::VAB_Vec2) ? 1
                           : (f.position_provider == hgl::graph::PositionProviderId::PCG_FullscreenTriangle) ? 0
                           : 2; // DirectVec3, SSBO_PackedVec3, etc.
        writer.EmitDefine("POSITION_KIND", std::to_string(pos_kind).c_str());
        if (f.has_direction)    writer.EmitDefine("HAS_DIRECTION");

        writer.EmitInclude("common/vertex_input_position.glsl")
              .EmitInclude("compositor/vert_forward_ubo.glsl")
              .EmitInclude("compositor/vert_forward_main.glsl");

        writer.EmitCommentLine("BuildForwardVertexEntry.End");
        return out;
    }

    std::string BuildForwardFragmentEntry(const hgl::graph::CompositorFeatureFlags &f)
    {
        std::string out = "#version " + std::to_string(g_shader_version) + "\n\n";
        hgl::graph::ShaderWriter writer(out);

        writer.EmitCommentLine("BuildForwardFragmentEntry.Begin");

        EmitEnabledVertexAttribDefines(writer, f);

        if (f.enable_lighting)  writer.EmitDefine("ENABLE_LIGHTING");
        if (f.needs_camera)     writer.EmitDefine("NEEDS_CAMERA");
        if (f.needs_sky)        writer.EmitDefine("NEEDS_SKY");
        if (f.alpha_masked)     writer.EmitDefine("ALPHA_MODE_MASKED");
        if (f.alpha_dither)     writer.EmitDefine("ALPHA_MODE_DITHER");
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

    // ─────────────────────────────────────────────────────────────────────────
    // Unified key-based VS/FS generators (replaces the old route-table system)
    // ─────────────────────────────────────────────────────────────────────────

    /// Derive VS CompositorFeatureFlags from MaterialVariantKey fields.
    /// Does NOT cover Billboard / Terrain / Palette (handled as special cases in BuildVSFromKey).
    hgl::graph::CompositorFeatureFlags VSFeatureFlagsFromKey(const hgl::graph::mtl::MaterialVariantKey &key)
    {
        hgl::graph::CompositorFeatureFlags flags;
        flags.vertex_attrib_bits = key.vertex_attribute_feature_bits;

        // Propagate position_provider from key (already set correctly by routing layer).
        flags.position_provider = key.position_provider;
        if (key.surface_type == hgl::graph::SurfaceType::Sky)
        {
            flags.has_direction      = true;
            flags.vertex_attrib_bits = 0;
        }

        return flags;
    }

    /// Build a single-include VS wrapper: for geometry modes whose VS logic lives in a .glsl file.
    std::string BuildIncludeOnlyVS(const char *include_path)
    {
        std::string out = "#version " + std::to_string(g_shader_version) + "\n\n";
        hgl::graph::ShaderWriter(out).EmitInclude(include_path);
        return out;
    }

    std::string BuildIncludeOnlyShader(const char *include_path)
    {
        std::string out = "#version " + std::to_string(g_shader_version) + "\n\n";
        hgl::graph::ShaderWriter(out).EmitInclude(include_path);
        return out;
    }

    std::string BuildForwardUnlitPaletteVS(const hgl::graph::mtl::MaterialVariantKey &)
    {
        return BuildIncludeOnlyVS("compositor/main_forward_unlit_palette.vert.glsl");
    }

    const hgl::graph::mtl::MaterialVariantRow *FindBuiltinVariantRow(const hgl::graph::mtl::MaterialVariantDesc &desc)
    {
        if (desc.variant_name.empty())
            return nullptr;

        const auto *row = desc.bound_row;
        return row;
    }

    const hgl::graph::mtl::MaterialVariantRow *FindBuiltinVariantRowByLegacyKeyApproximation(const hgl::graph::mtl::MaterialVariantKey &key)
    {
        (void)key;
        return nullptr;
    }

    const char *GetStageTemplatePath(const std::string &desc_template_path,
                                     const hgl::graph::mtl::MaterialVariantRow *row,
                                     const bool is_vertex_stage)
    {
        if (!desc_template_path.empty())
            return desc_template_path.c_str();

        if (!row)
            return nullptr;

        const char *row_template_path = is_vertex_stage ? row->vs_template_path : row->fs_template_path;
        if (row_template_path && row_template_path[0])
            return row_template_path;

        return nullptr;
    }

    hgl::graph::CompositorFeatureFlags LegacyVSFeatureFlagsFromKey(const hgl::graph::mtl::MaterialVariantKey &key)
    {
        hgl::graph::CompositorFeatureFlags flags;
        flags.vertex_attrib_bits = key.vertex_attribute_feature_bits;

        flags.position_provider = key.position_provider;
        if (key.surface_type == hgl::graph::SurfaceType::Sky)
        {
            flags.has_direction      = true;
            flags.vertex_attrib_bits = 0;
        }

        return flags;
    }

    bool DescLooksBuiltinRouted(const hgl::graph::mtl::MaterialVariantDesc &desc) noexcept
    {
        return desc.factory_type.has_value();
    }

    bool DescAllowsLegacyKeyApproximation(const hgl::graph::mtl::MaterialVariantDesc &desc) noexcept
    {
        if (desc.bound_row)
            return false;

        if (DescLooksBuiltinRouted(desc))
            return false;

        if (!desc.variant_name.empty())
            return false;

        if (!desc.vs_template_path.empty() || !desc.fs_template_path.empty() || !desc.surface_function_path.empty())
            return false;

        return true;
    }

    void WarnLegacyKeyFallbackOnce(const char *stage,
                                   const hgl::graph::mtl::MaterialVariantKey &key,
                                   const hgl::graph::mtl::MaterialVariantDesc &desc)
    {
        static std::atomic_bool s_warned{false};
        bool expected = false;
        if (!s_warned.compare_exchange_strong(expected, true, std::memory_order_relaxed))
            return;

        std::fprintf(stderr,
                     "[CompositorAssembler] warning: using legacy key fallback for %s stage variant='%s' factory=%s surface=%u geometry=%u. "
                     "This path is compatibility-only; prefer CreateBuiltinRowBoundVariantDesc() or MaterialVariantDesc::CreateRowBound()/BindRow() with explicit MaterialVariantRow binding.\n",
                     stage,
                     desc.variant_name.empty() ? "<unnamed>" : desc.variant_name.c_str(),
                     desc.factory_type ? std::to_string(static_cast<unsigned>(*desc.factory_type)).c_str() : "<none>",
                     static_cast<unsigned>(key.surface_type),
                     static_cast<unsigned>(key.geometry_mode));
    }

    const hgl::graph::mtl::MaterialVariantRow *ResolveVariantRow(const hgl::graph::mtl::MaterialVariantKey &key,
                                                                 const hgl::graph::mtl::MaterialVariantDesc &desc,
                                                                 const hgl::graph::mtl::MaterialVariantRow *row)
    {
        if (row)
            return row;

        if (desc.bound_row)
            return desc.bound_row;

        if (const auto *named_row = FindBuiltinVariantRow(desc))
            return named_row;

        if (!DescAllowsLegacyKeyApproximation(desc))
            return nullptr;

        return FindBuiltinVariantRowByLegacyKeyApproximation(key);
    }

    std::string BuildMissingRowMessage(const char *stage,
                                       const hgl::graph::mtl::MaterialVariantKey &key,
                                       const hgl::graph::mtl::MaterialVariantDesc &desc)
    {
        std::string msg;
        msg.reserve(256);
        msg += "[CompositorAssembler] ";
        msg += stage;
        msg += " requires explicit MaterialVariantRow for builtin-routed variant='";
        msg += desc.variant_name.empty() ? "<unnamed>" : desc.variant_name;
        msg += "' factory=";
        msg += desc.factory_type ? std::to_string(static_cast<unsigned>(*desc.factory_type)) : std::string("<none>");
        msg += " surface=";
        msg += std::to_string(static_cast<unsigned>(key.surface_type));
        msg += " geometry=";
        msg += std::to_string(static_cast<unsigned>(key.geometry_mode));
        msg += ". Key fallback is reserved for legacy anonymous descriptors without explicit row/name/template binding; prefer CreateBuiltinRowBoundVariantDesc() or MaterialVariantDesc::CreateRowBound()/BindRow().";
        return msg;
    }

    bool ValidateBoundRowConsistency(const hgl::graph::mtl::MaterialVariantKey &key,
                                     const hgl::graph::mtl::MaterialVariantDesc &desc,
                                     std::string &out_error)
    {
        out_error.clear();

        const auto *row = desc.bound_row;
        if (!row)
            return true;

        if (desc.factory_type.has_value() && row->factory_type != *desc.factory_type)
        {
            out_error = "[CompositorAssembler] bound_row factory_type mismatch for variant='";
            out_error += desc.variant_name.empty() ? "<unnamed>" : desc.variant_name;
            out_error += "'";
            return false;
        }

        if (DescLooksBuiltinRouted(desc)
         && !desc.variant_name.empty()
         && row->name
         && desc.variant_name != row->name)
        {
            out_error = "[CompositorAssembler] builtin bound_row name mismatch for variant='";
            out_error += desc.variant_name;
            out_error += "' row='";
            out_error += row->name;
            out_error += "'";
            return false;
        }

        if (row->surface_type != key.surface_type
         || row->geometry_mode != key.geometry_mode
         || row->position_provider != key.position_provider
         || row->blend != key.blend_mode
         || row->pass != key.pass_hint)
        {
            out_error = "[CompositorAssembler] bound_row structural identity mismatches key for variant='";
            out_error += desc.variant_name.empty() ? "<unnamed>" : desc.variant_name;
            out_error += "'";
            return false;
        }

        return true;
    }

    hgl::graph::CompositorFeatureFlags VSFeatureFlagsFromRow(const hgl::graph::mtl::MaterialVariantRow &row)
    {
        hgl::graph::CompositorFeatureFlags flags;
        flags.position_provider = row.position_provider;

        for (size_t i = 0; i < static_cast<size_t>(hgl::graph::VertexAttrib::RANGE_SIZE); ++i)
        {
            const auto attrib = static_cast<hgl::graph::VertexAttrib>(i);
            if (row.vs_features.HasVertexAttrib(attrib))
                flags.SetVertexAttrib(attrib);
        }

        flags.has_direction = row.vs_features.has_direction;

        if (row.surface_model == hgl::graph::mtl::SurfaceShadingModel::SkyMinimal)
            flags.vertex_attrib_bits = 0;

        return flags;
    }

    /// Legacy fallback VS generator: only used for non-builtin custom descriptors.
    std::string BuildLegacyVSFromKey(const hgl::graph::mtl::MaterialVariantKey &key)
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

        // 3. All other materials: derive flags from key and generate via template.
        return BuildForwardVertexEntry(LegacyVSFeatureFlagsFromKey(key));
    }

    std::string BuildVSFromRow(const hgl::graph::mtl::MaterialVariantRow &row)
    {
        if (row.vs_template_path && row.vs_template_path[0])
            return BuildIncludeOnlyVS(row.vs_template_path);

        return BuildForwardVertexEntry(VSFeatureFlagsFromRow(row));
    }

    /// Legacy key-derived FS feature inference: only used by non-builtin fallback assembly.
    hgl::graph::CompositorFeatureFlags LegacyFSFeatureFlagsFromKey(const hgl::graph::mtl::MaterialVariantKey &key,
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

        // 4. Lit 3D (not Unlit, not a 2D surface type).
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

    hgl::graph::CompositorFeatureFlags FSFeatureFlagsFromRow(const hgl::graph::mtl::MaterialVariantKey &key,
                                                             const hgl::graph::mtl::MaterialVariantRow &row,
                                                             hgl::graph::RenderAlphaMode blend,
                                                             const std::string &surface_path)
    {
        using VIP = hgl::graph::mtl::VertexInputProfile;

        hgl::graph::CompositorFeatureFlags flags;
        flags.surface_path = surface_path;
        flags.vertex_attrib_bits = key.vertex_attribute_feature_bits;

        if (blend == hgl::graph::RenderAlphaMode::Masked) flags.alpha_masked = true;
        if (blend == hgl::graph::RenderAlphaMode::Dither) flags.alpha_dither = true;

        for (size_t i = 0; i < static_cast<size_t>(hgl::graph::VertexAttrib::RANGE_SIZE); ++i)
        {
            const auto attrib = static_cast<hgl::graph::VertexAttrib>(i);
            if (!row.fs_features.HasVertexAttrib(attrib))
                continue;

            if (attrib == hgl::graph::VertexAttrib::TexCoord)
            {
                flags.has_texcoord = true;
                continue;
            }

            flags.SetVertexAttrib(attrib);
        }

        flags.has_direction = row.fs_features.has_direction;
        flags.has_clip_pos = row.fs_features.has_clip_pos;

        flags.enable_lighting = row.resources.enable_lighting;
        flags.lighting_model = row.resources.lighting_model;
        flags.needs_camera = row.resources.needs_camera;
        flags.needs_sky = row.resources.needs_sky;
        flags.sky_ambient_model = row.resources.sky_model;

        if (row.fs_features.has_direction)
            flags.vertex_attrib_bits = 0;

        return flags;
    }

    /// Legacy fallback FS generator: only used for non-builtin custom descriptors.
    std::string BuildLegacyFSFromKey(const hgl::graph::mtl::MaterialVariantKey &key,
                                     hgl::graph::RenderAlphaMode blend,
                                     const std::string &surface_path)
    {
        return BuildForwardFragmentEntry(LegacyFSFeatureFlagsFromKey(key, blend, surface_path));
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

    hgl::graph::CompositorAssembler::AssembleResult MakeError(std::string message)
    {
        hgl::graph::CompositorAssembler::AssembleResult result;
        result.error_message = std::move(message);
        // success is already false by default
        return result;
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

    bool CompositorAssembler::ReadFileCached(const std::string &rel_path,
                                             std::string       &out_source,
                                             std::string       &out_error) const
    {
        const std::string full_path = shader_lib_path_ + "/" + rel_path;
        {
            std::lock_guard<std::mutex> lock(file_cache_mutex_);
            auto it = file_cache_.find(full_path);
            if (it != file_cache_.end())
            {
                out_source = it->second;
                return true;
            }
        }
        std::string source;
        if (!hgl::graph::internal::ReadTextFile(full_path, source, out_error))
            return false;

        std::lock_guard<std::mutex> lock(file_cache_mutex_);
        file_cache_.emplace(full_path, source);
        out_source = std::move(source);
        return true;
    }

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

        if (key.HasAnyTextureMode(mtl::TextureSourceMode::Array))
        {
            defines += "#define TEXTURE_ARRAY_MODE\n";
        }


        if(defines.empty())
            return source;

        return hgl::graph::internal::InjectAfterVersion(source, defines);
    }

    bool CompositorAssembler::AssembleVertexShaderSource(const mtl::MaterialVariantKey &key,
                                                         const mtl::MaterialVariantDesc &desc,
                                                         const mtl::MaterialVariantRow *row,
                                                         std::string &out_source,
                                                         std::string &out_error) const
    {
        out_source.clear();
        out_error.clear();

        if (!ValidateBoundRowConsistency(key, desc, out_error))
            return false;

        const mtl::MaterialVariantRow *resolved_row = ResolveVariantRow(key, desc, row);
        const char *vs_template_path = GetStageTemplatePath(desc.vs_template_path, resolved_row, true);

        if (vs_template_path)
        {
            LogVSAssemblyPath(desc.vs_template_path.empty() ? "row.vs_template_path" : "desc.vs_template_path",
                              key,
                              desc,
                              resolved_row);
            std::string read_error;
            if (!ReadFileCached(vs_template_path, out_source, read_error))
            {
                out_error = BuildReadFailureMessage(
                    "VS", vs_template_path, shader_lib_path_ + "/" + vs_template_path, read_error);
                return false;
            }

            if (hgl::graph::IsCompositorTemplatePath(vs_template_path))
                out_source = BuildIncludeOnlyShader(vs_template_path);
        }
        else
        {
            if (resolved_row)
            {
                LogVSAssemblyPath("explicit_row", key, desc, resolved_row);
                out_source = BuildVSFromRow(*resolved_row);
            }
            else
            {
                if (DescLooksBuiltinRouted(desc))
                {
                    out_error = BuildMissingRowMessage("VS", key, desc);
                    return false;
                }

                WarnLegacyKeyFallbackOnce("VS", key, desc);
                LogVSAssemblyPath("legacy_key_fallback", key, desc, nullptr);
                out_source = BuildLegacyVSFromKey(key);
            }
        }

        if (out_source.empty())
        {
            out_error = BuildPreprocessFailureMessage(
                "VS", desc.vs_template_path, "BuildVSFromKey produced empty source", out_source);
            return false;
        }

        return true;
    }

    bool CompositorAssembler::AssembleFragmentShaderSource(const mtl::MaterialVariantKey &key,
                                                           const mtl::MaterialVariantDesc &desc,
                                                           const mtl::MaterialVariantRow *row,
                                                           const std::string &surface_rel,
                                                           std::string &out_source,
                                                           std::string &out_error) const
    {
        out_source.clear();
        out_error.clear();

        if (!ValidateBoundRowConsistency(key, desc, out_error))
            return false;

        const mtl::MaterialVariantRow *resolved_row = ResolveVariantRow(key, desc, row);
        const char *fs_template_path = GetStageTemplatePath(desc.fs_template_path, resolved_row, false);

        if (fs_template_path)
        {
            std::string read_error;
            if (!ReadFileCached(fs_template_path, out_source, read_error))
            {
                out_error = BuildReadFailureMessage(
                    "FS", fs_template_path, shader_lib_path_ + "/" + fs_template_path, read_error);
                return false;
            }

            if (hgl::graph::IsCompositorTemplatePath(fs_template_path))
                out_source = BuildIncludeOnlyShader(fs_template_path);
        }
        else
        {
            if (resolved_row)
                out_source = BuildForwardFragmentEntry(FSFeatureFlagsFromRow(key, *resolved_row, key.blend_mode, surface_rel));
            else
            {
                if (DescLooksBuiltinRouted(desc))
                {
                    out_error = BuildMissingRowMessage("FS", key, desc);
                    return false;
                }

                WarnLegacyKeyFallbackOnce("FS", key, desc);
                out_source = BuildLegacyFSFromKey(key, key.blend_mode, surface_rel);
            }
        }

        if (out_source.empty())
        {
            out_error = BuildPreprocessFailureMessage(
                "FS", desc.fs_template_path, "BuildLegacyFSFromKey produced empty source", out_source);
            return false;
        }

        return true;
    }

    CompositorAssembler::AssembleStageResult CompositorAssembler::AssembleVertexShader(
        const mtl::MaterialVariantKey  &key,
        const mtl::MaterialVariantDesc &desc
    ) const
    {
        return AssembleVertexShader(key, desc, nullptr);
    }

    CompositorAssembler::AssembleStageResult CompositorAssembler::AssembleVertexShader(
        const mtl::MaterialVariantKey  &key,
        const mtl::MaterialVariantDesc &desc,
        const mtl::MaterialVariantRow  *row
    ) const
    {
        AssembleStageResult result{};

        std::string source;
        std::string error;
        if(!AssembleVertexShaderSource(key, desc, row, source, error))
        {
            result.error_message = std::move(error);
            return result;
        }

        result.glsl = InjectDefines(source, key);
        result.success = true;
        return result;
    }

    CompositorAssembler::AssembleStageResult CompositorAssembler::AssembleFragmentShader(
        const mtl::MaterialVariantKey  &key,
        const mtl::MaterialVariantDesc &desc
    ) const
    {
        return AssembleFragmentShader(key, desc, nullptr);
    }

    CompositorAssembler::AssembleStageResult CompositorAssembler::AssembleFragmentShader(
        const mtl::MaterialVariantKey  &key,
        const mtl::MaterialVariantDesc &desc,
        const mtl::MaterialVariantRow  *row
    ) const
    {
        AssembleStageResult result{};

        const mtl::MaterialVariantRow *resolved_row = ResolveVariantRow(key, desc, row);
        const std::string surface_rel = !desc.surface_function_path.empty()
            ? desc.surface_function_path
            : (resolved_row && resolved_row->surface_path && resolved_row->surface_path[0])
                ? resolved_row->surface_path
                : hgl::graph::GetSurfaceFunctionPath(key.surface_type);

        std::string source;
        std::string error;
        if(!AssembleFragmentShaderSource(key, desc, resolved_row, surface_rel, source, error))
        {
            result.error_message = std::move(error);
            return result;
        }

        result.glsl = InjectDefines(source, key);
        result.success = true;
        return result;
    }

    CompositorAssembler::AssembleResult CompositorAssembler::Assemble(
        const mtl::MaterialVariantKey  &key,
        const mtl::MaterialVariantDesc &desc
    ) const
    {
        return Assemble(key, desc, nullptr);
    }

    CompositorAssembler::AssembleResult CompositorAssembler::Assemble(
        const mtl::MaterialVariantKey  &key,
        const mtl::MaterialVariantDesc &desc,
        const mtl::MaterialVariantRow  *row
    ) const
    {
        AssembleResult result{};

        const AssembleStageResult vs_result = AssembleVertexShader(key, desc, row);
        if(!vs_result.success)
            return MakeError(vs_result.error_message);

        const AssembleStageResult fs_result = AssembleFragmentShader(key, desc, row);
        if(!fs_result.success)
            return MakeError(fs_result.error_message);

        result.vertex_glsl   = vs_result.glsl;
        result.fragment_glsl = fs_result.glsl;
        result.success       = true;
        return result;
    }

    std::span<const PassType> CompositorAssembler::GetPassTypesForBlendMode(RenderAlphaMode blend)
    {
        using PT = PassType;
        static constexpr PassType kOpaque[]          = { PT::ForwardOpaque, PT::ShadowOpaque, PT::EarlyZSolid };
        static constexpr PassType kMasked[]          = { PT::ForwardMasked, PT::ShadowMasked, PT::EarlyZMasked };
        static constexpr PassType kTransparent[]     = { PT::ForwardTransparent };
        static constexpr PassType kDither[]          = { PT::ForwardDither, PT::ShadowOpaque };
        static constexpr PassType kAlphaToCoverage[] = { PT::ForwardA2C, PT::ShadowMasked };

        switch (blend)
        {
        case RenderAlphaMode::Opaque:          return kOpaque;
        case RenderAlphaMode::Masked:          return kMasked;
        case RenderAlphaMode::Transparent:
            // 透明物体无阴影、无 EarlyZ（从后往前排序第 8 Pass 渲染）
            return kTransparent;
        case RenderAlphaMode::Dither:
            // Dither 小批目使用 ShadowOpaque（不需要 alpha 阴影）
            return kDither;
        case RenderAlphaMode::AlphaToCoverage:
            // A2C 阴影和 Masked 相同——需要 alpha discard 避免阴影漏光
            return kAlphaToCoverage;
        default:                               return kOpaque;
        }
    }
}
