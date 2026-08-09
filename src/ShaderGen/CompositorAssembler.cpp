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
                return shader_lib_path_
                    + "/compositor/main_depth_only.frag.glsl";

            case PassType::EarlyZSolid:
            case PassType::EarlyZMasked:
                return shader_lib_path_
                    + "/compositor/main_depth_only.frag.glsl";

            case PassType::ForwardDither:
            case PassType::ForwardA2C:
                return shader_lib_path_ + "/compositor/main_forward_unlit.frag.glsl";

            default:
                return shader_lib_path_ + "/compositor/main_forward_unlit.frag.glsl";
            }
        }

        if (surface == SurfaceType::Sky)
        {
            switch (pass)
            {
            case PassType::ForwardOpaque:
            case PassType::ForwardMasked:
            case PassType::ForwardTransparent:
            case PassType::ForwardDither:
            case PassType::ForwardA2C:
                return shader_lib_path_ + "/compositor/main_forward_sky.frag.glsl";

            case PassType::ShadowOpaque:
            case PassType::ShadowMasked:
            case PassType::EarlyZSolid:
            case PassType::EarlyZMasked:
            case PassType::VBufferID:
                return {};

            default:
                return shader_lib_path_ + "/compositor/main_forward_sky.frag.glsl";
            }
        }

        // 非 Unlit 走 Lit 路径
        switch (pass)
        {
        case PassType::ForwardOpaque:
            return shader_lib_path_ + "/compositor/main_forward_lit.frag.glsl";

        case PassType::ForwardMasked:
            return shader_lib_path_ + "/compositor/main_forward_lit.frag.glsl";

        case PassType::ForwardTransparent:
            return shader_lib_path_ + "/compositor/main_forward_lit.frag.glsl";

        case PassType::ForwardDither:
        case PassType::ForwardA2C:
            return shader_lib_path_ + "/compositor/main_forward_lit.frag.glsl";

        case PassType::ShadowOpaque:
        case PassType::ShadowMasked:
            return shader_lib_path_
                + "/compositor/main_depth_only.frag.glsl";

        case PassType::EarlyZSolid:
        case PassType::EarlyZMasked:
            return shader_lib_path_
                + "/compositor/main_depth_only.frag.glsl";

        case PassType::VBufferID:
            return {};

        default:
            return shader_lib_path_ + "/compositor/main_forward_lit.frag.glsl";
        }
    }

    std::string CompositorAssembler::GetSurfaceFunctionPath(SurfaceType surface) const
    {
        switch (surface)
        {
        case SurfaceType::Lit:        return "surface/lit_surface.glsl";
        case SurfaceType::Unlit:      return "surface/unlit_color3d_surface.glsl";
        case SurfaceType::Sky:        return "surface/sky_minimal_surface.glsl";
        // Reserved: Skin, Hair, Cloth, Eye, Foliage, ClearCoat, Water — not yet specialized, fall through to Lit.
        case SurfaceType::Skin:
        case SurfaceType::Hair:
        case SurfaceType::Cloth:
        case SurfaceType::Eye:
        case SurfaceType::Foliage:
        case SurfaceType::ClearCoat:
        case SurfaceType::Water:      return "surface/lit_surface.glsl";
        default:                      return {};
        }
    }

    std::string CompositorAssembler::InjectDefines(
        const std::string &source,
        const NewShaderPermutationKey &key,
        const CompositorModuleOptions &module_options) const
    {
        std::string defines;
        key.AppendGLSLDefines(defines);
        if (module_options.alpha_test)
        {
            defines += "#define HGL_ALPHA_TEST 1\n";
            defines += "#define HGL_ALPHA_CUTOFF ";
            defines += std::to_string(module_options.alpha_cutoff);
            defines += "\n";
        }
        if (module_options.dither)
            defines += "#define HGL_ALPHA_DITHER 1\n";
        if (module_options.enable_material_source_provider)
            defines += "#define HGL_USE_MATERIAL_SOURCE_PROVIDER 1\n";
        else
            defines += "#define HGL_USE_MATERIAL_SOURCE_PROVIDER 0\n";
        if (module_options.enable_ntb_provider)
            defines += "#define HGL_USE_NTB_PROVIDER 1\n";
        else
            defines += "#define HGL_USE_NTB_PROVIDER 0\n";

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

        // DirectInclude fragment files may omit #version because the old
        // MaterialLibrary path prepended it before compilation.
        return "#version 450\n" + defines + "\n" + source;
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
            module_options.lighting_algorithm_module && module_options.lighting_algorithm_module[0]
                ? module_options.lighting_algorithm_module
                : "lighting/forward_pbr.glsl",
            module_options.material_source_module && module_options.material_source_module[0]
                ? module_options.material_source_module
                : "material/pbr_surface_source.glsl",
            module_options.ntb_module && module_options.ntb_module[0]
                ? module_options.ntb_module
                : "ntb/ntb_tangent_vbo_normalmap.glsl",
            module_options.forward_lighting_module && module_options.forward_lighting_module[0]
                ? module_options.forward_lighting_module
                : "compositor/forward_lighting.glsl"
        };
        const char *default_paths[] =
        {
            "sky/sky_atmosphere.glsl",
            "lighting/direct_cook_torrance_pbr.glsl",
            "lighting/indirect_simple_ambient.glsl",
            "lighting/forward_pbr.glsl",
            "material/pbr_surface_source.glsl",
            "ntb/ntb_tangent_vbo_normalmap.glsl",
            "compositor/forward_lighting.glsl"
        };

        std::string result = source;
        for (size_t i = 0; i < 7; ++i)
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

    bool CompositorAssembler::ApplyFragmentInputContract(
        const std::string &source,
        const hgl::ValueArray<mtl::InterStageSemanticContractEntry> &inputs,
        std::string &out_source) const
    {
        out_source.clear();
        std::string declarations;
        for (int i = 0; i < inputs.GetCount(); ++i)
        {
            AnsiString declaration;
            if (!mtl::BuildGLSLInterStageDeclaration(
                    inputs[i], "in", declaration))
                return false;
            declarations += declaration.c_str();
            declarations += "\n";
        }

        out_source.reserve(source.size() + declarations.size() + 1);
        bool declarations_inserted = false;

        size_t line_start = 0;
        while (line_start < source.size())
        {
            const size_t line_end = source.find('\n', line_start);
            const size_t length = line_end == std::string::npos
                ? source.size() - line_start
                : line_end - line_start;
            const std::string line = source.substr(line_start, length);
            const bool is_fragment_input =
                line.find("layout(location=") != std::string::npos
             && line.find(" in ") != std::string::npos;
            if (is_fragment_input)
            {
                if (!declarations_inserted)
                {
                    out_source += declarations;
                    declarations_inserted = true;
                }
            }
            else
            {
                out_source.append(line);
                if (line_end != std::string::npos)
                    out_source.push_back('\n');
            }

            if (line_end == std::string::npos)
                break;
            line_start = line_end + 1;
        }

        const std::string input_marker =
            "// ULRE_FRAGMENT_INPUT_CONTRACT";
        const size_t marker = out_source.find(input_marker);
        if (!declarations_inserted)
        {
            if (!inputs.IsEmpty() && marker == std::string::npos)
                return false;
            if (marker != std::string::npos)
                out_source.replace(marker, input_marker.size(), declarations);
        }
        else if (marker != std::string::npos)
        {
            out_source.erase(marker, input_marker.size());
        }
        return true;
    }

    CompositorAssembler::AssembleResult CompositorAssembler::Assemble(
        SurfaceType                  surface,
        BlendMode                    blend,
        PassType                     pass,
        const char                  *fragment_source_override,
        const char                  *surface_function_override,
        const CompositorModuleOptions &module_options
    ) const
    {
        AssembleResult result{};

        // 1. 构建 permutation key
        NewShaderPermutationKey key;
        key.SetSurfaceType(surface);

        // 2. Resolve the canonical fragment source/template path.
        std::string fs_path = (fragment_source_override && fragment_source_override[0])
            ? shader_lib_path_ + "/" + fragment_source_override
            : GetCompositorFSPath(surface, blend, pass);
        std::string surface_rel = (surface_function_override && surface_function_override[0])
            ? surface_function_override
            : GetSurfaceFunctionPath(surface);

        if (fs_path.empty())
        {
            result.error_message = "Unsupported compositor pass";
            result.success = false;
            return result;
        }
        if (surface_rel.empty())
        {
            result.error_message = "Unsupported compositor surface";
            result.success = false;
            return result;
        }

        // 3. 读取 FS 模板
        std::string fs_source;
        if (!ReadFile(fs_path, fs_source, result.error_message))
        {
            result.success = false;
            return result;
        }

        // 4. 注入 #define
        CompositorModuleOptions effective_options = module_options;
        const bool is_lit_surface =
            surface != SurfaceType::Unlit && surface != SurfaceType::Sky;
        effective_options.enable_material_source_provider =
            effective_options.enable_material_source_provider
            || is_lit_surface
            || (effective_options.material_source_module
                && effective_options.material_source_module[0]);
        effective_options.enable_ntb_provider =
            effective_options.enable_ntb_provider
            || is_lit_surface
            || (effective_options.ntb_module
                && effective_options.ntb_module[0]);
        if (!effective_options.use_resolved_render_state)
        {
            effective_options.alpha_test = effective_options.alpha_test
                || blend == BlendMode::Masked
                || pass == PassType::ForwardMasked;
            effective_options.dither = effective_options.dither
                || blend == BlendMode::Dither
                || pass == PassType::ForwardDither;
        }
        fs_source = InjectDefines(fs_source, key, effective_options);

        // 5. Resolve configurable module includes to literal header names.
        // GLSL does not allow a macro to stand in for the header token of #include.
        fs_source = ReplaceLightingModuleIncludes(fs_source, module_options);

        // 6. 替换 FS 中的 SURFACE_FUNCTION_FILE
        fs_source = ReplaceSurfaceInclude(fs_source, surface_rel);

        mtl::MaterialCoverageContract default_coverage_contract{};
        const mtl::MaterialCoverageContract *coverage_contract =
            module_options.coverage_contract
                ? module_options.coverage_contract
                : &default_coverage_contract;
        const mtl::ShaderProgramPurpose output_purpose =
            module_options.output_contract
                ? module_options.output_contract->purpose
                : mtl::GetShaderProgramPurpose(pass);
        if (output_purpose == mtl::ShaderProgramPurpose::DepthOnly
         || output_purpose == mtl::ShaderProgramPurpose::ShadowDepth)
        {
            std::string covered_source;
            const hgl::ValueArray<
                mtl::InterStageSemanticContractEntry> empty_inputs;
            const auto &coverage_inputs =
                module_options.fragment_inputs
                    ? *module_options.fragment_inputs
                    : empty_inputs;
            if (!mtl::ApplyDepthCoverageContract(
                    *coverage_contract,
                    coverage_inputs,
                    module_options.material_source_module,
                    surface_rel.c_str(),
                    fs_source,
                    covered_source))
            {
                result.error_message =
                    "Failed to apply depth coverage contract";
                result.success = false;
                return result;
            }
            fs_source = std::move(covered_source);
        }

        if (module_options.fragment_inputs)
        {
            std::string contracted_source;
            if (!ApplyFragmentInputContract(
                    fs_source,
                    *module_options.fragment_inputs,
                    contracted_source))
            {
                result.error_message =
                    "Failed to apply fragment stage interface contract";
                result.success = false;
                return result;
            }
            fs_source = std::move(contracted_source);
        }

        mtl::OutputContract default_output_contract{};
        mtl::MaterialOutputContractDiagnostic output_diagnostic{};
        const mtl::OutputContract *output_contract =
            module_options.output_contract;
        if (!output_contract)
        {
            if (!mtl::BuildMaterialOutputContract(
                    pass,
                    default_output_contract,
                    output_diagnostic))
            {
                result.error_message =
                    std::string("Failed to build output contract: ")
                    + mtl::GetMaterialOutputContractErrorName(
                        output_diagnostic.error);
                result.success = false;
                return result;
            }
            output_contract = &default_output_contract;
        }

        std::string contracted_source;
        if (!mtl::ApplyMaterialOutputContract(
                *output_contract,
                fs_source,
                contracted_source,
                output_diagnostic))
        {
            result.error_message =
                std::string("Failed to apply output contract: ")
                + mtl::GetMaterialOutputContractErrorName(
                    output_diagnostic.error);
            result.success = false;
            return result;
        }
        fs_source = std::move(contracted_source);

        result.fragment_glsl = std::move(fs_source);
        result.success       = true;
        return result;
    }
}
