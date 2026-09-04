#include <hgl/mtl/FragmentTemplateComposer.h>
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

        if (input.fragment_inputs)
        {
            std::string declarations;
            for (int index = 0;
                 index < input.fragment_inputs->GetCount();
                 ++index)
            {
                AnsiString declaration;
                if (!BuildGLSLInterStageDeclaration(
                        (*input.fragment_inputs)[index],
                        "in", declaration))
                    return false;
                declarations += declaration.c_str();
                declarations += "\n";
            }
            AddTemplateBlock(
                document, ShaderDocumentBlockKind::Interface,
                AnsiString(declarations.c_str()), "ForwardUnlit.FragmentInputs");
        }

        if (input.output_contract)
        {
            const OutputContract &output = *input.output_contract;
            for (int index = 0; index < output.attachments.GetCount(); ++index)
            {
                const ShaderOutputAttachmentContract &attachment =
                    output.attachments[index];
                const char *type_name = nullptr;
                switch (attachment.value_type)
                {
                case ShaderStageValueType::Float: type_name = "float"; break;
                case ShaderStageValueType::Vec2: type_name = "vec2"; break;
                case ShaderStageValueType::Vec3: type_name = "vec3"; break;
                case ShaderStageValueType::Vec4: type_name = "vec4"; break;
                default: return false;
                }
                const char *output_name =
                    GetMaterialOutputName(attachment.write_semantic_id);
                if (!output_name)
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
                AddTemplateBlock(
                    document, ShaderDocumentBlockKind::Interface,
                    AnsiString(declaration.c_str()), "ForwardUnlit.Output");
            }
        }

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
            input, ShaderModuleSlotRole::AmbientLightProvider);
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
        AddTemplateBlock(document, ShaderDocumentBlockKind::Function,
            IncludeTemplate(surface_module), "Sky.Surface", surface_module);
        AddTemplateBlock(document, ShaderDocumentBlockKind::Function,
            IncludeTemplate("common/alpha_compositor.glsl"),
            "Sky.Alpha", "common/alpha_compositor.glsl");

        if (input.fragment_inputs)
        {
            std::string declarations;
            for (int index = 0;
                 index < input.fragment_inputs->GetCount();
                 ++index)
            {
                AnsiString declaration;
                if (!BuildGLSLInterStageDeclaration(
                        (*input.fragment_inputs)[index],
                        "in", declaration))
                    return false;
                declarations += declaration.c_str();
                declarations += "\n";
            }
            AddTemplateBlock(document, ShaderDocumentBlockKind::Interface,
                AnsiString(declarations.c_str()), "Sky.FragmentInputs");
        }

        if (input.output_contract)
        {
            const OutputContract &output = *input.output_contract;
            for (int index = 0; index < output.attachments.GetCount(); ++index)
            {
                const ShaderOutputAttachmentContract &attachment =
                    output.attachments[index];
                const char *type_name = nullptr;
                switch (attachment.value_type)
                {
                case ShaderStageValueType::Float: type_name = "float"; break;
                case ShaderStageValueType::Vec2: type_name = "vec2"; break;
                case ShaderStageValueType::Vec3: type_name = "vec3"; break;
                case ShaderStageValueType::Vec4: type_name = "vec4"; break;
                default: return false;
                }
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
                    AnsiString(declaration.c_str()), "Sky.Output");
            }
        }

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

        if (input.output_contract)
        {
            const OutputContract &output = *input.output_contract;
            for (int index = 0; index < output.attachments.GetCount(); ++index)
            {
                const ShaderOutputAttachmentContract &attachment =
                    output.attachments[index];
                const char *type_name = nullptr;
                switch (attachment.value_type)
                {
                case ShaderStageValueType::Float: type_name = "float"; break;
                case ShaderStageValueType::Vec2: type_name = "vec2"; break;
                case ShaderStageValueType::Vec3: type_name = "vec3"; break;
                case ShaderStageValueType::Vec4: type_name = "vec4"; break;
                case ShaderStageValueType::Int: type_name = "int"; break;
                case ShaderStageValueType::UInt: type_name = "uint"; break;
                case ShaderStageValueType::Bool: type_name = "bool"; break;
                default: return false;
                }
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
                    AnsiString(declaration.c_str()), "ShadowCaster.Output");
            }
        }

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

        std::string declarations;
        if (input.fragment_inputs)
        {
            for (int index = 0;
                 index < input.fragment_inputs->GetCount();
                 ++index)
            {
                AnsiString declaration;
                if (!BuildGLSLInterStageDeclaration(
                        (*input.fragment_inputs)[index],
                        "in", declaration))
                    return false;
                declarations += declaration.c_str();
                declarations += "\n";
            }
        }
        AddTemplateBlock(document, ShaderDocumentBlockKind::Interface,
            AnsiString(declarations.c_str()), "ShadowCaster.FragmentInputs");

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

        const char *sky_module = "sky/sky_atmosphere.glsl";
        const char *direct_module = ResolvedInclude(
            input, ShaderModuleSlotRole::DirectLightProvider);
        const char *indirect_module = ResolvedInclude(
            input, ShaderModuleSlotRole::AmbientLightProvider);
        const char *shadow_module = ResolvedInclude(
            input, ShaderModuleSlotRole::ShadowProvider);
        const char *ao_module = ResolvedInclude(
            input, ShaderModuleSlotRole::AmbientOcclusionProvider);
        const char *algorithm_module = ResolvedInclude(
            input, ShaderModuleSlotRole::LightingModel);
        const char *material_module = ResolvedInclude(
            input, ShaderModuleSlotRole::MaterialSourceProvider);
        const char *ntb_module = ResolvedInclude(
            input, ShaderModuleSlotRole::NTBProvider);
        const char *forward_module = ResolvedInclude(
            input, ShaderModuleSlotRole::OutputPolicy);
        if (!direct_module || !indirect_module || !shadow_module || !ao_module
         || !algorithm_module || !material_module || !ntb_module
         || !forward_module)
            return false;
        const char *paths[] =
        {
            sky_module, direct_module, indirect_module, shadow_module,
            ao_module, algorithm_module,
            material_module, ntb_module, forward_module
        };
        const char *names[] =
        {
            "ForwardLit.Sky", "ForwardLit.Direct", "ForwardLit.Indirect",
            "ForwardLit.Shadow", "ForwardLit.AO",
            "ForwardLit.LightingModel", "ForwardLit.MaterialSource",
            "ForwardLit.NTB", "ForwardLit.ForwardLighting"
        };
        for (int index = 0; index < 9; ++index)
            AddTemplateBlock(document, ShaderDocumentBlockKind::Function,
                IncludeTemplate(paths[index]), names[index], paths[index]);

        const char *surface_module = ResolvedInclude(
            input, ShaderModuleSlotRole::SurfaceProvider);
        if (!surface_module)
            return false;
        AddTemplateBlock(document, ShaderDocumentBlockKind::Function,
            IncludeTemplate(surface_module), "ForwardLit.Surface",
            surface_module);
        AddTemplateBlock(document, ShaderDocumentBlockKind::Function,
            IncludeTemplate("common/alpha_compositor.glsl"),
            "ForwardLit.Alpha", "common/alpha_compositor.glsl");

        if (input.fragment_inputs)
        {
            std::string declarations;
            for (int index = 0;
                 index < input.fragment_inputs->GetCount();
                 ++index)
            {
                AnsiString declaration;
                if (!BuildGLSLInterStageDeclaration(
                        (*input.fragment_inputs)[index],
                        "in", declaration))
                    return false;
                declarations += declaration.c_str();
                declarations += "\n";
            }
            AddTemplateBlock(document, ShaderDocumentBlockKind::Interface,
                AnsiString(declarations.c_str()), "ForwardLit.FragmentInputs");
        }
        if (input.output_contract)
        {
            const OutputContract &output = *input.output_contract;
            for (int index = 0; index < output.attachments.GetCount(); ++index)
            {
                const ShaderOutputAttachmentContract &attachment =
                    output.attachments[index];
                const char *type_name = nullptr;
                switch (attachment.value_type)
                {
                case ShaderStageValueType::Float: type_name = "float"; break;
                case ShaderStageValueType::Vec2: type_name = "vec2"; break;
                case ShaderStageValueType::Vec3: type_name = "vec3"; break;
                case ShaderStageValueType::Vec4: type_name = "vec4"; break;
                default: return false;
                }
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
                    AnsiString(declaration.c_str()), "ForwardLit.Output");
            }
        }
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
           "No registered native fragment template matches the requested render template; legacy assembler fallback is intentionally disabled.";
        diagnostic->block_index = -1;
        diagnostic->source.stage = "fragment";
        diagnostic->source.logical_name = "FragmentTemplateComposer";
        return false;
    }
}
