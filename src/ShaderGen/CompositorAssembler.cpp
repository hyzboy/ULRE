#include <hgl/shadergen/CompositorAssembler.h>
#include <hgl/shadergen/CompositorFeatureFlags.h>
#include <hgl/shadergen/ShaderWriter.h>
#include <hgl/shadergen/ShaderLibraryPath.h>
#include <hgl/shadergen/VertexAttribMacroMap.h>
#include <hgl/mtl/MaterialVariantDesc.h>
#include <hgl/mtl/MaterialVariantKey.h>
#include <hgl/mtl/SkyLight.h>
#include <hgl/mtl/LightingModel.h>
#include <atomic>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstdio>

namespace
{
#if defined(ULRE_SHADERGEN_STRICT_GENERATED_TEMPLATES)
    constexpr bool kStrictGeneratedTemplateRoutes = true;
#else
    constexpr bool kStrictGeneratedTemplateRoutes = false;
#endif

    using GeneratedVSBuilder = std::string (*)(const hgl::graph::mtl::MaterialVariantKey &key);
    using GeneratedFSBuilder = std::string (*)(const hgl::graph::mtl::MaterialVariantKey &key, hgl::graph::RenderAlphaMode blend, const std::string &surface_path);

    struct GeneratedVSTemplateRoute
    {
        const char *template_path;
        GeneratedVSBuilder builder;
    };

    struct GeneratedFSTemplateRoute
    {
        const char *template_path;
        GeneratedFSBuilder builder;
    };

    struct SurfaceFunctionRoute
    {
        hgl::graph::SurfaceType surface;
        const char *path;
    };

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

    std::string BuildBillboardDynamicVertexEntry(const hgl::graph::mtl::MaterialVariantKey &)
    {
        std::string out = "#version 450\n\n";
        hgl::graph::ShaderWriter writer(out);
        writer.EmitCommentLine("BuildBillboardDynamicVertexEntry.Begin");
        writer.EmitInclude("compositor/main_forward_billboard_dynamic.vert.glsl");
        writer.EmitCommentLine("BuildBillboardDynamicVertexEntry.End");
        return out;
    }

    std::string BuildBillboardFixedVertexEntry(const hgl::graph::mtl::MaterialVariantKey &)
    {
        std::string out = "#version 450\n\n";
        hgl::graph::ShaderWriter writer(out);
        writer.EmitCommentLine("BuildBillboardFixedVertexEntry.Begin");
        writer.EmitInclude("compositor/main_forward_billboard_fixed.vert.glsl");
        writer.EmitCommentLine("BuildBillboardFixedVertexEntry.End");
        return out;
    }

    std::string BuildTerrainGridVertexEntry(const hgl::graph::mtl::MaterialVariantKey &)
    {
        std::string out = "#version 450\n\n";
        hgl::graph::ShaderWriter writer(out);
        writer.EmitCommentLine("BuildTerrainGridVertexEntry.Begin");
        writer.EmitInclude("compositor/main_terrain_grid.vert.glsl");
        writer.EmitCommentLine("BuildTerrainGridVertexEntry.End");
        return out;
    }

    std::string BuildForwardUnlitVertexColorVS(const hgl::graph::mtl::MaterialVariantKey &)
    {
        hgl::graph::CompositorFeatureFlags flags;
        flags.SetVertexAttrib(hgl::graph::VertexAttrib::Color);
        return BuildForwardVertexEntry(flags);
    }

    std::string BuildForwardUnlitLuminanceVS(const hgl::graph::mtl::MaterialVariantKey &)
    {
        hgl::graph::CompositorFeatureFlags flags;
        flags.SetVertexAttrib(hgl::graph::VertexAttrib::Luminance);
        return BuildForwardVertexEntry(flags);
    }

    std::string BuildForwardUnlitLuminance2DVS(const hgl::graph::mtl::MaterialVariantKey &)
    {
        hgl::graph::CompositorFeatureFlags flags;
        flags.vert_input_2d = true;
        flags.SetVertexAttrib(hgl::graph::VertexAttrib::Luminance);
        return BuildForwardVertexEntry(flags);
    }

    std::string BuildForwardUnlitNormalVS(const hgl::graph::mtl::MaterialVariantKey &)
    {
        hgl::graph::CompositorFeatureFlags flags;
        flags.SetVertexAttrib(hgl::graph::VertexAttrib::Position);
        flags.SetVertexAttrib(hgl::graph::VertexAttrib::Normal);
        return BuildForwardVertexEntry(flags);
    }

    std::string BuildForwardSkyVS(const hgl::graph::mtl::MaterialVariantKey &)
    {
        hgl::graph::CompositorFeatureFlags flags;
        flags.has_direction = true;
        return BuildForwardVertexEntry(flags);
    }

    std::string BuildForwardLitVS(const hgl::graph::mtl::MaterialVariantKey &key)
    {
        const bool uv0    = key.HasVertexAttrib(hgl::graph::VertexAttrib::TexCoord);
        const bool normal = key.HasVertexAttrib(hgl::graph::VertexAttrib::Normal);
        const bool tangent = key.HasVertexAttrib(hgl::graph::VertexAttrib::Tangent);
        hgl::graph::CompositorFeatureFlags flags;
        flags.SetVertexAttrib(hgl::graph::VertexAttrib::TexCoord, uv0);
        flags.SetVertexAttrib(hgl::graph::VertexAttrib::Position);
        flags.SetVertexAttrib(hgl::graph::VertexAttrib::Normal, normal);
        flags.SetVertexAttrib(hgl::graph::VertexAttrib::Tangent, tangent);
        return BuildForwardVertexEntry(flags);
    }

    std::string BuildForwardUnlitVertexColorFS(const hgl::graph::mtl::MaterialVariantKey &, const hgl::graph::RenderAlphaMode, const std::string &surface_path)
    {
        hgl::graph::CompositorFeatureFlags flags;
        flags.SetVertexAttrib(hgl::graph::VertexAttrib::Color);
        flags.surface_path = surface_path;
        return BuildForwardFragmentEntry(flags);
    }

    std::string BuildForwardUnlitLuminanceFS(const hgl::graph::mtl::MaterialVariantKey &, const hgl::graph::RenderAlphaMode, const std::string &surface_path)
    {
        hgl::graph::CompositorFeatureFlags flags;
        flags.SetVertexAttrib(hgl::graph::VertexAttrib::Luminance);
        flags.surface_path = surface_path;
        return BuildForwardFragmentEntry(flags);
    }

    std::string BuildForwardUnlitNormalFS(const hgl::graph::mtl::MaterialVariantKey &, const hgl::graph::RenderAlphaMode, const std::string &surface_path)
    {
        hgl::graph::CompositorFeatureFlags flags;
        flags.SetVertexAttrib(hgl::graph::VertexAttrib::Position);
        flags.SetVertexAttrib(hgl::graph::VertexAttrib::Normal);
        flags.needs_camera = true;
        flags.surface_path = surface_path;
        return BuildForwardFragmentEntry(flags);
    }

    std::string BuildForwardBillboardFS(const hgl::graph::mtl::MaterialVariantKey &, const hgl::graph::RenderAlphaMode blend, const std::string &surface_path)
    {
        const bool alpha_masked = (blend == hgl::graph::RenderAlphaMode::Masked);
        const bool alpha_dither = (blend == hgl::graph::RenderAlphaMode::Dither);
        hgl::graph::CompositorFeatureFlags flags;
        flags.alpha_masked = alpha_masked;
        flags.alpha_dither = alpha_dither;
        flags.has_texcoord = true;
        flags.surface_path = surface_path;
        return BuildForwardFragmentEntry(flags);
    }

    std::string BuildForwardSkyFS(const hgl::graph::mtl::MaterialVariantKey &, const hgl::graph::RenderAlphaMode, const std::string &surface_path)
    {
        hgl::graph::CompositorFeatureFlags flags;
        flags.has_direction = true;
        flags.surface_path = surface_path;
        return BuildForwardFragmentEntry(flags);
    }

    std::string BuildTerrainGridFS(const hgl::graph::mtl::MaterialVariantKey &, const hgl::graph::RenderAlphaMode, const std::string &surface_path)
    {
        hgl::graph::CompositorFeatureFlags flags;
        flags.SetVertexAttrib(hgl::graph::VertexAttrib::Normal);
        flags.has_clip_pos = true;
        flags.surface_path = surface_path;
        return BuildForwardFragmentEntry(flags);
    }

    bool TryBuildAutoForwardFS(const hgl::graph::mtl::MaterialVariantKey &key,
                               const hgl::graph::RenderAlphaMode blend,
                               const hgl::graph::PassType pass,
                               const std::string &surface_path,
                               std::string &out_source);

    std::string BuildForwardLitFS(const hgl::graph::mtl::MaterialVariantKey &key, const hgl::graph::RenderAlphaMode, const std::string &surface_path)
    {
        const bool uv0    = key.HasVertexAttrib(hgl::graph::VertexAttrib::TexCoord);
        const bool normal = key.HasVertexAttrib(hgl::graph::VertexAttrib::Normal);
        const bool tangent = key.HasVertexAttrib(hgl::graph::VertexAttrib::Tangent);
        hgl::graph::CompositorFeatureFlags flags;
        flags.SetVertexAttrib(hgl::graph::VertexAttrib::TexCoord, uv0);
        flags.SetVertexAttrib(hgl::graph::VertexAttrib::Position);
        flags.SetVertexAttrib(hgl::graph::VertexAttrib::Normal, normal);
        flags.SetVertexAttrib(hgl::graph::VertexAttrib::Tangent, tangent);
        flags.enable_lighting = true;
        flags.lighting_model = key.lighting_model;
        flags.needs_camera = true;
        flags.needs_sky = true;
        flags.sky_ambient_model = key.sky_ambient_model;
        flags.surface_path = surface_path;
        return BuildForwardFragmentEntry(flags);
    }

    std::string BuildForwardAutoFSFromKey(const hgl::graph::mtl::MaterialVariantKey &key,
                                          const hgl::graph::RenderAlphaMode blend,
                                          const std::string &surface_path)
    {
        std::string out_source;
        if (TryBuildAutoForwardFS(key, blend, key.pass_hint, surface_path, out_source))
            return out_source;

        return std::string();
    }

    bool TryBuildAutoForwardFS(const hgl::graph::mtl::MaterialVariantKey &key,
                               const hgl::graph::RenderAlphaMode blend,
                               const hgl::graph::PassType pass,
                               const std::string &surface_path,
                               std::string &out_source)
    {
        using hgl::graph::PassType;
        using hgl::graph::RenderAlphaMode;
        using hgl::graph::SurfaceType;

        // Only forward passes are generated in this path.
        switch (pass)
        {
            case PassType::ForwardOpaque:
            case PassType::ForwardMasked:
            case PassType::ForwardTransparent:
            case PassType::ForwardDither:
            case PassType::ForwardA2C:
                break;
            default:
                return false;
        }

        hgl::graph::CompositorFeatureFlags flags;
        flags.surface_path = surface_path;
        flags.vertex_attrib_bits = key.vertex_attribute_feature_bits;

        if (blend == RenderAlphaMode::Masked)
            flags.alpha_masked = true;
        if (blend == RenderAlphaMode::Dither)
            flags.alpha_dither = true;

        if (key.surface_type == SurfaceType::Sky)
            flags.has_direction = true;

        if (key.surface_type != SurfaceType::Unlit && !hgl::graph::Is2DSurfaceType(key.surface_type) && key.surface_type != SurfaceType::Sky)
        {
            flags.enable_lighting = true;
            flags.lighting_model = key.lighting_model;
            flags.needs_camera = true;
            flags.needs_sky = true;
            flags.sky_ambient_model = key.sky_ambient_model;
        }

        out_source = BuildForwardFragmentEntry(flags);
        return true;
    }

    static const GeneratedVSTemplateRoute kGeneratedVSTemplateRoutes[] = {
        {"compositor/main_forward_unlit_vertexcolor.vert.glsl", &BuildForwardUnlitVertexColorVS},
        {"compositor/main_forward_unlit_luminance.vert.glsl",   &BuildForwardUnlitLuminanceVS},
        {"compositor/main_forward_unlit_luminance_2d.vert.glsl", &BuildForwardUnlitLuminance2DVS},
        {"compositor/main_forward_unlit_normal.vert.glsl",      &BuildForwardUnlitNormalVS},
        {"compositor/main_forward_sky.vert.glsl",               &BuildForwardSkyVS},
        {"compositor/main_forward_billboard_dynamic.vert.glsl", &BuildBillboardDynamicVertexEntry},
        {"compositor/main_forward_billboard_fixed.vert.glsl",   &BuildBillboardFixedVertexEntry},
        {"compositor/main_terrain_grid.vert.glsl",              &BuildTerrainGridVertexEntry},
        {"compositor/main_forward_lit.vert.glsl",               &BuildForwardLitVS},
    };

    static const GeneratedFSTemplateRoute kGeneratedFSTemplateRoutes[] = {
        {"compositor/main_forward_unlit_vertexcolor.frag.glsl", &BuildForwardUnlitVertexColorFS},
        {"compositor/main_forward_unlit_luminance.frag.glsl",   &BuildForwardUnlitLuminanceFS},
        {"compositor/main_forward_unlit_normal.frag.glsl",      &BuildForwardUnlitNormalFS},
        {"compositor/main_forward_billboard.frag.glsl",         &BuildForwardBillboardFS},
        {"compositor/main_forward_sky.frag.glsl",               &BuildForwardSkyFS},
        {"compositor/main_terrain_grid.frag.glsl",              &BuildTerrainGridFS},
        {"compositor/main_forward_lit.frag.glsl",               &BuildForwardLitFS},
        {"compositor/main_forward_opaque.frag.glsl",            &BuildForwardAutoFSFromKey},
        {"compositor/main_forward_transparent.frag.glsl",       &BuildForwardAutoFSFromKey},
        {"compositor/main_forward_masked.frag.glsl",            &BuildForwardAutoFSFromKey},
        {"compositor/main_forward_dither.frag.glsl",            &BuildForwardAutoFSFromKey},
        {"compositor/main_forward_a2c.frag.glsl",               &BuildForwardAutoFSFromKey},
        {"compositor/main_forward_unlit.frag.glsl",             &BuildForwardAutoFSFromKey},
        {"compositor/main_forward_2d_opaque.frag.glsl",         &BuildForwardAutoFSFromKey},
        {"compositor/main_forward_2d_transparent.frag.glsl",    &BuildForwardAutoFSFromKey},
        {"compositor/main_forward_2d_masked.frag.glsl",         &BuildForwardAutoFSFromKey},
        {"compositor/main_forward_2d_dither.frag.glsl",         &BuildForwardAutoFSFromKey},
    };

    static const SurfaceFunctionRoute kSurfaceFunctionRoutes[] = {
        {hgl::graph::SurfaceType::PureColor2D,  "surface/unlit_color3d_surface.glsl"},
        {hgl::graph::SurfaceType::VertexColor2D,"surface/unlit_vertexcolor_surface.glsl"},
        {hgl::graph::SurfaceType::PureTexture2D,"surface/2d/puretexture2d_surface.glsl"},
        {hgl::graph::SurfaceType::Text2D,       "surface/2d/text2d_surface.glsl"},
        {hgl::graph::SurfaceType::Standard,     "surface/standard_surface.glsl"},
        {hgl::graph::SurfaceType::Unlit,        "surface/unlit_color3d_surface.glsl"},
        {hgl::graph::SurfaceType::Skin,         "surface/skin_surface.glsl"},
        {hgl::graph::SurfaceType::Hair,         "surface/hair_surface.glsl"},
        {hgl::graph::SurfaceType::Cloth,        "surface/cloth_surface.glsl"},
        {hgl::graph::SurfaceType::Eye,          "surface/eye_surface.glsl"},
        {hgl::graph::SurfaceType::Foliage,      "surface/foliage_surface.glsl"},
        {hgl::graph::SurfaceType::ClearCoat,    "surface/clearcoat_surface.glsl"},
        {hgl::graph::SurfaceType::Water,        "surface/water_surface.glsl"},
        {hgl::graph::SurfaceType::Terrain,      "surface/terrain_surface.glsl"},
        {hgl::graph::SurfaceType::Sky,          "surface/sky_surface.glsl"},
    };

    template<typename Route, size_t N>
    const Route *FindRouteByTemplatePath(const Route (&routes)[N], const std::string &template_path)
    {
        for(const auto &route : routes)
        {
            if(template_path == route.template_path)
                return &route;
        }

        return nullptr;
    }

    static void WarnLegacyTemplateDiskRead(const char *stage, const std::string &template_path, const std::string &absolute_path)
    {
        static std::atomic_bool s_warned_vs{false};
        static std::atomic_bool s_warned_fs{false};

        std::atomic_bool *flag = &s_warned_fs;
        if (stage && stage[0] == 'V')
            flag = &s_warned_vs;

        bool expected = false;
        if (!flag->compare_exchange_strong(expected, true, std::memory_order_relaxed))
            return;

        std::fprintf(stderr,
            "[CompositorAssembler] warning: falling back to disk template read (%s). "
            "template='%s' path='%s'. This path is compatibility-only; prefer generated template routes.\n",
            stage ? stage : "Unknown",
            template_path.c_str(),
            absolute_path.c_str());
    }

    template<size_t N>
    const SurfaceFunctionRoute *FindSurfaceFunctionRoute(const SurfaceFunctionRoute (&routes)[N], const hgl::graph::SurfaceType surface)
    {
        for(const auto &route : routes)
        {
            if(route.surface == surface)
                return &route;
        }

        return nullptr;
    }

    size_t FindVersionDirectiveLineEnd(const std::string &source)
    {
        if(source.empty())
            return std::string::npos;

        size_t begin = 0;

        // UTF-8 BOM support
        if(source.size() >= 3
        && static_cast<unsigned char>(source[0]) == 0xEF
        && static_cast<unsigned char>(source[1]) == 0xBB
        && static_cast<unsigned char>(source[2]) == 0xBF)
        {
            begin = 3;
        }

        while(begin < source.size())
        {
            const char ch = source[begin];
            if(ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n')
            {
                ++begin;
                continue;
            }
            break;
        }

        if(begin + 8 <= source.size() && source.compare(begin, 8, "#version") == 0)
        {
            const size_t eol = source.find('\n', begin);
            return eol == std::string::npos ? source.size() : (eol + 1);
        }

        return std::string::npos;
    }

    std::string BuildGLSLPreviewFirstLines(const std::string &source, const size_t max_lines)
    {
        if(source.empty() || max_lines == 0)
            return std::string();

        size_t line_count = 0;
        size_t pos = 0;

        while(pos < source.size() && line_count < max_lines)
        {
            const size_t eol = source.find('\n', pos);
            ++line_count;

            if(eol == std::string::npos)
                return source;

            pos = eol + 1;
        }

        if(pos >= source.size())
            return source;

        return source.substr(0, pos);
    }

    std::string BuildAssembleReadFailureMessage(const char *stage,
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

    std::string BuildAssemblePreprocessFailureMessage(const char *stage,
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
        msg += BuildGLSLPreviewFirstLines(glsl_source, 80);
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

    bool CompositorAssembler::ReadFile(const std::string &path, std::string &out_content, std::string &out_error) const
    {
        std::ifstream ifs(path, std::ios::in);
        if (!ifs.is_open())
        {
            out_error = "Failed to open file: " + path;
            return false;
        }
        std::ostringstream ss;
        ss << ifs.rdbuf();
        out_content = ss.str();
        return true;
    }

    bool CompositorAssembler::TryBuildGeneratedVSTemplatePath(const std::string &template_path, const mtl::MaterialVariantKey &key, std::string &out_source) const
    {
        if(const GeneratedVSTemplateRoute *route = FindRouteByTemplatePath(kGeneratedVSTemplateRoutes, template_path))
        {
            out_source = route->builder(key);
            return true;
        }

        return false;
    }

    bool CompositorAssembler::TryBuildGeneratedFSTemplatePath(const std::string &template_path, const mtl::MaterialVariantKey &key, RenderAlphaMode blend, const std::string &surface_path, std::string &out_source) const
    {
        if(const GeneratedFSTemplateRoute *route = FindRouteByTemplatePath(kGeneratedFSTemplateRoutes, template_path))
        {
            out_source = route->builder(key, blend, surface_path);
            return true;
        }

        return false;
    }

    std::string CompositorAssembler::GetCompositorVSPath(SurfaceType surface, PassType pass) const
    {
        // 2D Materials — reuse vert_forward_main.glsl via VERT_INPUT_2D
        if (Is2DSurfaceType(surface))
        {
            switch (surface)
            {
            case SurfaceType::PureColor2D:
                // No texture, no vertex color — minimal VS
                return shader_lib_path_ + "/compositor/main_forward_2d_common.vert.glsl";

            case SurfaceType::PureTexture2D:
            case SurfaceType::Text2D:
                // Needs UV0 for texture sampling
                return shader_lib_path_ + "/compositor/main_forward_2d_texcoord.vert.glsl";

            case SurfaceType::VertexColor2D:
                // Needs vertex color varying
                return shader_lib_path_ + "/compositor/main_forward_2d_vertexcolor.vert.glsl";

            default:
                break;
            }
        }

        // Unlit 表面使用精简 VS（仅 Position + 双 ID，无 Normal/UV）
        if (surface == SurfaceType::Unlit)
        {
            switch (pass)
            {
            case PassType::ForwardOpaque:
            case PassType::ForwardMasked:
            case PassType::ForwardTransparent:
                return shader_lib_path_ + "/compositor/main_forward_unlit.vert.glsl";
            default:
                return shader_lib_path_ + "/compositor/main_forward_unlit.vert.glsl";
            }
        }

        // Lit 表面走完整 VS（Position + Normal + UV + 双 ID）
        switch (pass)
        {
        case PassType::ForwardOpaque:
        case PassType::ForwardMasked:
        case PassType::ForwardTransparent:
        case PassType::ForwardDither:
        case PassType::ForwardA2C:
            return shader_lib_path_ + "/compositor/main_forward_opaque.vert.glsl";

        case PassType::ShadowOpaque:
        case PassType::ShadowMasked:
            return shader_lib_path_ + "/compositor/main_shadow.vert.glsl"; // 后续实现

        case PassType::EarlyZSolid:
        case PassType::EarlyZMasked:
            return shader_lib_path_ + "/compositor/main_earlyz.vert.glsl"; // 后续实现

        default:
            return shader_lib_path_ + "/compositor/main_forward_opaque.vert.glsl";
        }
    }

    std::string CompositorAssembler::GetCompositorFSPath(SurfaceType surface, RenderAlphaMode blend, PassType pass) const
    {
        // 2D Materials — reuse frag_forward_main.glsl, routed by pass type
        if (Is2DSurfaceType(surface))
        {
            switch (pass)
            {
            case PassType::ForwardOpaque:
                return shader_lib_path_ + "/compositor/main_forward_2d_opaque.frag.glsl";

            case PassType::ForwardMasked:
                return shader_lib_path_ + "/compositor/main_forward_2d_masked.frag.glsl";

            case PassType::ForwardDither:
                return shader_lib_path_ + "/compositor/main_forward_2d_dither.frag.glsl";

            case PassType::ForwardTransparent:
            default:
                return shader_lib_path_ + "/compositor/main_forward_2d_transparent.frag.glsl";
            }
        }

        // Unlit 表面类型使用专用 FS 模板（无光照）
        if (surface == SurfaceType::Unlit)
        {
            switch (pass)
            {
            case PassType::ForwardOpaque:
            case PassType::ForwardMasked:
            case PassType::ForwardTransparent:
                return shader_lib_path_ + "/compositor/main_forward_unlit.frag.glsl";

            case PassType::ShadowOpaque:
            case PassType::ShadowMasked:
                return shader_lib_path_ + "/compositor/main_shadow.frag.glsl"; // 后续实现

            case PassType::EarlyZSolid:
            case PassType::EarlyZMasked:
                return shader_lib_path_ + "/compositor/main_earlyz.frag.glsl"; // 后续实现

            default:
                return shader_lib_path_ + "/compositor/main_forward_unlit.frag.glsl";
            }
        }

        // 非 Unlit 走 Lit 路径
        switch (pass)
        {
        case PassType::ForwardOpaque:
            return shader_lib_path_ + "/compositor/main_forward_opaque.frag.glsl";

        case PassType::ForwardMasked:
            return shader_lib_path_ + "/compositor/main_forward_masked.frag.glsl";

        case PassType::ForwardTransparent:
            return shader_lib_path_ + "/compositor/main_forward_transparent.frag.glsl";

        case PassType::ForwardDither:
            return shader_lib_path_ + "/compositor/main_forward_dither.frag.glsl";

        case PassType::ForwardA2C:
            return shader_lib_path_ + "/compositor/main_forward_a2c.frag.glsl";

        case PassType::ShadowOpaque:
        case PassType::ShadowMasked:
            return shader_lib_path_ + "/compositor/main_shadow.frag.glsl"; // 后续实现

        case PassType::EarlyZSolid:
        case PassType::EarlyZMasked:
            return shader_lib_path_ + "/compositor/main_earlyz.frag.glsl"; // 后续实现

        default:
            return shader_lib_path_ + "/compositor/main_forward_opaque.frag.glsl";
        }
    }

    std::string CompositorAssembler::GetSurfaceFunctionPath(SurfaceType surface) const
    {
        if(const SurfaceFunctionRoute *route = FindSurfaceFunctionRoute(kSurfaceFunctionRoutes, surface))
            return route->path;

        return "surface/standard_surface.glsl";
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

        const size_t insert_pos = FindVersionDirectiveLineEnd(source);
        if(insert_pos != std::string::npos)
        {
            std::string result;
            result.reserve(source.size() + defines.size() + 2);
            result.append(source, 0, insert_pos);
            result.append("\n");
            result.append(defines);
            result.append("\n");
            result.append(source, insert_pos, std::string::npos);
            return result;
        }

        // 没有 #version 行则直接在头部插入
        return defines + "\n" + source;
    }

    CompositorAssembler::AssembleResult CompositorAssembler::Assemble(
        const mtl::MaterialVariantKey  &key,
        const mtl::MaterialVariantDesc &desc
    ) const
    {
        AssembleResult result{};

        std::string vs_path = desc.vs_template_path.empty()
            ? GetCompositorVSPath(key.surface_type, key.pass_hint)
            : shader_lib_path_ + "/" + desc.vs_template_path;
        std::string fs_path = desc.fs_template_path.empty()
            ? GetCompositorFSPath(key.surface_type, key.blend_mode, key.pass_hint)
            : shader_lib_path_ + "/" + desc.fs_template_path;
        std::string surface_rel = desc.surface_function_path.empty()
            ? GetSurfaceFunctionPath(key.surface_type)
            : desc.surface_function_path;

        std::string vs_source;
        if (!desc.vs_template_path.empty())
        {
            std::string read_error;
            const bool built_generated = TryBuildGeneratedVSTemplatePath(desc.vs_template_path, key, vs_source);

            if (!built_generated && kStrictGeneratedTemplateRoutes)
            {
                result.error_message = "Strict generated-template mode rejects VS disk fallback for template='"
                    + desc.vs_template_path + "'";
                result.success = false;
                return result;
            }

            if (!built_generated && !ReadFile(vs_path, vs_source, read_error))
            {
                result.error_message = BuildAssembleReadFailureMessage("VS", desc.vs_template_path, vs_path, read_error);
                result.success = false;
                return result;
            }

            if (!built_generated)
                WarnLegacyTemplateDiskRead("VS", desc.vs_template_path, vs_path);
        }
        else
        {
            std::string read_error;
            if(!ReadFile(vs_path, vs_source, read_error))
            {
                result.error_message = BuildAssembleReadFailureMessage("VS", std::string(), vs_path, read_error);
                result.success = false;
                return result;
            }
        }

        std::string fs_source;
        if (!desc.fs_template_path.empty())
        {
            std::string read_error;
            const bool built_generated = TryBuildGeneratedFSTemplatePath(desc.fs_template_path, key, key.blend_mode, surface_rel, fs_source);

            if (!built_generated && kStrictGeneratedTemplateRoutes)
            {
                result.error_message = "Strict generated-template mode rejects FS disk fallback for template='"
                    + desc.fs_template_path + "'";
                result.success = false;
                return result;
            }

            if (!built_generated && !ReadFile(fs_path, fs_source, read_error))
            {
                result.error_message = BuildAssembleReadFailureMessage("FS", desc.fs_template_path, fs_path, read_error);
                result.success = false;
                return result;
            }

            if (!built_generated)
                WarnLegacyTemplateDiskRead("FS", desc.fs_template_path, fs_path);
        }
        else
        {
            std::string read_error;
            if(!TryBuildAutoForwardFS(key, key.blend_mode, key.pass_hint, surface_rel, fs_source)
            && !ReadFile(fs_path, fs_source, read_error))
            {
                result.error_message = BuildAssembleReadFailureMessage("FS", std::string(), fs_path, read_error);
                result.success = false;
                return result;
            }
        }

        vs_source = InjectDefines(vs_source, key);
        fs_source = InjectDefines(fs_source, key);

        if(vs_source.empty())
        {
            result.error_message = BuildAssemblePreprocessFailureMessage("VS",
                                                                          desc.vs_template_path,
                                                                          "InjectDefines produced empty source",
                                                                          vs_source);
            result.success = false;
            return result;
        }

        if(fs_source.empty())
        {
            result.error_message = BuildAssemblePreprocessFailureMessage("FS",
                                                                          desc.fs_template_path,
                                                                          "InjectDefines produced empty source",
                                                                          fs_source);
            result.success = false;
            return result;
        }

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
