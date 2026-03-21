#include <hgl/shadergen/CompositorAssembler.h>
#include <hgl/mtl/MaterialVariantDesc.h>
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

    std::string CompositorAssembler::GetCompositorVSPath(SurfaceType surface, PassType pass) const
    {
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

    std::string CompositorAssembler::GetSurfaceFunctionPath(SurfaceType surface, bool texture_array_mode) const
    {
        switch (surface)
        {
        case SurfaceType::Standard:
            return texture_array_mode
                ? "surface/standard_texturearray_surface.glsl"
                : "surface/standard_surface.glsl";
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
            : GetSurfaceFunctionPath(surface, (key.GetFlags() & 0x1) != 0);

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

    CompositorAssembler::AssembleResult CompositorAssembler::Assemble(
        const mtl::MaterialVariantKey  &key,
        const mtl::MaterialVariantDesc &desc
    ) const
    {
        AssembleResult result{};

        ShaderPermutationKey perm;
        perm.SetSurfaceType(key.surface_type);

        const bool has_slot_modes = key.HasAnyTextureSourceBits();
        const auto base_mode = has_slot_modes
            ? key.GetTextureSourceMode(mtl::SamplerSlot::BaseColor)
            : key.texture_source_mode;
        const auto normal_mode = has_slot_modes
            ? key.GetTextureSourceMode(mtl::SamplerSlot::Normal)
            : key.texture_source_mode;
        const auto rough_mode = has_slot_modes
            ? key.GetTextureSourceMode(mtl::SamplerSlot::Roughness)
            : key.texture_source_mode;

        perm.SetBaseTextureArrayMode(base_mode == mtl::TextureSourceMode::Array);
        perm.SetNormalTextureArrayMode(normal_mode == mtl::TextureSourceMode::Array);
        perm.SetRoughnessTextureArrayMode(rough_mode == mtl::TextureSourceMode::Array);

        std::string vs_path = desc.vs_template_path.empty()
            ? GetCompositorVSPath(key.surface_type, key.pass_hint)
            : shader_lib_path_ + "/" + desc.vs_template_path;
        std::string fs_path = desc.fs_template_path.empty()
            ? GetCompositorFSPath(key.surface_type, key.blend_mode, key.pass_hint)
            : shader_lib_path_ + "/" + desc.fs_template_path;
        std::string surface_rel = desc.surface_function_path.empty()
            ? GetSurfaceFunctionPath(key.surface_type, perm.GetTextureArrayMode())
            : desc.surface_function_path;

        std::string vs_source;
        if (!ReadFile(vs_path, vs_source, result.error_message))
        {
            result.success = false;
            return result;
        }

        std::string fs_source;
        if (!ReadFile(fs_path, fs_source, result.error_message))
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
