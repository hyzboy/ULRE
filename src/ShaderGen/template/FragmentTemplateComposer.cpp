#include <hgl/mtl/FragmentTemplateComposer.h>
#include <hgl/log/Log.h>
#include <hgl/mtl/ShaderCodeModuleRegistry.h>
#include <hgl/mtl/MaterialOutputContract.h>
#include <hgl/mtl/MaterialStageInterface.h>

namespace
{
    using namespace hgl::graph::mtl;
    using hgl::AnsiString;

    void AddTemplateBlock(
        ShaderDocument &document,
        const ShaderDocumentBlockKind kind,
        const AnsiString &text,
        const char *logical_name,
        const char *path = nullptr)
    {
        if (text.IsEmpty())
            return;
        ShaderDocumentSource source;
        source.stage = "fragment";
        source.logical_name = logical_name;
        if (path)
        {
            source.module = path;
            source.path = path;
        }
        document.Add(kind, text, source);
    }

    void AppendDocumentBlocks(
        ShaderDocument &out_document,
        const ShaderDocument *fragment)
    {
        if (!fragment)
            return;

        for (int index = 0; index < fragment->GetBlockCount(); ++index)
        {
            const ShaderDocumentBlock &block = fragment->GetBlock(index);
            out_document.Add(block.kind, block.text, block.source);
        }
    }

    AnsiString IncludeTemplate(const char *path)
    {
        return AnsiString("#include \"") + AnsiString(path) + AnsiString("\"\n");
    }

    // 输出附件类型 → GLSL 类型名。整数/布尔仅深度-only 模板（ShadowCaster）
    // 会用到，其余模板传 allow_integer_types=false 保持原有拒绝行为。
    const char *GetGLSLOutputTypeName(
        const ShaderStageValueType value_type,
        const bool allow_integer_types) noexcept
    {
        switch (value_type)
        {
        case ShaderStageValueType::Float: return "float";
        case ShaderStageValueType::Vec2:  return "vec2";
        case ShaderStageValueType::Vec3:  return "vec3";
        case ShaderStageValueType::Vec4:  return "vec4";
        case ShaderStageValueType::Int:   return allow_integer_types ? "int"  : nullptr;
        case ShaderStageValueType::UInt:  return allow_integer_types ? "uint" : nullptr;
        case ShaderStageValueType::Bool:  return allow_integer_types ? "bool" : nullptr;
        default:                          return nullptr;
        }
    }

    // 各模板共用的 fragment 输入（inter-stage varying）声明块。
    bool AppendFragmentInputDeclarations(
        const hgl::ValueArray<InterStageSemanticContractEntry> *fragment_inputs,
        const char *logical_name,
        ShaderDocument &document)
    {
        if (!fragment_inputs)
            return true;

        std::string declarations;
        for (int index = 0; index < fragment_inputs->GetCount(); ++index)
        {
            AnsiString declaration;
            if (!BuildGLSLInterStageDeclaration(
                    (*fragment_inputs)[index], "in", declaration))
                return false;

            declarations += declaration.c_str();
            declarations += "\n";
        }

        AddTemplateBlock(document, ShaderDocumentBlockKind::Interface,
                         AnsiString(declarations.c_str()), logical_name);
        return true;
    }

    // 各模板共用的输出附件声明块（layout(location=n) out ... + WriteMaterialOutput）。
    bool AppendOutputDeclarations(
        const OutputContract *output_contract,
        const char *logical_name,
        const bool allow_integer_types,
        ShaderDocument &document)
    {
        if (!output_contract)
            return true;

        for (int index = 0;
             index < output_contract->attachments.GetCount();
             ++index)
        {
            const ShaderOutputAttachmentContract &attachment =
                output_contract->attachments[index];

            const char *type_name =
                GetGLSLOutputTypeName(attachment.value_type,
                                      allow_integer_types);
            const char *output_name =
                GetMaterialOutputName(attachment.write_semantic_id);
            if (!type_name || !output_name)
                return false;

            std::string declaration = "layout(location=";
            declaration += std::to_string(attachment.location);
            declaration += ") out ";
            declaration += type_name;
            declaration += " ";
            declaration += output_name;
            declaration += ";\nvoid WriteMaterialOutput(";
            declaration += type_name;
            declaration += " value) { ";
            declaration += output_name;
            declaration += " = value; }\n";

            AddTemplateBlock(document, ShaderDocumentBlockKind::Interface,
                             AnsiString(declaration.c_str()), logical_name);
        }

        return true;
    }

    const char *ResolvedInclude(
        const FragmentTemplateComposer::ComposeInput &input,
        const ShaderModuleSlotRole role)
    {
        const RenderTemplateModuleRoot *root = input.resolved_template
            ? input.resolved_template->request.FindModuleRoot(role)
            : nullptr;
        return root && !root->include_path.IsEmpty()
            ? root->include_path.c_str() : nullptr;
    }

    // slot role → 逻辑名后缀。顺序完全由 RenderTemplateDefinition::slots
    // 决定，这里只提供显示标签，不参与排序。
    const char *GetSlotLogicalNameSuffix(
        const ShaderModuleSlotRole role) noexcept
    {
        switch (role)
        {
        case ShaderModuleSlotRole::SurfaceProvider:          return "Surface";
        case ShaderModuleSlotRole::DirectLightProvider:      return "Direct";
        case ShaderModuleSlotRole::ShadowProvider:           return "Shadow";
        case ShaderModuleSlotRole::AmbientLightProvider:     return "Indirect";
        case ShaderModuleSlotRole::AmbientOcclusionProvider: return "AO";
        case ShaderModuleSlotRole::LightingModel:            return "LightingModel";
        case ShaderModuleSlotRole::OutputPolicy:             return "ForwardLighting";
        case ShaderModuleSlotRole::MaterialSourceProvider:   return "MaterialSource";
        case ShaderModuleSlotRole::NTBProvider:              return "NTB";
        case ShaderModuleSlotRole::SkyProvider:              return "Sky";
        default:                                             return nullptr;
        }
    }

    // 按 RenderTemplateDefinition::slots 定义的顺序发射模块 #include。
    // include 顺序的唯一来源就是 slots——Composer 不得再另立一套顺序。
    // 可选 slot（required=false）未提供时跳过；必需 slot 缺失则返回失败。
    bool AppendSlotIncludes(
        const FragmentTemplateComposer::ComposeInput &input,
        const char *prefix,
        ShaderDocument &document)
    {
        const ResolvedRenderTemplate *resolved = input.resolved_template;
        if (!resolved || !resolved->definition)
            return false;

        const RenderTemplateDefinition &definition = *resolved->definition;
        for (hgl::uint32 index = 0; index < definition.slot_count; ++index)
        {
            const RenderTemplateSlot &slot = definition.slots[index];

            const char *path = ResolvedInclude(input, slot.role);
            if (!path)
            {
                if (slot.required)
                    return false;
                continue;
            }

            const char *suffix = GetSlotLogicalNameSuffix(slot.role);
            if (!suffix)
                return false;

            const AnsiString logical_name =
                AnsiString(prefix) + AnsiString(".") + AnsiString(suffix);
            AddTemplateBlock(document, ShaderDocumentBlockKind::Function,
                             IncludeTemplate(path), logical_name.c_str(), path);
        }
        return true;
    }

    // 纹理声明里的 channels → GLSL 宏 MTL_TEX_<NAME>_CHANNELS=<n>。
    // 用途：法线贴图声明 channels = 2 时，ntb 模块据此只读 XY 并用球面公式还原 Z
    // （BC5 法线；宏名"存在即非零"也让 #if defined() 判断可用）。
    void AppendTextureChannelDefines(
        std::string &defines,
        const FragmentTemplateComposer::ComposeInput &input)
    {
        if (!input.texture_declarations)
            return;

        for (const MaterialTextureDeclaration &declaration :
            *input.texture_declarations)
        {
            if (declaration.channels == 0)
                continue;

            std::string macro = "MTL_TEX_";
            for (const char ch : declaration.name)
                macro += (ch >= 'a' && ch <= 'z')
                    ? static_cast<char>(ch - 'a' + 'A') : ch;
            macro += "_CHANNELS";

            defines += "#define ";
            defines += macro;
            defines += " ";
            defines += std::to_string(declaration.channels);
            defines += "\n";
        }
    }

    bool ComposeForwardUnlit(
        const FragmentTemplateComposer::ComposeInput &input,
        ShaderDocument &document)
    {
        document.Clear();
        AddTemplateBlock(
            document, ShaderDocumentBlockKind::Version,
            AnsiString("#version 450\n"), "ForwardUnlit.Version");

        std::string defines;
        if (input.alpha_test)
        {
            defines += "#define HGL_ALPHA_TEST 1\n#define HGL_ALPHA_CUTOFF ";
            defines += std::to_string(input.alpha_cutoff);
            defines += "\n";
        }
        if (input.dither)
            defines += "#define HGL_ALPHA_DITHER 1\n";
        const char *material_source_module = ResolvedInclude(
            input, ShaderModuleSlotRole::MaterialSourceProvider);
        const char *ntb_module = ResolvedInclude(
            input, ShaderModuleSlotRole::NTBProvider);
        defines += "#define HGL_USE_MATERIAL_SOURCE_PROVIDER ";
        defines += material_source_module ? "1\n" : "0\n";
        defines += "#define HGL_USE_NTB_PROVIDER ";
        defines += ntb_module ? "1\n" : "0\n";
        defines += "#define HGL_USE_SCENE_LIGHTING 0\n";
        AppendTextureChannelDefines(defines, input);
        AddTemplateBlock(
            document, ShaderDocumentBlockKind::Define,
            AnsiString(defines.c_str()), "ForwardUnlit.Defines");

        AddTemplateBlock(
            document, ShaderDocumentBlockKind::Resource,
            IncludeTemplate("common/descriptor_macros.glsl"),
            "ForwardUnlit.DescriptorMacros",
            "common/descriptor_macros.glsl");
        AddTemplateBlock(
            document, ShaderDocumentBlockKind::Resource,
            IncludeTemplate("common/surface_interface.glsl"),
            "ForwardUnlit.SurfaceInterface",
            "common/surface_interface.glsl");
        AddTemplateBlock(
            document, ShaderDocumentBlockKind::Function,
            IncludeTemplate("lighting/forward_flat.glsl"),
            "ForwardUnlit.LightingModel",
            "lighting/forward_flat.glsl");
        const char *output_policy_module = ResolvedInclude(
            input, ShaderModuleSlotRole::OutputPolicy);
        const char *surface_module = ResolvedInclude(
            input, ShaderModuleSlotRole::SurfaceProvider);
        if (!material_source_module || !output_policy_module || !surface_module)
            return false;
        AddTemplateBlock(
            document, ShaderDocumentBlockKind::Function,
            IncludeTemplate(output_policy_module),
            "ForwardUnlit.OutputPolicy",
            output_policy_module);
        AppendDocumentBlocks(document, input.code_module_document);
        AddTemplateBlock(
            document, ShaderDocumentBlockKind::Function,
            IncludeTemplate(material_source_module),
            "ForwardUnlit.MaterialSource", material_source_module);
        AddTemplateBlock(
            document, ShaderDocumentBlockKind::Function,
            IncludeTemplate(surface_module),
            "ForwardUnlit.Surface",
            surface_module);
        AddTemplateBlock(
            document, ShaderDocumentBlockKind::Function,
            IncludeTemplate("common/alpha_compositor.glsl"),
            "ForwardUnlit.Alpha",
            "common/alpha_compositor.glsl");

        if (!AppendFragmentInputDeclarations(
                input.fragment_inputs, "ForwardUnlit.FragmentInputs", document))
            return false;

        if (!AppendOutputDeclarations(
                input.output_contract, "ForwardUnlit.Output", false, document))
            return false;

        std::string main_body = "\nvoid main()\n{\n";
        if (input.fragment_inputs)
        {
            AnsiString wiring;
            if (!BuildGLSLMaterialSurfaceInput(
                    *input.fragment_inputs, false, wiring))
                return false;
            main_body += wiring.c_str();
        }
        main_body +=
            "    const SurfaceOutput surface = EvalSurface(si, materialDataIndex);\n"
            "    const LightingInput lighting = BuildForwardLightingInput(surface, si);\n"
            "    const vec4 finalColor = EvalLighting(lighting);\n"
            "    WriteMaterialOutput(HGLComposeColor(finalColor));\n"
            "}\n";
        AddTemplateBlock(
            document, ShaderDocumentBlockKind::MainBody,
            AnsiString(main_body.c_str()), "ForwardUnlit.Main");
        return true;
    }

    bool ComposeSky(
        const FragmentTemplateComposer::ComposeInput &input,
        ShaderDocument &document)
    {
        document.Clear();
        AddTemplateBlock(document, ShaderDocumentBlockKind::Version,
            AnsiString("#version 450\n"), "Sky.Version");
        AddTemplateBlock(document, ShaderDocumentBlockKind::Resource,
            IncludeTemplate("common/descriptor_macros.glsl"),
            "Sky.DescriptorMacros", "common/descriptor_macros.glsl");
        AddTemplateBlock(document, ShaderDocumentBlockKind::Resource,
            IncludeTemplate("ubo/sky_info.glsl"),
            "Sky.SkyInfo", "ubo/sky_info.glsl");
        AddTemplateBlock(document, ShaderDocumentBlockKind::Resource,
            AnsiString("SCENE_SKY_UBO;\n"), "Sky.SkyUBO");

        const char *sky_module = ResolvedInclude(
            input, ShaderModuleSlotRole::SkyProvider);
        if (!sky_module)
            return false;
        AddTemplateBlock(document, ShaderDocumentBlockKind::Function,
            IncludeTemplate(sky_module), "Sky.Provider", sky_module);
        AddTemplateBlock(document, ShaderDocumentBlockKind::Resource,
            IncludeTemplate("common/surface_interface.glsl"),
            "Sky.SurfaceInterface", "common/surface_interface.glsl");
        const char *surface_module = ResolvedInclude(
            input, ShaderModuleSlotRole::SurfaceProvider);
        if (!surface_module)
            return false;

        // [SkyRoute 诊断] Sky 模板实际注入的 surface 模块
        GLogInfo("[SkyRoute] ComposeSky surface_module=%s", surface_module);

        AddTemplateBlock(document, ShaderDocumentBlockKind::Function,
            IncludeTemplate(surface_module), "Sky.Surface", surface_module);
        AddTemplateBlock(document, ShaderDocumentBlockKind::Function,
            IncludeTemplate("common/alpha_compositor.glsl"),
            "Sky.Alpha", "common/alpha_compositor.glsl");

        if (!AppendFragmentInputDeclarations(
                input.fragment_inputs, "Sky.FragmentInputs", document))
            return false;

        if (!AppendOutputDeclarations(
                input.output_contract, "Sky.Output", false, document))
            return false;

        std::string main_body =
            "\nvoid main()\n{\n"
            "    SurfaceInput si;\n"
            "    si.worldPos = fragDirection;\n"
            "    si.worldNormal = normalize(fragDirection);\n"
            "    si.uv0 = vec2(0.0);\n"
            "    si.uv1 = vec2(0.0);\n"
            "    si.vertexColor = vec4(1.0);\n"
            "    si.viewDir = fragDirection;\n"
            "    si.screenPos = vec2(0.0);\n"
            "    si.luminance = 0.0;\n"
            "    SurfaceOutput so = EvalSurface(si, 0u);\n"
            "    WriteMaterialOutput(HGLComposeColor(vec4(so.baseColor, so.alpha)));\n"
            "}\n";
        AddTemplateBlock(document, ShaderDocumentBlockKind::MainBody,
            AnsiString(main_body.c_str()), "Sky.Main");
        return true;
    }

    bool ComposeShadow(
        const FragmentTemplateComposer::ComposeInput &input,
        ShaderDocument &document)
    {
        document.Clear();
        AddTemplateBlock(document, ShaderDocumentBlockKind::Version,
            AnsiString("#version 450\n"), "ShadowCaster.Version");
        std::string defines = "#define HGL_COVERAGE_ONLY 1\n";
        if (input.alpha_test)
        {
            defines += "#define HGL_ALPHA_TEST 1\n#define HGL_ALPHA_CUTOFF ";
            defines += std::to_string(input.alpha_cutoff);
            defines += "\n";
        }
        if (input.dither)
            defines += "#define HGL_ALPHA_DITHER 1\n";
        AddTemplateBlock(document, ShaderDocumentBlockKind::Define,
            AnsiString(defines.c_str()), "ShadowCaster.Defines");
        AppendDocumentBlocks(document, input.code_module_document);

        // ShadowCaster 允许整数/布尔输出附件（depth-only 变体）
        if (!AppendOutputDeclarations(
                input.output_contract, "ShadowCaster.Output", true, document))
            return false;

        if (!input.coverage_contract
            || !input.coverage_contract->requires_alpha_evaluation)
        {
            AddTemplateBlock(document, ShaderDocumentBlockKind::MainBody,
                AnsiString("void main()\n{\n}\n"), "ShadowCaster.Main");
            return true;
        }

        AddTemplateBlock(document, ShaderDocumentBlockKind::Resource,
            IncludeTemplate("common/surface_interface.glsl"),
            "ShadowCaster.SurfaceInterface", "common/surface_interface.glsl");
        AddTemplateBlock(document, ShaderDocumentBlockKind::Function,
            IncludeTemplate("common/alpha_compositor.glsl"),
            "ShadowCaster.Alpha", "common/alpha_compositor.glsl");
        const char *material_source_module = ResolvedInclude(
            input, ShaderModuleSlotRole::MaterialSourceProvider);
        if (material_source_module)
            AddTemplateBlock(document, ShaderDocumentBlockKind::Function,
                IncludeTemplate(material_source_module),
                "ShadowCaster.MaterialSource",
                material_source_module);
        const char *surface_module = ResolvedInclude(
            input, ShaderModuleSlotRole::SurfaceProvider);
        if (!surface_module)
            return false;
        AddTemplateBlock(document, ShaderDocumentBlockKind::Function,
            IncludeTemplate(surface_module), "ShadowCaster.Surface",
            surface_module);

        if (!AppendFragmentInputDeclarations(
                input.fragment_inputs, "ShadowCaster.FragmentInputs", document))
            return false;

        std::string main_body =
            "\nvoid main()\n{\n"
            "    SurfaceInput si;\n"
            "    si.worldPos = vec3(0.0);\n"
            "    si.worldNormal = vec3(0.0, 0.0, 1.0);\n"
            "    si.uv0 = vec2(0.0);\n"
            "    si.uv1 = vec2(0.0);\n"
            "    si.vertexColor = vec4(1.0);\n"
            "    si.viewDir = vec3(0.0, 0.0, 1.0);\n"
            "    si.screenPos = gl_FragCoord.xy;\n"
            "    si.luminance = 1.0;\n"
            "    si.styleID = 0u;\n";
        if (input.fragment_inputs)
        {
            AnsiString wiring;
            if (!BuildGLSLMaterialSurfaceInput(
                    *input.fragment_inputs, false, wiring))
                return false;
            main_body += wiring.c_str();
        }
        main_body +=
            "    const float alpha = EvalAlpha(si, 0u);\n"
            "    HGLApplyAlpha(alpha);\n"
            "}\n";
        AddTemplateBlock(document, ShaderDocumentBlockKind::MainBody,
            AnsiString(main_body.c_str()), "ShadowCaster.Main");
        return true;
    }

    bool ComposeForwardLit(
        const FragmentTemplateComposer::ComposeInput &input,
        ShaderDocument &document)
    {
        document.Clear();
        AddTemplateBlock(document, ShaderDocumentBlockKind::Version,
            AnsiString("#version 450\n"), "ForwardLit.Version");

        std::string defines =
            "#define HGL_USE_MATERIAL_SOURCE_PROVIDER 1\n"
            "#define HGL_USE_NTB_PROVIDER 1\n"
            "#define HGL_USE_SCENE_LIGHTING 1\n";
        AppendTextureChannelDefines(defines, input);
        if (input.alpha_test)
        {
            defines += "#define HGL_ALPHA_TEST 1\n#define HGL_ALPHA_CUTOFF ";
            defines += std::to_string(input.alpha_cutoff);
            defines += "\n";
        }
        if (input.dither)
            defines += "#define HGL_ALPHA_DITHER 1\n";
        AddTemplateBlock(document, ShaderDocumentBlockKind::Define,
            AnsiString(defines.c_str()), "ForwardLit.Defines");

        const char *resource_paths[] =
        {
            "common/descriptor_macros.glsl",
            "common/surface_interface.glsl",
            "ubo/camera_info.glsl",
            "ubo/sky_info.glsl"
        };
        const char *resource_names[] =
        {
            "ForwardLit.DescriptorMacros",
            "ForwardLit.SurfaceInterface",
            "ForwardLit.CameraInfo",
            "ForwardLit.SkyInfo"
        };
        for (int index = 0; index < 4; ++index)
            AddTemplateBlock(document, ShaderDocumentBlockKind::Resource,
                IncludeTemplate(resource_paths[index]), resource_names[index],
                resource_paths[index]);
        AddTemplateBlock(document, ShaderDocumentBlockKind::Resource,
            AnsiString("SCENE_CAMERA_UBO;\nSCENE_SKY_UBO;\n"),
            "ForwardLit.SceneUBO");

        // 全部模块（含天光与 Surface）一律按 template.slots 顺序发射——
        // Surface 排在 MaterialSource / NTB 之后，否则
        // material_surface.glsl 里的 EvalMaterialSource / GetNTB 未声明。
        if (!AppendSlotIncludes(input, "ForwardLit", document))
            return false;

        AddTemplateBlock(document, ShaderDocumentBlockKind::Function,
            IncludeTemplate("common/alpha_compositor.glsl"),
            "ForwardLit.Alpha", "common/alpha_compositor.glsl");

        if (!AppendFragmentInputDeclarations(
                input.fragment_inputs, "ForwardLit.FragmentInputs", document))
            return false;

        if (!AppendOutputDeclarations(
                input.output_contract, "ForwardLit.Output", false, document))
            return false;

        AppendDocumentBlocks(document, input.code_module_document);

        std::string main_body = "\nvoid main()\n{\n";
        if (input.fragment_inputs)
        {
            AnsiString wiring;
            if (!BuildGLSLMaterialSurfaceInput(
                    *input.fragment_inputs, true, wiring))
                return false;
            main_body += wiring.c_str();
        }
        main_body +=
            "    const SurfaceOutput surface = EvalSurface(si, materialDataIndex);\n"
            "    const LightingInput lighting = BuildForwardLightingInput(surface, si);\n"
            "    const vec4 finalColor = EvalLighting(lighting);\n"
            "    WriteMaterialOutput(HGLComposeColor(finalColor));\n"
            "}\n";
        AddTemplateBlock(document, ShaderDocumentBlockKind::MainBody,
            AnsiString(main_body.c_str()), "ForwardLit.Main");
        return true;
    }

}

namespace hgl::graph::mtl
{
    bool FragmentTemplateComposer::Compose(
        const ComposeInput &input,
        ShaderDocument &out_document,
        ShaderDocumentDiagnostics &out_diagnostics) const
    {
        if (!input.resolved_template)
        {
           ShaderDocumentDiagnostic *diagnostic = out_diagnostics.Create();
           diagnostic->code = "template-not-resolved";
           diagnostic->message =
               "FragmentTemplateComposer requires a valid ResolvedRenderTemplate.";
           diagnostic->block_index = -1;
           diagnostic->source.stage = "fragment";
           diagnostic->source.logical_name = "FragmentTemplateComposer";
           return false;
        }

        if (!input.resolved_template->IsValid())
        {
           ShaderDocumentDiagnostic *diagnostic = out_diagnostics.Create();
           diagnostic->code = "template-invalid";
           diagnostic->message =
               "FragmentTemplateComposer received an invalid resolved template.";
           diagnostic->block_index = -1;
           diagnostic->source.stage = "fragment";
           diagnostic->source.logical_name = "FragmentTemplateComposer";
           return false;
        }

        const RenderTemplateID template_id =
           input.resolved_template->request.template_id;
        if (template_id == RenderTemplateID::ForwardUnlit)
          return ComposeForwardUnlit(input, out_document);
        if (template_id == RenderTemplateID::Sky)
          return ComposeSky(input, out_document);
        if (template_id == RenderTemplateID::ShadowCasterOpaque
         || template_id == RenderTemplateID::ShadowCasterMasked)
          return ComposeShadow(input, out_document);
        if (template_id == RenderTemplateID::ForwardLitShadowedAO
         || template_id
                == RenderTemplateID::ForwardLitShadowedIdentityAO
         || template_id == RenderTemplateID::ForwardLitUnshadowedAO)
          return ComposeForwardLit(input, out_document);
        ShaderDocumentDiagnostic *diagnostic = out_diagnostics.Create();
        diagnostic->code = "template-unregistered";
        diagnostic->message =
           "No registered native fragment template matches the requested render template.";
        diagnostic->block_index = -1;
        diagnostic->source.stage = "fragment";
        diagnostic->source.logical_name = "FragmentTemplateComposer";
        return false;
    }
}
