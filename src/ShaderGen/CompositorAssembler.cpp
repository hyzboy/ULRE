#include <hgl/shadergen/CompositorAssembler.h>
#include <hgl/shadergen/CompositorTemplateRouter.h>
#include <hgl/shadergen/internal/GLSLSourceUtils.h>
#include <hgl/shadergen/CompositorFeatureFlags.h>
#include <hgl/shadergen/ShaderWriter.h>
#include <hgl/shadergen/ShaderLibraryPath.h>
#include <hgl/shadergen/ShaderRequirementSet.h>
#include <hgl/shadergen/VertexAttribMacroMap.h>
#include <hgl/shadergen/PositionProviderRegistry.h>
#include <hgl/shadergen/VertexPolicyRegistry.h>
#include <hgl/shadergen/FragmentProviderRegistry.h>
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
        if (f.has_direction) writer.EmitDefine("HAS_DIRECTION");

        // ── Axis 1: position provider ─────────────────────────────────────────
        // Each provider file declares its own VBO layout (or none) and exposes
        // vec3 GetPositionLocal().  DirectVec3 (empty path) falls back to the
        // legacy common/vertex_input_position.glsl path.
        {
            const hgl::graph::PositionProvider *pp =
                hgl::graph::FindBuiltinProvider(f.position_provider);

            if (pp && !pp->glsl_path.empty())
            {
                // Emit POSITION_LOCATION if the provider uses a VBO slot.
                // The emitter can extend this to emit set/binding for SSBO providers.
                writer.EmitInclude(std::string(pp->glsl_path));
            }
            else
            {
                // Fallback: legacy POSITION_KIND macro + common/vertex_input_position.glsl
                writer.EmitDefine("POSITION_KIND", "2");
                writer.EmitInclude("common/vertex_input_position.glsl");
            }
        }

        // ── Common UBOs (camera, transform, MI id) ────────────────────────────
        // Emit each base fragment directly; no macro-based prologue needed.
        if (f.needs_camera)    writer.EmitInclude("common/ubo_camera.glsl");
        if (f.needs_transform) writer.EmitInclude("common/ssbo_transform.glsl");
        // Material instance ID: emit the ID-only variant so GetMaterialInstanceID() is defined.
        writer.EmitDefine("MATERIAL_INSTANCE_ID_ONLY");
        writer.EmitInclude("common/ssbo_material_instance.glsl");

        // ── Axis 2: vertex policy ─────────────────────────────────────────────
        // Each policy file implements ApplyVertexTransform(local, out worldPos, out clipPos).
        {
            const hgl::graph::VertexPolicyDescriptor *vp =
                hgl::graph::FindBuiltinVertexPolicy(f.vertex_policy);

            if (vp && !vp->glsl_path.empty())
            {
                if (vp->needs_viewport)
                    writer.EmitInclude("common/ubo_viewport.glsl");

                writer.EmitInclude(vp->glsl_path.data());
            }
            else
            {
                // Unknown / unimplemented policy: emit a safe no-op so the shader
                // at least compiles (will produce a black/invisible object).
                writer.EmitCommentLine("WARNING: no vertex_policy glsl found; emitting identity passthrough");
                out += "void ApplyVertexTransform(vec3 l, out vec4 w, out vec4 c)"
                       "{ w=vec4(l,1.0); c=vec4(l,1.0); }\n";
            }
        }

        // ── Generic forward main (glue) ───────────────────────────────────────
        writer.EmitInclude("compositor/vert_forward_main.glsl");

        writer.EmitCommentLine("BuildForwardVertexEntry.End");
        return out;
    }

    std::string BuildForwardFragmentEntry(const hgl::graph::CompositorFeatureFlags &f,
                                          const hgl::graph::ShaderRequirementSet &req_set)
    {
        std::string out = "#version " + std::to_string(g_shader_version) + "\n\n";
        hgl::graph::ShaderWriter writer(out);

        writer.EmitCommentLine("BuildForwardFragmentEntry.Begin");

        EmitEnabledVertexAttribDefines(writer, f);

        if (f.enable_lighting)  writer.EmitDefine("ENABLE_LIGHTING");
        if (f.alpha_masked)     writer.EmitDefine("ALPHA_MODE_MASKED");
        if (f.alpha_dither)     writer.EmitDefine("ALPHA_MODE_DITHER");
        if (f.has_direction)    writer.EmitDefine("HAS_DIRECTION");
        if (f.has_clip_pos)     writer.EmitDefine("HAS_CLIP_POS");

        // ── Common UBOs: driven entirely by SFM surface+lighting requirements ──
        // req_set is pre-parsed from surface shader AND skylight includes, so
        // req_set.Requires("sky"/"camera") is the single source of truth here.
        if (req_set.Requires("sky"))
            writer.EmitInclude("common/ubo_sky.glsl");
        if (req_set.Requires("camera") || f.needs_camera)
            writer.EmitInclude("common/ubo_camera.glsl");

        // Shared struct definitions (SurfaceInput, SurfaceOutput, SurfaceOutputExt)
        writer.EmitInclude("common/surface_interface.glsl");
        // Varying declarations
        writer.EmitInclude("common/varying_fs.glsl");

        if (f.enable_lighting)
        {
            writer.EmitInclude(GetSkyLightGLSLPath(f.sky_ambient_model));
            writer.EmitInclude(hgl::graph::mtl::GetLightingModelGLSLPath(f.lighting_model));
        }

        // Fragment provider: if non-default, include the PCG/procedural provider
        // and set PCG_FRAGMENT_PROVIDER so frag_forward_main skips frag_input_resolve.
        const auto *frag_prov = hgl::graph::FindBuiltinFragmentProvider(f.fragment_provider);
        if (frag_prov && !frag_prov->glsl_path.empty())
        {
            if (frag_prov->needs_viewport)
                writer.EmitInclude("common/ubo_viewport.glsl");
            writer.EmitDefine("PCG_FRAGMENT_PROVIDER");
            writer.EmitInclude(std::string(frag_prov->glsl_path));
        }

        writer.EmitInclude(f.surface_path)
              .EmitInclude("compositor/frag_forward_main.glsl");

        writer.EmitCommentLine("BuildForwardFragmentEntry.End");
        return out;
    }

    // ─────────────────────────────────────────────────────────────────────────
    // Unified key-based VS/FS generators (replaces the old route-table system)
    // ─────────────────────────────────────────────────────────────────────────

    /// Map GeometryMode to VertexTransformPolicy explicitly — they do NOT share numeric values.
    hgl::graph::mtl::VertexTransformPolicy GeometryModeToVertexPolicy(hgl::graph::mtl::GeometryMode gm) noexcept
    {
        using GM = hgl::graph::mtl::GeometryMode;
        using VP = hgl::graph::mtl::VertexTransformPolicy;
        switch (gm)
        {
            case GM::Mesh3D:               return VP::Mesh3D;
            case GM::Quad2D:               return VP::Quad2D;
            case GM::ScreenRect:           return VP::Quad2D;   // ScreenRect = 2D quad in clip space
            case GM::BillboardCameraFacing:return VP::BillboardCameraFacing;
            case GM::BillboardAxisLocked:  return VP::BillboardAxisLocked;
            default:                       return VP::Mesh3D;
        }
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
         // position_provider is a runtime VS-only axis and must NOT be compared here:
         // the registry row always stores the preset default (e.g. DirectVec3) while the
         // key carries the runtime value (e.g. VAB_Vec2 for a D2 recipe). The assembler
         // reads the effective provider from the key, not from the row.
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

    hgl::graph::CompositorFeatureFlags VSFeatureFlagsFromRow(const hgl::graph::mtl::MaterialVariantRow &row,
                                                             hgl::graph::CoordinateSystem2D coord_2d = hgl::graph::CoordinateSystem2D::NDC,
                                                             std::optional<hgl::graph::PositionProviderId> key_position_provider = std::nullopt)
    {
        hgl::graph::CompositorFeatureFlags flags;
        // Runtime key overrides the row default for position_provider:
        // row stores the preset default (e.g. DirectVec3); key carries the effective
        // value from the recipe (e.g. VAB_Vec2 for dim=D2).
        flags.position_provider = key_position_provider.value_or(row.position_provider);

        for (size_t i = 0; i < static_cast<size_t>(hgl::graph::VertexAttrib::RANGE_SIZE); ++i)
        {
            const auto attrib = static_cast<hgl::graph::VertexAttrib>(i);
            if (row.vs_features.HasVertexAttrib(attrib))
                flags.SetVertexAttrib(attrib);
        }

        flags.has_direction   = row.vs_features.has_direction;
        flags.vertex_policy   = row.vertex_policy;
        flags.needs_camera    = row.resources.needs_camera;
        flags.needs_transform = row.resources.needs_transform;
        flags.coord_2d        = coord_2d;

        // If position_provider is VAB_Vec2 (2D input), derive vertex_policy directly from
        // coord_2d — the three 2D coordinate systems map 1:1 to concrete policies:
        //   NDC       → Position2DTransform  (per-instance L2W via SSBO)
        //   Ortho     → Position2DOrtho      (viewport ortho_matrix; no per-instance transform)
        //   ZeroToOne → Position2DZeroToOne  (linear remap; no per-instance transform)
        // This is authoritative regardless of what the row stored (e.g. a 3D-default row).
        using VP = hgl::graph::mtl::VertexTransformPolicy;
        using CS = hgl::graph::CoordinateSystem2D;
        const bool is_2d_input =
            key_position_provider.has_value()
                ? (*key_position_provider == hgl::graph::PositionProviderId::VAB_Vec2)
                : (row.position_provider  == hgl::graph::PositionProviderId::VAB_Vec2);
        if (is_2d_input)
        {
            VP policy = VP::Position2DTransform;
            if (coord_2d == CS::Ortho)
                policy = VP::Position2DOrtho;
            else if (coord_2d == CS::ZeroToOne)
                policy = VP::Position2DZeroToOne;

            flags.vertex_policy = policy;
            if (const auto *vp = hgl::graph::FindBuiltinVertexPolicy(policy))
            {
                flags.needs_camera    = vp->needs_camera;
                flags.needs_transform = vp->needs_transform;
            }
        }

        // Sky vertex shaders declare has_direction=true and use the sky vertex_policy,
        // which provides a direction vector instead of normal vertex attributes.
        // Clear vertex_attrib_bits so no spurious HAS_NORMAL / HAS_TEXCOORD defines
        // are emitted — driven by has_direction, not by surface_model identity.
        if (flags.has_direction)
            flags.vertex_attrib_bits = 0;

        return flags;
    }

    /// Legacy fallback VS generator
    std::string BuildLegacyVSFromKey(const hgl::graph::mtl::MaterialVariantKey &key)
    {
        // Terrain still uses a bespoke VS file (TODO: migrate to vertex_policy).
        if (key.surface_type == hgl::graph::SurfaceType::Terrain)
            return BuildIncludeOnlyVS("compositor/main_terrain_grid.vert.glsl");

        // All other cases (including Billboard*) now go through the two-axis
        // BuildForwardVertexEntry: position_provider selects GetPositionLocal(),
        // vertex_policy selects ApplyVertexTransform().
        return BuildForwardVertexEntry(LegacyVSFeatureFlagsFromKey(key));
    }

    std::string BuildVSFromRow(const hgl::graph::mtl::MaterialVariantRow &row,
                               hgl::graph::CoordinateSystem2D coord_2d = hgl::graph::CoordinateSystem2D::NDC,
                               std::optional<hgl::graph::PositionProviderId> key_position_provider = std::nullopt)
    {
        if (row.vs_template_path && row.vs_template_path[0])
            return BuildIncludeOnlyVS(row.vs_template_path);

        return BuildForwardVertexEntry(VSFeatureFlagsFromRow(row, coord_2d, key_position_provider));
    }

    /// Legacy key-derived FS feature inference: only used by non-builtin fallback assembly.
    hgl::graph::CompositorFeatureFlags LegacyFSFeatureFlagsFromKey(const hgl::graph::mtl::MaterialVariantKey &key,
                                                                   hgl::graph::RenderAlphaMode blend,
                                                                   const std::string &surface_path)
    {
        using ST = hgl::graph::SurfaceType;
        using RM = hgl::graph::RenderAlphaMode;
        using VA = hgl::graph::VertexAttrib;

        hgl::graph::CompositorFeatureFlags flags;
        flags.surface_path       = surface_path;
        flags.vertex_attrib_bits = key.vertex_attribute_feature_bits;

        if (blend == RM::Masked) flags.alpha_masked = true;
        if (blend == RM::Dither) flags.alpha_dither = true;

        // Billboard is no longer special-cased here: billboard geometry_mode is
        // handled entirely by the vertex_policy two-axis path (BillboardCameraFacing /
        // BillboardAxisLocked in VertexTransformPolicy). Any legacy key that reaches
        // this function with a billboard geometry_mode falls through to the standard
        // attrib-driven path below, which is correct for FS feature inference.

        // 1. Terrain: normal varying + clip-pos for grid edge fade.
        if (key.surface_type == ST::Terrain)
        {
            flags.SetVertexAttrib(VA::Normal);
            flags.has_clip_pos = true;
            return flags;
        }

        // 2. Sky: direction-based shading, no standard per-vertex varyings.
        // ubo_sky.glsl is now emitted via req_set.Requires("sky") in BuildForwardFragmentEntry.
        if (key.surface_type == ST::Sky)
        {
            flags.has_direction      = true;
            flags.vertex_attrib_bits = 0;
            return flags;
        }

        // 3. Lit 3D (not Unlit, not a 2D surface type).
        // needs_sky / sky_ambient_model are now SFM-driven: skylight_*.glsl declares
        // @sfm:require UBO sky, so sky binding is discovered automatically at compile time.
        if (key.surface_type != ST::Unlit && !hgl::graph::Is2DSurfaceType(key.surface_type))
        {
            flags.enable_lighting   = true;
            flags.lighting_model    = key.lighting_model;
            flags.needs_camera      = true;
            flags.sky_ambient_model = key.sky_ambient_model;
        }

        return flags;
    }

    hgl::graph::CompositorFeatureFlags FSFeatureFlagsFromRow(const hgl::graph::mtl::MaterialVariantKey &key,
                                                             const hgl::graph::mtl::MaterialVariantRow &row,
                                                             hgl::graph::RenderAlphaMode blend,
                                                             const std::string &surface_path)
    {
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

        flags.enable_lighting   = row.resources.enable_lighting;
        flags.needs_camera      = row.resources.needs_camera;
        // lighting_model / sky_ambient_model come from MaterialVariantKey (ECS-injected).
        // needs_sky is SFM-driven: skylight_*.glsl declares @sfm:require UBO sky.

        // Fragment provider: PCG_FullscreenTriangle preset pairs with PCG_FragCoord
        // so the FS derives its SurfaceInput from gl_FragCoord instead of varyings.
        if (row.position_provider == hgl::graph::PositionProviderId::PCG_FullscreenTriangle)
            flags.fragment_provider = hgl::graph::FragmentProviderId::PCG_FragCoord;
        else
            flags.fragment_provider = hgl::graph::FragmentProviderId::Default;

        if (row.fs_features.has_direction)
            flags.vertex_attrib_bits = 0;

        // If the surface shader itself requires the sky UBO (e.g. SkyMinimal accesses sky.*)
        return flags;
    }

    /// Legacy fallback FS generator: only used for non-builtin custom descriptors.
    std::string BuildLegacyFSFromKey(const hgl::graph::mtl::MaterialVariantKey &key,
                                     hgl::graph::RenderAlphaMode blend,
                                     const std::string &surface_path,
                                     const hgl::graph::ShaderRequirementSet &req_set)
    {
        return BuildForwardFragmentEntry(LegacyFSFeatureFlagsFromKey(key, blend, surface_path), req_set);
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

    // ─────────────────────────────────────────────────────────────────────────
    // SFM helpers: mirror the include chain from Build* to collect req_set
    // ─────────────────────────────────────────────────────────────────────────

    /// Collect VS resource requirements that match the include chain of BuildForwardVertexEntry.
    void CollectVSRequirements(const hgl::graph::CompositorFeatureFlags &f,
                               hgl::graph::ShaderRequirementSet &req_set,
                               const std::string &lib_path)
    {
        // position provider
        {
            const hgl::graph::PositionProvider *pp = hgl::graph::FindBuiltinProvider(f.position_provider);
            if (pp && !pp->glsl_path.empty())
                req_set.ParseFromGLSLFile(pp->glsl_path, lib_path);
        }

        // Collect camera / transform / material-instance requirements directly,
        // mirroring BuildForwardVertexEntry() which always includes these files.
        if (f.needs_camera)
            req_set.ParseFromGLSLFile("common/ubo_camera.glsl", lib_path);
        if (f.needs_transform)
            req_set.ParseFromGLSLFile("common/ssbo_transform.glsl", lib_path);
        // ssbo_material_instance.glsl is always emitted by BuildForwardVertexEntry
        // (with MATERIAL_INSTANCE_ID_ONLY). Parse its @sfm annotations so mbi_id
        // ends up in the merged manifest and MBI_ID_BINDING is emitted.
        req_set.ParseFromGLSLFile("common/ssbo_material_instance.glsl", lib_path);

        // vertex policy
        {
            const hgl::graph::VertexPolicyDescriptor *vp = hgl::graph::FindBuiltinVertexPolicy(f.vertex_policy);
            if (vp && !vp->glsl_path.empty())
            {
                if (vp->needs_viewport)
                    req_set.ParseFromGLSLFile("common/ubo_viewport.glsl", lib_path);
                req_set.ParseFromGLSLFile(vp->glsl_path, lib_path);
            }
        }
    }

    /// Collect FS resource requirements that match the include chain of BuildForwardFragmentEntry.
    /// Must stay in sync with BuildForwardFragmentEntry() — both use req_set.Requires() as the
    /// single source of truth so that binding contract and emitted GLSL always agree.
    void CollectFSRequirements(const hgl::graph::CompositorFeatureFlags &f,
                               hgl::graph::ShaderRequirementSet &req_set,
                               const std::string &lib_path)
    {
        // ── Step 1: parse the surface shader itself ───────────────────────────
        // Must come first so req_set.Requires("sky"/"camera") below reflects
        // any @sfm:require annotations declared directly in the surface file
        // (e.g. sky_minimal_surface.glsl → @sfm:require UBO sky).
        if (!f.surface_path.empty())
            req_set.ParseFromGLSLFile(f.surface_path, lib_path);

        // ── Step 2: lighting chain ────────────────────────────────────────────
        // skylight_*.glsl each carry @sfm:require UBO sky, so parsing them
        // populates req_set with the sky binding automatically.
        if (f.enable_lighting)
        {
            req_set.ParseFromGLSLFile(GetSkyLightGLSLPath(f.sky_ambient_model), lib_path);
            req_set.ParseFromGLSLFile(hgl::graph::mtl::GetLightingModelGLSLPath(f.lighting_model), lib_path);
        }

        // ── Step 3: emit UBOs driven by req_set — mirrors BuildForwardFragmentEntry ──
        // After Steps 1+2, req_set already reflects @sfm:require from surface AND
        // skylight chain, so Requires("sky"/"camera") is the sole decision point.
        if (req_set.Requires("sky"))
            req_set.ParseFromGLSLFile("common/ubo_sky.glsl", lib_path);
        if (req_set.Requires("camera") || f.needs_camera)
            req_set.ParseFromGLSLFile("common/ubo_camera.glsl", lib_path);

        // ── Step 4: fragment provider ─────────────────────────────────────────
        {
            const hgl::graph::FragmentProviderDescriptor *fp =
                hgl::graph::FindBuiltinFragmentProvider(f.fragment_provider);
            if (fp && !fp->glsl_path.empty())
            {
                if (fp->needs_viewport)
                    req_set.ParseFromGLSLFile("common/ubo_viewport.glsl", lib_path);
                req_set.ParseFromGLSLFile(fp->glsl_path, lib_path);
            }
        }
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
                                                         hgl::graph::CoordinateSystem2D coord_2d,
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
                out_source = BuildVSFromRow(*resolved_row, coord_2d, key.position_provider);
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
            // Build the full SFM requirement set that mirrors the include chain of
            // BuildForwardFragmentEntry, so that UBO emission (sky, camera) is driven
            // entirely by req_set.Requires() without needing enable_lighting fallbacks.
            //   1) surface shader declares its own @sfm:require (e.g. sky_minimal_surface.glsl)
            //   2) skylight_*.glsl declares @sfm:require UBO sky for lit surfaces
            //   3) ubo_sky / ubo_camera are then decided by req_set.Requires()
            hgl::graph::ShaderRequirementSet surface_req_set;
            if (!surface_rel.empty())
                surface_req_set.ParseFromGLSLFile(surface_rel, shader_lib_path_);

            if (resolved_row)
            {
                const auto fs_flags = FSFeatureFlagsFromRow(key, *resolved_row, key.blend_mode, surface_rel);
                if (fs_flags.enable_lighting)
                {
                    surface_req_set.ParseFromGLSLFile(GetSkyLightGLSLPath(fs_flags.sky_ambient_model), shader_lib_path_);
                    surface_req_set.ParseFromGLSLFile(hgl::graph::mtl::GetLightingModelGLSLPath(fs_flags.lighting_model), shader_lib_path_);
                }
                out_source = BuildForwardFragmentEntry(fs_flags, surface_req_set);
            }
            else
            {
                if (DescLooksBuiltinRouted(desc))
                {
                    out_error = BuildMissingRowMessage("FS", key, desc);
                    return false;
                }

                WarnLegacyKeyFallbackOnce("FS", key, desc);
                out_source = BuildLegacyFSFromKey(key, key.blend_mode, surface_rel, surface_req_set);
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
        const mtl::MaterialVariantDesc &desc,
        hgl::graph::CoordinateSystem2D  coord_2d
    ) const
    {
        return AssembleVertexShader(key, desc, nullptr, coord_2d);
    }

    CompositorAssembler::AssembleStageResult CompositorAssembler::AssembleVertexShader(
        const mtl::MaterialVariantKey  &key,
        const mtl::MaterialVariantDesc &desc,
        const mtl::MaterialVariantRow  *row,
        hgl::graph::CoordinateSystem2D  coord_2d
    ) const
    {
        AssembleStageResult result{};

        std::string source;
        std::string error;
        if(!AssembleVertexShaderSource(key, desc, row, coord_2d, source, error))
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

    CompositorAssembler::CompositorShaderArtifact CompositorAssembler::AssembleVertexArtifact(
        const mtl::MaterialVariantKey  &key,
        const mtl::MaterialVariantDesc &desc,
        const mtl::MaterialVariantRow  *row,
        hgl::graph::CoordinateSystem2D  coord_2d
    ) const
    {
        CompositorShaderArtifact artifact;

        const AssembleStageResult stage = AssembleVertexShader(key, desc, row, coord_2d);
        artifact.glsl          = stage.glsl;
        artifact.success       = stage.success;
        artifact.error_message = stage.error_message;

        if (artifact.success)
        {
            const mtl::MaterialVariantRow *resolved_row = ResolveVariantRow(key, desc, row);
            if (resolved_row)
            {
                if (!resolved_row->vs_template_path || !resolved_row->vs_template_path[0])
                {
                    // Standard two-axis path: collect from fragment/policy/position files
                    CollectVSRequirements(VSFeatureFlagsFromRow(*resolved_row, coord_2d, key.position_provider), artifact.req_set, shader_lib_path_);
                }
                else
                {
                    // Custom vs_path template: parse the template file itself for @sfm annotations
                    artifact.req_set.ParseFromGLSLFile(resolved_row->vs_template_path, shader_lib_path_);
                }
            }
        }

        return artifact;
    }

    CompositorAssembler::CompositorShaderArtifact CompositorAssembler::AssembleFragmentArtifact(
        const mtl::MaterialVariantKey  &key,
        const mtl::MaterialVariantDesc &desc,
        const mtl::MaterialVariantRow  *row
    ) const
    {
        CompositorShaderArtifact artifact;

        const mtl::MaterialVariantRow *resolved_row = ResolveVariantRow(key, desc, row);
        const std::string surface_rel = !desc.surface_function_path.empty()
            ? desc.surface_function_path
            : (resolved_row && resolved_row->surface_path && resolved_row->surface_path[0])
                ? resolved_row->surface_path
                : hgl::graph::GetSurfaceFunctionPath(key.surface_type);

        const AssembleStageResult stage = AssembleFragmentShader(key, desc, resolved_row);
        artifact.glsl          = stage.glsl;
        artifact.success       = stage.success;
        artifact.error_message = stage.error_message;

        if (artifact.success && resolved_row)
        {
            if (!resolved_row->fs_template_path || !resolved_row->fs_template_path[0])
            {
                // Standard two-axis path
                CollectFSRequirements(
                    FSFeatureFlagsFromRow(key, *resolved_row, key.blend_mode, surface_rel),
                    artifact.req_set, shader_lib_path_);
            }
            else
            {
                // Custom fs_path template: parse the template file for @sfm annotations
                artifact.req_set.ParseFromGLSLFile(resolved_row->fs_template_path, shader_lib_path_);
            }
        }

        return artifact;
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
