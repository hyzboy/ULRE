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

    std::string CompositorAssembler::GetCompositorVSPath(PassType pass) const
    {
        // 第一版只支持 ForwardOpaque
        switch (pass)
        {
        case PassType::ForwardOpaque:
        case PassType::ForwardMasked:
            return shader_lib_path_ + "/compositor/main_forward_opaque.vert.glsl";

        case PassType::ShadowOpaque:
        case PassType::ShadowMasked:
            return shader_lib_path_ + "/compositor/main_shadow.vert.glsl"; // 后续实现

        case PassType::EarlyZSolid:
        case PassType::EarlyZMasked:
            return shader_lib_path_ + "/compositor/main_earlyz.vert.glsl"; // 后续实现

        case PassType::VBufferID:
            return shader_lib_path_ + "/compositor/main_vbuffer.vert.glsl"; // 后续实现

        default:
            return shader_lib_path_ + "/compositor/main_forward_opaque.vert.glsl";
        }
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
        case SurfaceType::Terrain:    return "surface/terrain_surface.glsl";     // 后续实现
        case SurfaceType::Sky:        return "surface/sky_surface.glsl";         // 后续实现
        default:                      return "surface/standard_surface.glsl";
        }
    }

    std::string CompositorAssembler::InjectDefines(const std::string &source, const NewShaderPermutationKey &key) const
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
        QualityTier     tier,
        PlatformBackend platform
    ) const
    {
        AssembleResult result{};

        // 1. 构建 permutation key
        NewShaderPermutationKey key;
        key.SetSurfaceType(surface);
        key.SetQualityTier(tier);
        key.SetPlatform(platform);

        // 2. 获取模板文件路径
        std::string vs_path = GetCompositorVSPath(pass);
        std::string fs_path = GetCompositorFSPath(surface, blend, pass);
        std::string surface_rel = GetSurfaceFunctionPath(surface);

        // 3. 读取 VS 模板
        std::string vs_source;
        if (!ReadFile(vs_path, vs_source, result.error_message))
        {
            result.success = false;
            return result;
        }

        // 4. 读取 FS 模板
        std::string fs_source;
        if (!ReadFile(fs_path, fs_source, result.error_message))
        {
            result.success = false;
            return result;
        }

        // 5. 注入 #define
        vs_source = InjectDefines(vs_source, key);
        fs_source = InjectDefines(fs_source, key);

        // 6. 替换 FS 中的 SURFACE_FUNCTION_FILE
        fs_source = ReplaceSurfaceInclude(fs_source, surface_rel);

        result.vertex_glsl   = std::move(vs_source);
        result.fragment_glsl = std::move(fs_source);
        result.success       = true;
        return result;
    }
}
