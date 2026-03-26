#include <hgl/shadergen/CompositorAssembler.h>
#include <hgl/shadergen/ShaderGenPathConfig.h>
#include <hgl/mtl/MaterialVariantDesc.h>
#include <fstream>
#include <sstream>

namespace
{
    void AppendDefine(std::string &out, const char *name)
    {
        out += "#define ";
        out += name;
        out += '\n';
    }

    void AppendInclude(std::string &out, const std::string &path)
    {
        out += "#include \"";
        out += path;
        out += "\"\n";
    }

    std::string BuildForwardVertexEntry(const bool vert_input_2d,
                                        const bool has_uv0,
                                        const bool has_vertex_color,
                                        const bool has_world_pos,
                                        const bool has_world_normal,
                                        const bool has_luminance,
                                        const bool has_direction)
    {
        std::string out = "#version 450\n\n";

        if (vert_input_2d)
            AppendDefine(out, "VERT_INPUT_2D");
        if (has_uv0)
            AppendDefine(out, "HAS_UV0");
        if (has_vertex_color)
            AppendDefine(out, "HAS_VERTEX_COLOR");
        if (has_world_pos)
            AppendDefine(out, "HAS_WORLD_POS");
        if (has_world_normal)
            AppendDefine(out, "HAS_WORLD_NORMAL");
        if (has_luminance)
            AppendDefine(out, "HAS_LUMINANCE");
        if (has_direction)
            AppendDefine(out, "HAS_DIRECTION");

        AppendInclude(out, "compositor/vert_forward_ubo.glsl");
        AppendInclude(out, "compositor/vert_forward_main.glsl");
        return out;
    }

    std::string BuildForwardFragmentEntry(const bool enable_lighting,
                                          const bool needs_camera,
                                          const bool needs_sky,
                                          const bool alpha_masked,
                                          const bool alpha_dither,
                                          const bool has_world_pos,
                                          const bool has_world_normal,
                                          const bool has_uv0,
                                          const bool has_vertex_color,
                                          const bool has_texcoord,
                                          const bool has_direction,
                                          const bool has_luminance,
                                          const bool has_clip_pos,
                                          const std::string &surface_path)
    {
        std::string out = "#version 450\n\n";

        if (enable_lighting)
            AppendDefine(out, "ENABLE_LIGHTING");
        if (needs_camera)
            AppendDefine(out, "NEEDS_CAMERA");
        if (needs_sky)
            AppendDefine(out, "NEEDS_SKY");
        if (alpha_masked)
            AppendDefine(out, "ALPHA_MODE_MASKED");
        if (alpha_dither)
            AppendDefine(out, "ALPHA_MODE_DITHER");
        if (has_world_pos)
            AppendDefine(out, "HAS_WORLD_POS");
        if (has_world_normal)
            AppendDefine(out, "HAS_WORLD_NORMAL");
        if (has_uv0)
            AppendDefine(out, "HAS_UV0");
        if (has_vertex_color)
            AppendDefine(out, "HAS_VERTEX_COLOR");
        if (has_texcoord)
            AppendDefine(out, "HAS_TEXCOORD");
        if (has_direction)
            AppendDefine(out, "HAS_DIRECTION");
        if (has_luminance)
            AppendDefine(out, "HAS_LUMINANCE");
        if (has_clip_pos)
            AppendDefine(out, "HAS_CLIP_POS");

        AppendInclude(out, "compositor/frag_forward_ubo.glsl");
        AppendInclude(out, surface_path);
        AppendInclude(out, "compositor/frag_forward_main.glsl");
        return out;
    }

    std::string BuildBillboardDynamicVertexEntry()
    {
        std::string out = "#version 450\n\n";
        AppendInclude(out, "compositor/main_forward_billboard_dynamic.vert.glsl");
        return out;
    }

    std::string BuildBillboardFixedVertexEntry()
    {
        std::string out = "#version 450\n\n";
        AppendInclude(out, "compositor/main_forward_billboard_fixed.vert.glsl");
        return out;
    }

    std::string BuildTerrainGridVertexEntry()
    {
        std::string out = "#version 450\n\n";
        AppendInclude(out, "compositor/main_terrain_grid.vert.glsl");
        return out;
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

    bool CompositorAssembler::TryBuildGeneratedCompositorVS(SurfaceType surface, PassType pass, std::string &out_source) const
    {
        if (Is2DSurfaceType(surface))
        {
            switch (surface)
            {
            case SurfaceType::PureColor2D:
                out_source = BuildForwardVertexEntry(true, false, false, false, false, false, false);
                return true;

            case SurfaceType::PureTexture2D:
            case SurfaceType::Text2D:
                out_source = BuildForwardVertexEntry(true, true, false, false, false, false, false);
                return true;

            case SurfaceType::VertexColor2D:
                out_source = BuildForwardVertexEntry(true, false, true, false, false, false, false);
                return true;

            default:
                break;
            }
        }

        if (surface == SurfaceType::Unlit)
        {
            switch (pass)
            {
            case PassType::ForwardOpaque:
            case PassType::ForwardMasked:
            case PassType::ForwardTransparent:
                out_source = BuildForwardVertexEntry(false, false, false, false, false, false, false);
                return true;
            default:
                return false;
            }
        }

        switch (pass)
        {
        case PassType::ForwardOpaque:
        case PassType::ForwardMasked:
        case PassType::ForwardTransparent:
        case PassType::ForwardDither:
        case PassType::ForwardA2C:
            out_source = BuildForwardVertexEntry(false, true, false, true, true, false, false);
            return true;
        default:
            return false;
        }
    }

    bool CompositorAssembler::TryBuildGeneratedCompositorFS(SurfaceType surface, BlendMode blend, PassType pass, const std::string &surface_path, std::string &out_source) const
    {
        (void)blend;

        if (Is2DSurfaceType(surface))
        {
            switch (pass)
            {
            case PassType::ForwardOpaque:
                out_source = BuildForwardFragmentEntry(false, false, false, false, false, false, false, false, false, false, false, false, false, surface_path);
                return true;
            case PassType::ForwardMasked:
                out_source = BuildForwardFragmentEntry(false, false, false, true, false, false, false, false, false, false, false, false, false, surface_path);
                return true;
            case PassType::ForwardDither:
                out_source = BuildForwardFragmentEntry(false, false, false, false, true, false, false, false, false, false, false, false, false, surface_path);
                return true;
            case PassType::ForwardTransparent:
                out_source = BuildForwardFragmentEntry(false, false, false, false, false, false, false, false, false, false, false, false, false, surface_path);
                return true;
            default:
                return false;
            }
        }

        if (surface == SurfaceType::Unlit)
        {
            switch (pass)
            {
            case PassType::ForwardOpaque:
            case PassType::ForwardMasked:
            case PassType::ForwardTransparent:
                out_source = BuildForwardFragmentEntry(false, false, false, false, false, false, false, false, false, false, false, false, false, surface_path);
                return true;
            default:
                return false;
            }
        }

        switch (pass)
        {
        case PassType::ForwardOpaque:
            out_source = BuildForwardFragmentEntry(true, false, false, false, false, true, true, true, false, false, false, false, false, surface_path);
            return true;
        case PassType::ForwardMasked:
            out_source = BuildForwardFragmentEntry(false, false, false, true, false, true, true, true, false, false, false, false, false, surface_path);
            return true;
        case PassType::ForwardTransparent:
        case PassType::ForwardA2C:
            out_source = BuildForwardFragmentEntry(false, false, false, false, false, true, true, true, false, false, false, false, false, surface_path);
            return true;
        case PassType::ForwardDither:
            out_source = BuildForwardFragmentEntry(false, false, false, false, true, true, true, true, false, false, false, false, false, surface_path);
            return true;
        default:
            return false;
        }
    }

    bool CompositorAssembler::TryBuildGeneratedVSTemplatePath(const std::string &template_path, std::string &out_source) const
    {
        if (template_path == "compositor/main_forward_unlit_vertexcolor.vert.glsl")
        {
            out_source = BuildForwardVertexEntry(false, false, true, false, false, false, false);
            return true;
        }

        if (template_path == "compositor/main_forward_unlit_luminance.vert.glsl")
        {
            out_source = BuildForwardVertexEntry(false, false, false, false, false, true, false);
            return true;
        }

        if (template_path == "compositor/main_forward_unlit_luminance_2d.vert.glsl")
        {
            out_source = BuildForwardVertexEntry(true, false, false, false, false, true, false);
            return true;
        }

        if (template_path == "compositor/main_forward_unlit_normal.vert.glsl")
        {
            out_source = BuildForwardVertexEntry(false, false, false, true, true, false, false);
            return true;
        }

        if (template_path == "compositor/main_forward_sky.vert.glsl")
        {
            out_source = BuildForwardVertexEntry(false, false, false, false, false, false, true);
            return true;
        }

        if (template_path == "compositor/main_forward_billboard_dynamic.vert.glsl")
        {
            out_source = BuildBillboardDynamicVertexEntry();
            return true;
        }

        if (template_path == "compositor/main_forward_billboard_fixed.vert.glsl")
        {
            out_source = BuildBillboardFixedVertexEntry();
            return true;
        }

        if (template_path == "compositor/main_terrain_grid.vert.glsl")
        {
            out_source = BuildTerrainGridVertexEntry();
            return true;
        }

        if (template_path == "compositor/main_forward_lit.vert.glsl")
        {
            // HAS_WORLD_POS + HAS_WORLD_NORMAL + HAS_UV0
            out_source = BuildForwardVertexEntry(false, true, false, true, true, false, false);
            return true;
        }

        return false;
    }

    bool CompositorAssembler::TryBuildGeneratedFSTemplatePath(const std::string &template_path, BlendMode blend, const std::string &surface_path, std::string &out_source) const
    {
        if (template_path == "compositor/main_forward_unlit_vertexcolor.frag.glsl")
        {
            out_source = BuildForwardFragmentEntry(false, false, false, false, false, false, false, false, true, false, false, false, false, surface_path);
            return true;
        }

        if (template_path == "compositor/main_forward_unlit_luminance.frag.glsl")
        {
            // has_luminance=true (param 12); has_direction stays false
            out_source = BuildForwardFragmentEntry(false, false, false, false, false, false, false, false, false, false, false, true, false, surface_path);
            return true;
        }

        if (template_path == "compositor/main_forward_unlit_normal.frag.glsl")
        {
            out_source = BuildForwardFragmentEntry(false, true, false, false, false, true, true, false, false, false, false, false, false, surface_path);
            return true;
        }

        if (template_path == "compositor/main_forward_billboard.frag.glsl")
        {
            const bool alpha_masked = (blend == BlendMode::Masked);
            const bool alpha_dither = (blend == BlendMode::Dither);
            out_source = BuildForwardFragmentEntry(false, false, false, alpha_masked, alpha_dither, false, false, false, false, true, false, false, false, surface_path);
            return true;
        }

        if (template_path == "compositor/main_forward_sky.frag.glsl")
        {
            out_source = BuildForwardFragmentEntry(false, false, false, false, false, false, false, false, false, false, true, false, false, surface_path);
            return true;
        }

        if (template_path == "compositor/main_terrain_grid.frag.glsl")
        {
            out_source = BuildForwardFragmentEntry(false, false, false, false, false, false, true, false, false, false, false, false, true, surface_path);
            return true;
        }

        if (template_path == "compositor/main_forward_lit.frag.glsl")
        {
            out_source = BuildForwardFragmentEntry(false, true, true, false, false, true, true, true, false, false, false, false, false, surface_path);
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

    std::string CompositorAssembler::GetCompositorFSPath(SurfaceType surface, BlendMode blend, PassType pass) const
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
        // 2D Materials
        if (Is2DSurfaceType(surface))
        {
            switch (surface)
            {
            // Reuse existing 3D surfaces — MaterialInstance layout is compatible
            case SurfaceType::PureColor2D:
                return "surface/unlit_color3d_surface.glsl";
            case SurfaceType::VertexColor2D:
                return "surface/unlit_vertexcolor_surface.glsl";
            // New 2D-specific surfaces
            case SurfaceType::PureTexture2D:
                return "surface/2d/puretexture2d_surface.glsl";
            case SurfaceType::Text2D:
                return "surface/2d/text2d_surface.glsl";
            default:
                break;
            }
        }

        switch (surface)
        {
        case SurfaceType::Standard:   return "surface/standard_surface.glsl";
        case SurfaceType::Unlit:      return "surface/unlit_color3d_surface.glsl";
        case SurfaceType::Skin:       return "surface/skin_surface.glsl";        // 后续实现
        case SurfaceType::Hair:       return "surface/hair_surface.glsl";        // 后续实现
        case SurfaceType::Cloth:      return "surface/cloth_surface.glsl";       // 后续实现
        case SurfaceType::Eye:        return "surface/eye_surface.glsl";         // 后续实现
        case SurfaceType::Foliage:    return "surface/foliage_surface.glsl";     // 后续实现
        case SurfaceType::ClearCoat:  return "surface/clearcoat_surface.glsl";   // 后续实现
        case SurfaceType::Water:      return "surface/water_surface.glsl";       // 后续实现
        case SurfaceType::Terrain:    return "surface/terrain_surface.glsl";     // 后续实现
        case SurfaceType::Sky:        return "surface/sky_surface.glsl";         // 后续实现
        default:                      return "surface/standard_surface.glsl";
        }
    }

    std::string CompositorAssembler::InjectDefines(const std::string &source, const ShaderPermutationKey &key) const
    {
        // 在 #version 行之后插入 #define 宏
        std::string defines;
        key.AppendGLSLDefines(defines);

        // 查找 #version 行的末尾
        auto pos = source.find('\n');
        if (pos != std::string::npos && source.substr(0, 8) == "#version")
        {
            std::string result;
            result.reserve(source.size() + defines.size() + 2);
            result.append(source, 0, pos + 1);
            result.append("\n");
            result.append(defines);
            result.append("\n");
            result.append(source, pos + 1, std::string::npos);
            return result;
        }

        // 没有 #version 行则直接在头部插入
        return defines + "\n" + source;
    }

    std::string CompositorAssembler::ReplaceSurfaceInclude(const std::string &source, const std::string &surface_path) const
    {
        const std::string marker = "#include SURFACE_FUNCTION_FILE";
        auto pos = source.find(marker);
        if (pos == std::string::npos)
            return source;

        std::string replacement = "#include \"" + surface_path + "\"";

        std::string result;
        result.reserve(source.size() + replacement.size());
        result.append(source, 0, pos);
        result.append(replacement);
        result.append(source, pos + marker.size(), std::string::npos);
        return result;
    }

    CompositorAssembler::AssembleResult CompositorAssembler::Assemble(
        SurfaceType     surface,
        BlendMode       blend,
        PassType        pass,
        const char     *vs_template_override,
        const char     *fs_template_override,
        const char     *surface_function_override
    ) const
    {
        AssembleResult result{};

        // 1. 构建 permutation key
        ShaderPermutationKey key;
        key.SetSurfaceType(surface);

        // 2. 获取模板文件路径（支持覆盖）
        std::string vs_path = (vs_template_override && vs_template_override[0])
            ? shader_lib_path_ + "/" + vs_template_override
            : GetCompositorVSPath(surface, pass);
        std::string fs_path = (fs_template_override && fs_template_override[0])
            ? shader_lib_path_ + "/" + fs_template_override
            : GetCompositorFSPath(surface, blend, pass);
        std::string surface_rel = (surface_function_override && surface_function_override[0])
            ? surface_function_override
            : GetSurfaceFunctionPath(surface);

        // 3. 构建或读取 VS 模板
        std::string vs_source;
          if ((vs_template_override && vs_template_override[0])
           && TryBuildGeneratedVSTemplatePath(vs_template_override, vs_source))
          {
          }
          else if (!(vs_template_override && vs_template_override[0])
              && TryBuildGeneratedCompositorVS(surface, pass, vs_source))
        {
        }
        else if (!ReadFile(vs_path, vs_source, result.error_message))
        {
            result.success = false;
            return result;
        }

        // 4. 构建或读取 FS 模板
        std::string fs_source;
          if ((fs_template_override && fs_template_override[0])
           && TryBuildGeneratedFSTemplatePath(fs_template_override, blend, surface_rel, fs_source))
          {
          }
          else if (!(fs_template_override && fs_template_override[0])
              && TryBuildGeneratedCompositorFS(surface, blend, pass, surface_rel, fs_source))
        {
        }
        else if (!ReadFile(fs_path, fs_source, result.error_message))
        {
            result.success = false;
            return result;
        }

        // 5. 注入 #define
        vs_source = InjectDefines(vs_source, key);
        fs_source = InjectDefines(fs_source, key);

        // 6. 替换 FS 中的 SURFACE_FUNCTION_FILE（自定义/遗留文件模板仍支持）
        fs_source = ReplaceSurfaceInclude(fs_source, surface_rel);

        result.vertex_glsl   = std::move(vs_source);
        result.fragment_glsl = std::move(fs_source);
        result.success       = true;
        return result;
    }

    CompositorAssembler::AssembleResult CompositorAssembler::Assemble(
        const mtl::MaterialVariantKey  &key,
        const mtl::MaterialVariantDesc &desc
    ) const
    {
        AssembleResult result{};

        ShaderPermutationKey perm;
        perm.SetSurfaceType(key.surface_type);

        const bool has_slot_modes = key.HasAnyTextureSourceBits();
        for (uint8 i = 0; i < uint8(mtl::SamplerSlotCount); ++i)
        {
            const mtl::SamplerSlot slot = static_cast<mtl::SamplerSlot>(i);
            const mtl::TextureSourceMode mode = has_slot_modes
                ? key.GetTextureSourceMode(slot)
                : key.texture_source_mode;
            perm.SetSlotArrayMode(slot, mode == mtl::TextureSourceMode::Array);
        }

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
          if (!desc.vs_template_path.empty()
           && TryBuildGeneratedVSTemplatePath(desc.vs_template_path, vs_source))
          {
          }
          else if (desc.vs_template_path.empty()
              && TryBuildGeneratedCompositorVS(key.surface_type, key.pass_hint, vs_source))
        {
        }
        else if (!ReadFile(vs_path, vs_source, result.error_message))
        {
            result.success = false;
            return result;
        }

        std::string fs_source;
          if (!desc.fs_template_path.empty()
           && TryBuildGeneratedFSTemplatePath(desc.fs_template_path, key.blend_mode, surface_rel, fs_source))
          {
          }
          else if (desc.fs_template_path.empty()
              && TryBuildGeneratedCompositorFS(key.surface_type, key.blend_mode, key.pass_hint, surface_rel, fs_source))
        {
        }
        else if (!ReadFile(fs_path, fs_source, result.error_message))
        {
            result.success = false;
            return result;
        }

        vs_source = InjectDefines(vs_source, perm);
        fs_source = InjectDefines(fs_source, perm);

        fs_source = ReplaceSurfaceInclude(fs_source, surface_rel);

        result.vertex_glsl   = std::move(vs_source);
        result.fragment_glsl = std::move(fs_source);
        result.success       = true;
        return result;
    }

    std::vector<PassType> CompositorAssembler::GetPassTypesForBlendMode(BlendMode blend)
    {
        switch (blend)
        {
        case BlendMode::Opaque:
            return { PassType::ForwardOpaque, PassType::ShadowOpaque, PassType::EarlyZSolid };

        case BlendMode::Masked:
            return { PassType::ForwardMasked, PassType::ShadowMasked, PassType::EarlyZMasked };

        case BlendMode::Transparent:
            // 透明物体无阴影、无 EarlyZ（从后往前排序第 8 Pass 渲染）
            return { PassType::ForwardTransparent };

        case BlendMode::Dither:
            // Dither 小批目使用 ShadowOpaque（不需要 alpha 阴影）
            return { PassType::ForwardDither, PassType::ShadowOpaque };

        case BlendMode::AlphaToCoverage:
            // A2C 阴影和 Masked 相同——需要 alpha discard 避免阴影漏光
            return { PassType::ForwardA2C, PassType::ShadowMasked };

        default:
            return { PassType::ForwardOpaque };
        }
    }
}
