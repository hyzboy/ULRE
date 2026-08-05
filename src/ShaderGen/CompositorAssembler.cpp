#include <hgl/shadergen/CompositorAssembler.h>
#include <fstream>
#include <sstream>

namespace hgl::graph
{
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

    std::string CompositorAssembler::GetCompositorFSPath(SurfaceType surface, BlendMode blend, PassType pass) const
    {
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
            return shader_lib_path_ + "/compositor/main_forward_masked.frag.glsl"; // 后续实现

        case PassType::ForwardTransparent:
            return shader_lib_path_ + "/compositor/main_forward_transparent.frag.glsl"; // 后续实现

        case PassType::ShadowOpaque:
        case PassType::ShadowMasked:
            return shader_lib_path_ + "/compositor/main_shadow.frag.glsl"; // 后续实现

        case PassType::EarlyZSolid:
        case PassType::EarlyZMasked:
            return shader_lib_path_ + "/compositor/main_earlyz.frag.glsl"; // 后续实现

        case PassType::VBufferID:
            return shader_lib_path_ + "/compositor/main_vbuffer.frag.glsl"; // 后续实现

        default:
            return shader_lib_path_ + "/compositor/main_forward_opaque.frag.glsl";
        }
    }

    std::string CompositorAssembler::GetSurfaceFunctionPath(SurfaceType surface) const
    {
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
        case SurfaceType::Sky:        return "surface/sky_surface.glsl";         // 后续实现
        default:                      return "surface/standard_surface.glsl";
        }
    }

    std::string CompositorAssembler::InjectDefines(
        const std::string &source,
        const NewShaderPermutationKey &key,
        const CompositorModuleOptions &module_options) const
    {
        std::string defines;
        key.AppendGLSLDefines(defines);

        // Metadata/comments may precede #version in file-backed templates, but
        // GLSL requires #version to be the first preprocessing token.
        size_t version_line_start = std::string::npos;
        size_t version_token_start = std::string::npos;
        size_t version_line_end = std::string::npos;
        size_t line_start = 0;
        while (line_start < source.size())
        {
            const size_t line_end = source.find('\n', line_start);
            const size_t content_end = line_end == std::string::npos ? source.size() : line_end;
            size_t token_start = line_start;
            while (token_start < content_end
                && (source[token_start] == ' ' || source[token_start] == '\t'
                 || source[token_start] == '\r'))
                ++token_start;

            if (content_end - token_start >= 8
             && source.compare(token_start, 8, "#version") == 0)
            {
                version_line_start = line_start;
                version_token_start = token_start;
                version_line_end = content_end;
                break;
            }

            if (line_end == std::string::npos)
                break;
            line_start = line_end + 1;
        }

        if (version_token_start != std::string::npos)
        {
            std::string result;
            result.reserve(source.size() + defines.size() + 32);
            result.append(source, version_token_start, version_line_end - version_token_start);
            result.append("\n");
            result.append(source, 0, version_line_start);
            if (!defines.empty())
            {
                result.append(defines);
                result.append("\n");
            }

            const size_t suffix_start = version_line_end < source.size()
                ? version_line_end + 1 : version_line_end;
            result.append(source, suffix_start, std::string::npos);
            return result;
        }

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

    std::string CompositorAssembler::ReplaceLightingModuleIncludes(
        const std::string &source,
        const CompositorModuleOptions &module_options) const
    {
        const char *module_paths[] =
        {
            module_options.sky_module && module_options.sky_module[0]
                ? module_options.sky_module
                : "sky/sky_atmosphere.glsl",
            module_options.direct_lighting_module && module_options.direct_lighting_module[0]
                ? module_options.direct_lighting_module
                : "lighting/direct_cook_torrance_pbr.glsl",
            module_options.indirect_lighting_module && module_options.indirect_lighting_module[0]
                ? module_options.indirect_lighting_module
                : "lighting/indirect_simple_ambient.glsl",
            module_options.ntb_module && module_options.ntb_module[0]
                ? module_options.ntb_module
                : "ntb/ntb_tangent_vbo_normalmap.glsl"
        };
        const char *default_paths[] =
        {
            "sky/sky_atmosphere.glsl",
            "lighting/direct_cook_torrance_pbr.glsl",
            "lighting/indirect_simple_ambient.glsl",
            "ntb/ntb_tangent_vbo_normalmap.glsl"
        };

        std::string result = source;
        for (size_t i = 0; i < 4; ++i)
        {
            const std::string marker =
                std::string("#include \"") + default_paths[i] + "\"";
            const size_t pos = result.find(marker);
            if (pos == std::string::npos)
                continue;

            const std::string replacement =
                std::string("#include \"") + module_paths[i] + "\"";
            result.replace(pos, marker.size(), replacement);
        }

        return result;
    }

    CompositorAssembler::AssembleResult CompositorAssembler::Assemble(
        SurfaceType                  surface,
        BlendMode                    blend,
        PassType                     pass,
        const char                  *fs_template_override,
        const char                  *surface_function_override,
        const CompositorModuleOptions &module_options
    ) const
    {
        AssembleResult result{};

        // 1. 构建 permutation key
        NewShaderPermutationKey key;
        key.SetSurfaceType(surface);

        // 2. 获取模板文件路径（支持覆盖）
        std::string fs_path = (fs_template_override && fs_template_override[0])
            ? shader_lib_path_ + "/" + fs_template_override
            : GetCompositorFSPath(surface, blend, pass);
        std::string surface_rel = (surface_function_override && surface_function_override[0])
            ? surface_function_override
            : GetSurfaceFunctionPath(surface);

        // 3. 读取 FS 模板
        std::string fs_source;
        if (!ReadFile(fs_path, fs_source, result.error_message))
        {
            result.success = false;
            return result;
        }

        // 4. 注入 #define
        fs_source = InjectDefines(fs_source, key, module_options);

        // 5. Resolve configurable module includes to literal header names.
        // GLSL does not allow a macro to stand in for the header token of #include.
        fs_source = ReplaceLightingModuleIncludes(fs_source, module_options);

        // 6. 替换 FS 中的 SURFACE_FUNCTION_FILE
        fs_source = ReplaceSurfaceInclude(fs_source, surface_rel);

        result.fragment_glsl = std::move(fs_source);
        result.success       = true;
        return result;
    }
}
