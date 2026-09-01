#include <hgl/mtl/CompositorAssembler.h>
#include <hgl/log/Log.h>

namespace hgl::graph::mtl
{
    namespace
    {
        // ── 骨架选择键（TOML fragment.source 引用）────────────────────
        constexpr const char ForwardSurfaceSkeletonKey[] = "forward_surface";
        constexpr const char DepthOnlySkeletonKey[]      = "depth_only";
        constexpr const char SkySkeletonKey[]            = "forward_sky";

        enum class Skeleton
        {
            ForwardSurface,
            DepthOnly,
            Sky
        };

        // ── 骨架段常量（原模板的不可推导部分，语义逐字保留）────────────────

        // Forward Surface main 尾段（契约驱动的 si 装配之后）
        constexpr const char ForwardSurfaceMainTail[] =
            "    const SurfaceOutput surface =\n"
            "        EvalSurface(si, materialDataIndex);\n"
            "    const LightingInput lighting =\n"
            "        BuildForwardLightingInput(surface, si);\n"
            "    const vec4 finalColor = EvalLighting(lighting);\n"
            "    WriteMaterialOutput(HGLComposeColor(finalColor));\n"
            "}\n";

        // Sky main 体（静态 si 装配——天空只有方向，无逐语义输入）
        constexpr const char SkyMainBody[] =
            "    SurfaceInput si;\n"
            "    si.worldPos     = fragDirection;\n"
            "    si.worldNormal  = normalize(fragDirection);\n"
            "    si.uv0          = vec2(0.0);\n"
            "    si.uv1          = vec2(0.0);\n"
            "    si.vertexColor  = vec4(1.0);\n"
            "    si.viewDir      = fragDirection;\n"
            "    si.screenPos    = vec2(0.0);\n"
            "    si.luminance    = 0.0;\n"
            "\n"
            "    SurfaceOutput so = EvalSurface(si, 0u);\n"
            "\n"
            "    WriteMaterialOutput(HGLComposeColor(vec4(so.baseColor, so.alpha)));\n"
            "}\n";

        // ── 发射辅助 ────────────────────────────────────────────────────────

        // HGL_* 编译宏（原 InjectDefines 的 define 段，顺序与文本一致）
        std::string BuildOptionDefines(const CompositorAssembler::CompositorModuleOptions &options)
        {
            std::string defines;
            if (options.alpha_test)
            {
                defines += "#define HGL_ALPHA_TEST 1\n";
                defines += "#define HGL_ALPHA_CUTOFF ";
                defines += std::to_string(options.alpha_cutoff);
                defines += "\n";
            }
            if (options.dither)
                defines += "#define HGL_ALPHA_DITHER 1\n";
            defines += options.enable_material_source_provider
                ? "#define HGL_USE_MATERIAL_SOURCE_PROVIDER 1\n"
                : "#define HGL_USE_MATERIAL_SOURCE_PROVIDER 0\n";
            defines += options.enable_ntb_provider
                ? "#define HGL_USE_NTB_PROVIDER 1\n"
                : "#define HGL_USE_NTB_PROVIDER 0\n";
            defines += options.enable_scene_lighting
                ? "#define HGL_USE_SCENE_LIGHTING 1\n"
                : "#define HGL_USE_SCENE_LIGHTING 0\n";
            return defines;
        }

        std::string IncludeLine(const std::string &path)
        {
            return "#include \"" + path + "\"\n";
        }

        // 槽位最终路径：override 优先，否则默认模块（原 kModuleSlots 语义）
        std::string SlotPath(const char *override_path, const char *default_path)
        {
            return (override_path && override_path[0]) ? override_path : default_path;
        }

        // fragment 输入声明（契约驱动；原 ApplyFragmentInputContract 的插入段）
        bool BuildInputDeclarations(
            const hgl::ValueArray<InterStageSemanticContractEntry> *inputs,
            std::string &out_declarations)
        {
            out_declarations.clear();
            if (!inputs)
                return true;

            for (int i = 0; i < inputs->GetCount(); ++i)
            {
                AnsiString declaration;
                if (!BuildGLSLInterStageDeclaration(
                        (*inputs)[i], "in", declaration))
                    return false;
                out_declarations += declaration.c_str();
                out_declarations += "\n";
            }
            return true;
        }

        // 输出附件声明 + WriteMaterialOutput 转发（契约驱动；
        // 原 ApplyMaterialOutputContract 的生成段，文本一致）
        bool BuildOutputDeclarations(
            const OutputContract &contract,
            std::string &out_declarations,
            MaterialOutputContractDiagnostic &out_diagnostic)
        {
            out_declarations.clear();
            out_diagnostic = {};
            out_diagnostic.purpose = contract.purpose;
            if (!ValidateOutputContract(contract))
            {
                out_diagnostic.error = MaterialOutputContractError::InvalidContract;
                return false;
            }

            for (int i = 0; i < contract.attachments.GetCount(); ++i)
            {
                const ShaderOutputAttachmentContract &attachment =
                    contract.attachments[i];
                const char *type_name;
                switch (attachment.value_type)
                {
                case ShaderStageValueType::Float: type_name = "float"; break;
                case ShaderStageValueType::Vec2:  type_name = "vec2";  break;
                case ShaderStageValueType::Vec3:  type_name = "vec3";  break;
                case ShaderStageValueType::Vec4:  type_name = "vec4";  break;
                case ShaderStageValueType::Int:   type_name = "int";   break;
                case ShaderStageValueType::UInt:  type_name = "uint";  break;
                case ShaderStageValueType::Bool:  type_name = "bool";  break;
                default: type_name = nullptr; break;
                }

                // 输出语义名解析（outColor 单语义，共享实现见 MaterialOutputContract.h）
                const char *output_name =
                    GetMaterialOutputName(attachment.write_semantic_id);
                if (!type_name || !output_name)
                {
                    out_diagnostic.error =
                        MaterialOutputContractError::UnsupportedAttachment;
                    out_diagnostic.attachment_semantic_id =
                        attachment.write_semantic_id;
                    return false;
                }

                out_declarations += "layout(location=";
                out_declarations += std::to_string(attachment.location);
                out_declarations += ") out ";
                out_declarations += type_name;
                out_declarations += " ";
                out_declarations += output_name;
                out_declarations += ";\n";
                out_declarations += "void WriteMaterialOutput(";
                out_declarations += type_name;
                out_declarations += " value) { ";
                out_declarations += output_name;
                out_declarations += " = value; }\n";
            }
            return true;
        }

        // depth/shadow 覆盖率 main（原 ApplyDepthCoverageContract 的生成段，
        // 逐行一致；si 装配由覆盖契约语义驱动）
        void AppendCoverageMain(
            const MaterialCoverageContract &coverage,
            const hgl::ValueArray<InterStageSemanticContractEntry> &stage_interface,
            const char *material_source_module,
            const char *surface_module,
            std::string &out_glsl)
        {
            if (!coverage.requires_alpha_evaluation)
            {
                out_glsl += "void main()\n{\n}\n";
                return;
            }

            out_glsl += "#include \"common/surface_interface.glsl\"\n";
            out_glsl += "#include \"common/alpha_compositor.glsl\"\n";
            out_glsl += "#define HGL_COVERAGE_ONLY 1\n";
            if (material_source_module && material_source_module[0])
                out_glsl += IncludeLine(material_source_module);
            if (surface_module && surface_module[0])
                out_glsl += IncludeLine(surface_module);
            out_glsl += "\nvoid main()\n{\n";
            out_glsl += "    SurfaceInput si;\n";
            out_glsl += "    si.worldPos = vec3(0.0);\n";
            out_glsl += "    si.worldNormal = vec3(0.0, 0.0, 1.0);\n";
            out_glsl += "    si.uv0 = vec2(0.0);\n";
            out_glsl += "    si.uv1 = vec2(0.0);\n";
            out_glsl += "    si.vertexColor = vec4(1.0);\n";
            out_glsl += "    si.viewDir = vec3(0.0, 0.0, 1.0);\n";
            out_glsl += "    si.screenPos = gl_FragCoord.xy;\n";
            out_glsl += "    si.luminance = 1.0;\n";
            out_glsl += "    si.styleID = 0u;\n";

            const auto has_semantic =
                [&stage_interface](const InterStageSemantic semantic)
            {
                return FindMaterialStageInterfaceEntry(stage_interface, semantic)
                    != nullptr;
            };
            if (has_semantic(InterStageSemantic::WorldPosition))
                out_glsl += "    si.worldPos = fragWorldPos;\n";
            if (has_semantic(InterStageSemantic::WorldNormal))
                out_glsl += "    si.worldNormal = fragWorldNormal;\n";
            if (has_semantic(InterStageSemantic::UV0))
                out_glsl += "    si.uv0 = fragUV0;\n";
            if (has_semantic(InterStageSemantic::Color))
                out_glsl += "    si.vertexColor = fragVertexColor;\n";
            if (has_semantic(InterStageSemantic::Luminance))
                out_glsl += "    si.luminance = fragLuminance;\n";

            out_glsl += "    const float alpha = EvalAlpha(si, ";
            out_glsl += has_semantic(InterStageSemantic::DataIndexID)
                ? "fragDataIndexID" : "0u";
            out_glsl += ");\n";
            out_glsl += "    HGLApplyAlpha(alpha);\n";
            out_glsl += "}\n";
        }
    }

    std::string CompositorAssembler::GetSurfaceFunctionPath(SurfaceType surface) const
    {
        switch (surface)
        {
        case SurfaceType::Lit:        return "surface/material_surface.glsl";
        case SurfaceType::Unlit:      return "surface/material_surface.glsl";
        case SurfaceType::Sky:        return "surface/sky_minimal_surface.glsl";
        default:                      return {};
        }
    }

    CompositorAssembler::AssembleResult CompositorAssembler::Assemble(
        SurfaceType                  surface,
        PassType                     pass,
        const char                  *fragment_source_override,
        const char                  *surface_function_override,
        const CompositorModuleOptions &module_options,
        const std::string           &code_module_glsl) const
    {
        AssembleResult result{};

        // ── 1. 骨架选择（原 GetCompositorFSPath + override 覆盖）───────────
        Skeleton skeleton;
        {
            const std::string path =
                (fragment_source_override && fragment_source_override[0])
                    ? fragment_source_override
                    : (surface == SurfaceType::Sky
                        ? (pass == PassType::ShadowOpaque
                        || pass == PassType::ShadowMasked
                        || pass == PassType::EarlyZSolid
                        || pass == PassType::EarlyZMasked
                            ? ""                 // Sky 无深度/阴影 pass（原返回空 → 报错）
                            : SkySkeletonKey)
                        : (pass == PassType::ShadowOpaque
                        || pass == PassType::ShadowMasked
                        || pass == PassType::EarlyZSolid
                        || pass == PassType::EarlyZMasked
                            ? DepthOnlySkeletonKey
                            : ForwardSurfaceSkeletonKey));

            if (path == ForwardSurfaceSkeletonKey)
                skeleton = Skeleton::ForwardSurface;
            else if (path == DepthOnlySkeletonKey)
                skeleton = Skeleton::DepthOnly;
            else if (path == SkySkeletonKey)
                skeleton = Skeleton::Sky;
            else if (path.empty())
            {
                result.error_message = "Unsupported compositor pass";
                return result;
            }
            else
            {
                result.error_message =
                    "Unsupported compositor fragment source: " + path;
                return result;
            }
        }

        // ── 2. 有效化选项（原 Assemble 的 lit 强制逻辑）────────────────────
        CompositorModuleOptions options = module_options;
        const bool is_lit_surface =
            surface != SurfaceType::Unlit && surface != SurfaceType::Sky;
        options.enable_material_source_provider =
            options.enable_material_source_provider
            || is_lit_surface
            || (options.material_source_module
                && options.material_source_module[0]);
        options.enable_ntb_provider =
            options.enable_ntb_provider
            || is_lit_surface
            || (options.ntb_module
                && options.ntb_module[0]);
        options.enable_scene_lighting =
            options.enable_scene_lighting
            || is_lit_surface;

        // ── 3. 输出用途 / surface 函数路径 ─────────────────────────────────
        const mtl::ShaderProgramPurpose output_purpose =
            module_options.output_contract
                ? module_options.output_contract->purpose
                : GetShaderProgramPurpose(pass);

        std::string surface_rel = (surface_function_override && surface_function_override[0])
            ? surface_function_override
            : GetSurfaceFunctionPath(surface);
        if (surface_rel.empty())
        {
            result.error_message = "Unsupported compositor surface";
            return result;
        }

        // ── 4. 契约数据（输入声明 / 输出附件 / 覆盖率）─────────────────────
        std::string input_declarations;
        if (!BuildInputDeclarations(module_options.fragment_inputs, input_declarations))
        {
            result.error_message =
                "Failed to apply fragment stage interface contract";
            return result;
        }

        OutputContract default_output_contract{};
        mtl::MaterialOutputContractDiagnostic output_diagnostic{};
        const OutputContract *output_contract =
            module_options.output_contract;
        if (!output_contract)
        {
            if (!BuildMaterialOutputContract(
                    pass,
                    default_output_contract,
                    output_diagnostic))
            {
                result.error_message =
                    std::string("Failed to build output contract: ")
                    + GetMaterialOutputContractErrorName(
                        output_diagnostic.error);
                return result;
            }
            output_contract = &default_output_contract;
        }

        std::string output_declarations;
        if (!BuildOutputDeclarations(
                *output_contract,
                output_declarations,
                output_diagnostic))
        {
            result.error_message =
                std::string("Failed to apply output contract: ")
                + GetMaterialOutputContractErrorName(output_diagnostic.error);
            return result;
        }

        mtl::MaterialCoverageContract default_coverage_contract{};
        const mtl::MaterialCoverageContract *coverage_contract =
            module_options.coverage_contract
                ? module_options.coverage_contract
                : &default_coverage_contract;

        // ── 5. 单趟组装 ────────────────────────────────────────────────────
        std::string glsl = "#version 450\n";
        const std::string defines = BuildOptionDefines(options);
        glsl += defines;
        if (!defines.empty())
            glsl += "\n";

        if (skeleton == Skeleton::ForwardSurface)
        {
            glsl += "#include \"common/descriptor_macros.glsl\"\n";
            glsl += "#include \"common/surface_interface.glsl\"\n";

            if (options.enable_scene_lighting)
            {
                glsl += "#include \"ubo/camera_info.glsl\"\n";
                glsl += "#include \"ubo/sky_info.glsl\"\n";
                glsl += "SCENE_CAMERA_UBO;\n";
                glsl += "SCENE_SKY_UBO;\n";
                glsl += IncludeLine(SlotPath(
                    options.sky_module, "sky/sky_atmosphere.glsl"));
                glsl += IncludeLine(SlotPath(
                    options.direct_lighting_module,
                    "lighting/direct_cook_torrance_pbr.glsl"));
                glsl += IncludeLine(SlotPath(
                    options.indirect_lighting_module,
                    "lighting/indirect_sky_ambient.glsl"));
            }

            glsl += IncludeLine(SlotPath(
                options.lighting_algorithm_module, "lighting/forward_pbr.glsl"));
            if (options.enable_material_source_provider)
                glsl += IncludeLine(SlotPath(
                    options.material_source_module,
                    "material/pbr_surface_source.glsl"));
            if (options.enable_ntb_provider)
                glsl += IncludeLine(SlotPath(
                    options.ntb_module, "ntb/ntb_tangent_vbo_normalmap.glsl"));
            glsl += IncludeLine(SlotPath(
                options.forward_lighting_module, "compositor/forward_lighting.glsl"));

            glsl += "\n";
            glsl += input_declarations;
            glsl += "\n";
            glsl += output_declarations;
            glsl += "\n";

            // 代码模块置于 surface 函数 include 之前——模块代码可能提供
            // EvalMaterialSource 等被 surface 函数调用的定义（原
            // BeforeSurfaceFunction 注入点语义）
            glsl += code_module_glsl;

            glsl += IncludeLine(surface_rel);
            glsl += "#include \"common/alpha_compositor.glsl\"\n";

            glsl += "\nvoid main()\n{\n";
            if (module_options.fragment_inputs)
            {
                AnsiString wiring;
                BuildGLSLMaterialSurfaceInput(
                    *module_options.fragment_inputs,
                    options.enable_scene_lighting,
                    wiring);
                glsl += wiring.c_str();
            }
            glsl += ForwardSurfaceMainTail;
        }
        else if (skeleton == Skeleton::DepthOnly)
        {
            // 代码模块紧跟 defines（原无 marker 模板的 AfterVersion 注入位置）
            glsl += code_module_glsl;
            glsl += input_declarations;
            glsl += "\n";
            glsl += output_declarations;

            // 覆盖率契约：非 alpha 评估变体为空 main；否则按语义装配 EvalAlpha
            AppendCoverageMain(
                *coverage_contract,
                module_options.fragment_inputs
                    ? *module_options.fragment_inputs
                    : hgl::ValueArray<InterStageSemanticContractEntry>{},
                module_options.material_source_module,
                surface_rel.c_str(),
                glsl);
        }
        else // Skeleton::Sky
        {
            glsl += "#include \"common/descriptor_macros.glsl\"\n";
            glsl += "#include \"ubo/sky_info.glsl\"\n";
            glsl += "SCENE_SKY_UBO;\n";
            glsl += "#include \"sky/sky_atmosphere.glsl\"\n";

            glsl += "\n";
            glsl += "#include \"common/surface_interface.glsl\"\n";
            glsl += IncludeLine(surface_rel);
            glsl += "#include \"common/alpha_compositor.glsl\"\n";

            glsl += "\n";
            glsl += input_declarations;
            glsl += "\n";
            glsl += output_declarations;
            glsl += "\n";

            glsl += "void main()\n{\n";
            glsl += SkyMainBody;
        }

        result.fragment_glsl = std::move(glsl);
        result.success       = true;
        return result;
    }
}
