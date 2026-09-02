#include <hgl/mtl/CompositorAssembler.h>
#include <hgl/mtl/ShaderDocument.h>
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

            ShaderDocument document;
            ShaderDocumentDiagnostics diagnostics;
            ShaderDocumentSource source;
            source.stage = "fragment";
            source.logical_name = "CompositorModuleOptions";
            document.Add(
                ShaderDocumentBlockKind::Define,
                AnsiString(defines.c_str()),
                source);
            AnsiString serialized;
            if (!document.SerializeFragment(serialized, diagnostics))
                return {};
            return std::string(serialized.c_str(), serialized.Length());
        }

        std::string IncludeLine(const std::string &path)
        {
            return "#include \"" + path + "\"\n";
        }

        void AddDocumentBlock(
            ShaderDocument &document,
            const ShaderDocumentBlockKind kind,
            const std::string &text,
            const char *logical_name,
            const char *module = nullptr,
            const char *path = nullptr)
        {
            if (text.empty())
                return;

            ShaderDocumentSource source;
            source.stage = "fragment";
            source.logical_name = logical_name;
            if (module)
                source.module = module;
            if (path)
                source.path = path;
            document.Add(kind, AnsiString(text.c_str()), source);
        }

        void AddCompositorDiagnostic(
            ShaderDocumentDiagnostics &diagnostics,
            const char *message)
        {
            ShaderDocumentDiagnostic *diagnostic = diagnostics.Create();
            diagnostic->code = "compositor-assemble";
            diagnostic->message = message;
            diagnostic->block_index = -1;
            diagnostic->source.stage = "fragment";
            diagnostic->source.logical_name = "CompositorAssembler";
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
            ShaderDocument &out_document)
        {
            if (!coverage.requires_alpha_evaluation)
            {
                AddDocumentBlock(
                    out_document,
                    ShaderDocumentBlockKind::MainBody,
                    "void main()\n{\n}\n",
                    "CompositorAssembler.CoverageMain");
                return;
            }

            AddDocumentBlock(
                out_document,
                ShaderDocumentBlockKind::Resource,
                "#include \"common/surface_interface.glsl\"\n",
                "CompositorAssembler.CoverageResource",
                nullptr,
                "common/surface_interface.glsl");
            AddDocumentBlock(
                out_document,
                ShaderDocumentBlockKind::Function,
                "#include \"common/alpha_compositor.glsl\"\n",
                "CompositorAssembler.CoverageAlphaFunction",
                nullptr,
                "common/alpha_compositor.glsl");
            AddDocumentBlock(
                out_document,
                ShaderDocumentBlockKind::Define,
                "#define HGL_COVERAGE_ONLY 1\n",
                "CompositorAssembler.CoverageDefines");
            if (material_source_module && material_source_module[0])
                AddDocumentBlock(
                    out_document,
                    ShaderDocumentBlockKind::Module,
                    IncludeLine(material_source_module),
                    "CompositorAssembler.CoverageMaterialSource",
                    material_source_module,
                    material_source_module);
            if (surface_module && surface_module[0])
                AddDocumentBlock(
                    out_document,
                    ShaderDocumentBlockKind::Function,
                    IncludeLine(surface_module),
                    "CompositorAssembler.CoverageSurfaceFunction",
                    surface_module,
                    surface_module);

            std::string main_body = "\nvoid main()\n{\n";
            main_body += "    SurfaceInput si;\n";
            main_body += "    si.worldPos = vec3(0.0);\n";
            main_body += "    si.worldNormal = vec3(0.0, 0.0, 1.0);\n";
            main_body += "    si.uv0 = vec2(0.0);\n";
            main_body += "    si.uv1 = vec2(0.0);\n";
            main_body += "    si.vertexColor = vec4(1.0);\n";
            main_body += "    si.viewDir = vec3(0.0, 0.0, 1.0);\n";
            main_body += "    si.screenPos = gl_FragCoord.xy;\n";
            main_body += "    si.luminance = 1.0;\n";
            main_body += "    si.styleID = 0u;\n";

            const auto has_semantic =
                [&stage_interface](const InterStageSemantic semantic)
            {
                return FindMaterialStageInterfaceEntry(stage_interface, semantic)
                    != nullptr;
            };
            if (has_semantic(InterStageSemantic::WorldPosition))
                main_body += "    si.worldPos = fragWorldPos;\n";
            if (has_semantic(InterStageSemantic::WorldNormal))
                main_body += "    si.worldNormal = fragWorldNormal;\n";
            if (has_semantic(InterStageSemantic::UV0))
                main_body += "    si.uv0 = fragUV0;\n";
            if (has_semantic(InterStageSemantic::Color))
                main_body += "    si.vertexColor = fragVertexColor;\n";
            if (has_semantic(InterStageSemantic::Luminance))
                main_body += "    si.luminance = fragLuminance;\n";

            main_body += "    const float alpha = EvalAlpha(si, ";
            main_body += has_semantic(InterStageSemantic::DataIndexID)
                ? "fragDataIndexID" : "0u";
            main_body += ");\n";
            main_body += "    HGLApplyAlpha(alpha);\n";
            main_body += "}\n";
            AddDocumentBlock(
                out_document,
                ShaderDocumentBlockKind::MainBody,
                main_body,
                "CompositorAssembler.CoverageMain");
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
        ShaderDocument document;
        ShaderDocumentDiagnostics diagnostics;
        if (!AssembleDocument(
                surface,
                pass,
                fragment_source_override,
                surface_function_override,
                module_options,
                code_module_glsl,
                document,
                diagnostics))
        {
            if (diagnostics.GetCount() > 0)
                result.error_message = diagnostics[0]->message.c_str();
            return result;
        }

        AnsiString serialized;
        if (!document.SerializeFragment(serialized, diagnostics))
        {
            if (diagnostics.GetCount() > 0)
                result.error_message = diagnostics[0]->message.c_str();
            return result;
        }

        result.fragment_glsl.assign(
            serialized.c_str(),
            static_cast<size_t>(serialized.Length()));
        result.success = true;
        return result;
    }

    bool CompositorAssembler::AssembleDocument(
        const SurfaceType surface,
        const PassType pass,
        const char *fragment_source_override,
        const char *surface_function_override,
        const CompositorModuleOptions &module_options,
        const std::string &code_module_glsl,
        ShaderDocument &out_document,
        ShaderDocumentDiagnostics &out_diagnostics) const
    {
        out_document.Clear();
        out_diagnostics.Clear();

        Skeleton skeleton;
        const std::string skeleton_path =
            (fragment_source_override && fragment_source_override[0])
                ? fragment_source_override
                : (surface == SurfaceType::Sky
                    ? (pass == PassType::ShadowOpaque
                    || pass == PassType::ShadowMasked
                    || pass == PassType::EarlyZSolid
                    || pass == PassType::EarlyZMasked
                        ? ""
                        : SkySkeletonKey)
                    : (pass == PassType::ShadowOpaque
                    || pass == PassType::ShadowMasked
                    || pass == PassType::EarlyZSolid
                    || pass == PassType::EarlyZMasked
                        ? DepthOnlySkeletonKey
                        : ForwardSurfaceSkeletonKey));
        if (skeleton_path == ForwardSurfaceSkeletonKey)
            skeleton = Skeleton::ForwardSurface;
        else if (skeleton_path == DepthOnlySkeletonKey)
            skeleton = Skeleton::DepthOnly;
        else if (skeleton_path == SkySkeletonKey)
            skeleton = Skeleton::Sky;
        else
        {
            const std::string error = skeleton_path.empty()
                ? "Unsupported compositor pass"
                : "Unsupported compositor fragment source: " + skeleton_path;
            AddCompositorDiagnostic(out_diagnostics, error.c_str());
            return false;
        }

        CompositorModuleOptions options = module_options;
        const bool is_lit_surface =
            surface != SurfaceType::Unlit && surface != SurfaceType::Sky;
        options.enable_material_source_provider =
            options.enable_material_source_provider || is_lit_surface
            || (options.material_source_module && options.material_source_module[0]);
        options.enable_ntb_provider =
            options.enable_ntb_provider || is_lit_surface
            || (options.ntb_module && options.ntb_module[0]);
        options.enable_scene_lighting =
            options.enable_scene_lighting || is_lit_surface;

        const std::string surface_rel =
            (surface_function_override && surface_function_override[0])
                ? surface_function_override
                : GetSurfaceFunctionPath(surface);
        if (surface_rel.empty())
        {
            AddCompositorDiagnostic(out_diagnostics, "Unsupported compositor surface");
            return false;
        }

        std::string input_declarations;
        if (!BuildInputDeclarations(
                module_options.fragment_inputs, input_declarations))
        {
            AddCompositorDiagnostic(
                out_diagnostics,
                "Failed to apply fragment stage interface contract");
            return false;
        }

        OutputContract default_output_contract{};
        MaterialOutputContractDiagnostic output_diagnostic{};
        const OutputContract *output_contract = module_options.output_contract;
        if (!output_contract)
        {
            if (!BuildMaterialOutputContract(
                    pass, default_output_contract, output_diagnostic))
            {
                const std::string error =
                    std::string("Failed to build output contract: ")
                    + GetMaterialOutputContractErrorName(output_diagnostic.error);
                AddCompositorDiagnostic(out_diagnostics, error.c_str());
                return false;
            }
            output_contract = &default_output_contract;
        }

        std::string output_declarations;
        if (!BuildOutputDeclarations(
                *output_contract, output_declarations, output_diagnostic))
        {
            const std::string error =
                std::string("Failed to apply output contract: ")
                + GetMaterialOutputContractErrorName(output_diagnostic.error);
            AddCompositorDiagnostic(out_diagnostics, error.c_str());
            return false;
        }

        const MaterialCoverageContract default_coverage_contract{};
        const MaterialCoverageContract *coverage_contract =
            module_options.coverage_contract
                ? module_options.coverage_contract
                : &default_coverage_contract;

        // Keep historical emission order rather than normalizing blocks: the
        // compatibility serializer must preserve the legacy GLSL byte stream.
        AddDocumentBlock(
            out_document,
            ShaderDocumentBlockKind::Version,
            "#version 450\n",
            "CompositorAssembler.Version");
        const std::string defines = BuildOptionDefines(options);
        AddDocumentBlock(
            out_document,
            ShaderDocumentBlockKind::Define,
            defines + "\n",
            "CompositorAssembler.Defines");

        const auto add_resource_include =
            [&out_document](
                const char *logical_name,
                const std::string &path,
                const bool trailing_blank_line = false)
            {
                std::string text = IncludeLine(path);
                if (trailing_blank_line)
                    text += "\n";
                AddDocumentBlock(
                    out_document,
                    ShaderDocumentBlockKind::Resource,
                    text,
                    logical_name,
                    path.c_str(),
                    path.c_str());
            };
        const auto add_interfaces =
            [&out_document, &input_declarations, &output_declarations]()
            {
                AddDocumentBlock(
                    out_document,
                    ShaderDocumentBlockKind::Interface,
                    input_declarations,
                    "CompositorAssembler.FragmentInputs");
                AddDocumentBlock(
                    out_document,
                    ShaderDocumentBlockKind::Interface,
                    "\n" + output_declarations + "\n",
                    "CompositorAssembler.OutputInterfaces");
            };

        if (skeleton == Skeleton::ForwardSurface)
        {
            add_resource_include(
                "CompositorAssembler.DescriptorMacros",
                "common/descriptor_macros.glsl");
            add_resource_include(
                "CompositorAssembler.SurfaceInterface",
                "common/surface_interface.glsl");
            if (options.enable_scene_lighting)
            {
                add_resource_include(
                    "CompositorAssembler.CameraInfo", "ubo/camera_info.glsl");
                add_resource_include(
                    "CompositorAssembler.SkyInfo", "ubo/sky_info.glsl");
                AddDocumentBlock(
                    out_document,
                    ShaderDocumentBlockKind::Resource,
                    "SCENE_CAMERA_UBO;\n",
                    "CompositorAssembler.CameraUBO");
                AddDocumentBlock(
                    out_document,
                    ShaderDocumentBlockKind::Resource,
                    "SCENE_SKY_UBO;\n",
                    "CompositorAssembler.SkyUBO");
                add_resource_include(
                    "CompositorAssembler.SkyModule",
                    SlotPath(options.sky_module, "sky/sky_atmosphere.glsl"));
                add_resource_include(
                    "CompositorAssembler.DirectLightingModule",
                    SlotPath(
                        options.direct_lighting_module,
                        "lighting/direct_cook_torrance_pbr.glsl"));
                add_resource_include(
                    "CompositorAssembler.IndirectLightingModule",
                    SlotPath(
                        options.indirect_lighting_module,
                        "lighting/indirect_sky_ambient.glsl"));
            }
            add_resource_include(
                "CompositorAssembler.LightingAlgorithmModule",
                SlotPath(
                    options.lighting_algorithm_module,
                    "lighting/forward_pbr.glsl"));
            if (options.enable_material_source_provider)
                add_resource_include(
                    "CompositorAssembler.MaterialSourceModule",
                    SlotPath(
                        options.material_source_module,
                        "material/pbr_surface_source.glsl"));
            if (options.enable_ntb_provider)
                add_resource_include(
                    "CompositorAssembler.NTBModule",
                    SlotPath(options.ntb_module, "ntb/ntb_tangent_vbo_normalmap.glsl"));
            add_resource_include(
                "CompositorAssembler.ForwardLightingModule",
                SlotPath(
                    options.forward_lighting_module,
                    "compositor/forward_lighting.glsl"),
                true);
            add_interfaces();
            AddDocumentBlock(
                out_document,
                ShaderDocumentBlockKind::Module,
                code_module_glsl,
                "CompositorAssembler.CodeModule");
            AddDocumentBlock(
                out_document,
                ShaderDocumentBlockKind::Function,
                IncludeLine(surface_rel),
                "CompositorAssembler.SurfaceFunction",
                surface_rel.c_str(),
                surface_rel.c_str());
            AddDocumentBlock(
                out_document,
                ShaderDocumentBlockKind::Function,
                "#include \"common/alpha_compositor.glsl\"\n",
                "CompositorAssembler.AlphaFunction",
                nullptr,
                "common/alpha_compositor.glsl");

            std::string main_body = "\nvoid main()\n{\n";
            if (module_options.fragment_inputs)
            {
                AnsiString wiring;
                BuildGLSLMaterialSurfaceInput(
                    *module_options.fragment_inputs,
                    options.enable_scene_lighting,
                    wiring);
                main_body += wiring.c_str();
            }
            main_body += ForwardSurfaceMainTail;
            AddDocumentBlock(
                out_document,
                ShaderDocumentBlockKind::MainBody,
                main_body,
                "CompositorAssembler.ForwardMain");
        }
        else if (skeleton == Skeleton::DepthOnly)
        {
            AddDocumentBlock(
                out_document,
                ShaderDocumentBlockKind::Module,
                code_module_glsl,
                "CompositorAssembler.CodeModule");
            AddDocumentBlock(
                out_document,
                ShaderDocumentBlockKind::Interface,
                input_declarations,
                "CompositorAssembler.FragmentInputs");
            AddDocumentBlock(
                out_document,
                ShaderDocumentBlockKind::Interface,
                "\n" + output_declarations,
                "CompositorAssembler.OutputInterfaces");
            AppendCoverageMain(
                *coverage_contract,
                module_options.fragment_inputs
                    ? *module_options.fragment_inputs
                    : hgl::ValueArray<InterStageSemanticContractEntry>{},
                module_options.material_source_module,
                surface_rel.c_str(),
                out_document);
        }
        else
        {
            add_resource_include(
                "CompositorAssembler.DescriptorMacros",
                "common/descriptor_macros.glsl");
            add_resource_include(
                "CompositorAssembler.SkyInfo", "ubo/sky_info.glsl");
            AddDocumentBlock(
                out_document,
                ShaderDocumentBlockKind::Resource,
                "SCENE_SKY_UBO;\n",
                "CompositorAssembler.SkyUBO");
            add_resource_include(
                "CompositorAssembler.SkyModule",
                "sky/sky_atmosphere.glsl");
            AddDocumentBlock(
                out_document,
                ShaderDocumentBlockKind::Resource,
                "\n#include \"common/surface_interface.glsl\"\n",
                "CompositorAssembler.SurfaceInterface",
                nullptr,
                "common/surface_interface.glsl");
            AddDocumentBlock(
                out_document,
                ShaderDocumentBlockKind::Function,
                IncludeLine(surface_rel),
                "CompositorAssembler.SurfaceFunction",
                surface_rel.c_str(),
                surface_rel.c_str());
            AddDocumentBlock(
                out_document,
                ShaderDocumentBlockKind::Function,
                "#include \"common/alpha_compositor.glsl\"\n\n",
                "CompositorAssembler.AlphaFunction",
                nullptr,
                "common/alpha_compositor.glsl");
            add_interfaces();
            AddDocumentBlock(
                out_document,
                ShaderDocumentBlockKind::MainBody,
                std::string("\nvoid main()\n{\n") + SkyMainBody,
                "CompositorAssembler.SkyMain");
        }

        return true;
    }
}
