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

    AnsiString IncludeTemplate(const char *path)
    {
        return AnsiString("#include \"") + AnsiString(path) + AnsiString("\"\n");
    }

    const char *ResolvedInclude(
        const FragmentTemplateComposer::ComposeInput &input,
        const ShaderModuleSlotRole role,
        const char *fallback)
    {
        if (input.resolved_template)
        {
            const RenderTemplateRequest &request =
                input.resolved_template->request;
            const RenderTemplateModuleRoot *root = request.FindModuleRoot(role);
            if (root && !root->include_path.IsEmpty())
                return root->include_path.c_str();
        }
        return fallback;
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
        defines += "#define HGL_USE_MATERIAL_SOURCE_PROVIDER ";
        defines += input.enable_material_source_provider ? "1\n" : "0\n";
        defines += "#define HGL_USE_NTB_PROVIDER ";
        defines += input.enable_ntb_provider ? "1\n" : "0\n";
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
            input, ShaderModuleSlotRole::OutputPolicy,
            "compositor/flat_lighting.glsl");
        AddTemplateBlock(
            document, ShaderDocumentBlockKind::Function,
            IncludeTemplate(output_policy_module),
            "ForwardUnlit.OutputPolicy",
            output_policy_module);
        if (input.code_module_glsl)
            AddTemplateBlock(
                document, ShaderDocumentBlockKind::Module,
                AnsiString(input.code_module_glsl->c_str()),
                "ForwardUnlit.CodeModule");
        if (input.enable_material_source_provider)
            AddTemplateBlock(
                document, ShaderDocumentBlockKind::Function,
                IncludeTemplate(input.material_source_module
                    && input.material_source_module[0]
                    ? input.material_source_module
                    : "material/unlit_source.glsl"),
                "ForwardUnlit.MaterialSource");
        AddTemplateBlock(
            document, ShaderDocumentBlockKind::Function,
            IncludeTemplate(input.surface_module && input.surface_module[0]
                ? input.surface_module
                : "surface/material_surface.glsl"),
            "ForwardUnlit.Surface",
            input.surface_module && input.surface_module[0]
                ? input.surface_module : "surface/material_surface.glsl");
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

        const char *sky_module =
            input.sky_module && input.sky_module[0]
                ? input.sky_module
                : "sky/sky_atmosphere.glsl";
        sky_module = ResolvedInclude(
            input, ShaderModuleSlotRole::AmbientLightProvider, sky_module);
        AddTemplateBlock(document, ShaderDocumentBlockKind::Function,
            IncludeTemplate(sky_module), "Sky.Provider", sky_module);
        AddTemplateBlock(document, ShaderDocumentBlockKind::Resource,
            IncludeTemplate("common/surface_interface.glsl"),
            "Sky.SurfaceInterface", "common/surface_interface.glsl");
        const char *surface_module =
            input.surface_module && input.surface_module[0]
                ? input.surface_module
                : "surface/sky_minimal_surface.glsl";
        surface_module = ResolvedInclude(
            input, ShaderModuleSlotRole::SurfaceProvider, surface_module);
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
        if (input.code_module_glsl)
            AddTemplateBlock(document, ShaderDocumentBlockKind::Module,
                AnsiString(input.code_module_glsl->c_str()),
                "ShadowCaster.CodeModule");

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
        if (input.material_source_module
            && input.material_source_module[0])
            AddTemplateBlock(document, ShaderDocumentBlockKind::Function,
                IncludeTemplate(input.material_source_module),
                "ShadowCaster.MaterialSource",
                input.material_source_module);
        const char *surface_module =
            input.surface_module && input.surface_module[0]
                ? input.surface_module
                : "surface/material_surface.glsl";
        surface_module = ResolvedInclude(
            input, ShaderModuleSlotRole::SurfaceProvider, surface_module);
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

        const char *sky_module =
            input.sky_module && input.sky_module[0]
                ? input.sky_module : "sky/sky_atmosphere.glsl";
        const char *direct_module =
            input.direct_lighting_module
                && input.direct_lighting_module[0]
                ? input.direct_lighting_module
                : "lighting/direct_cook_torrance_pbr.glsl";
        direct_module = ResolvedInclude(
            input, ShaderModuleSlotRole::DirectLightProvider, direct_module);
        const char *indirect_module =
            input.indirect_lighting_module
                && input.indirect_lighting_module[0]
                ? input.indirect_lighting_module
                : "lighting/indirect_sky_ambient.glsl";
        indirect_module = ResolvedInclude(
            input, ShaderModuleSlotRole::AmbientLightProvider, indirect_module);
        const char *shadow_module = ResolvedInclude(
            input, ShaderModuleSlotRole::ShadowProvider,
            "shadow/identity.glsl");
        const char *ao_module = ResolvedInclude(
            input, ShaderModuleSlotRole::AmbientOcclusionProvider,
            "ao/identity.glsl");
        const char *algorithm_module =
            input.lighting_algorithm_module
                && input.lighting_algorithm_module[0]
                ? input.lighting_algorithm_module
                : "lighting/forward_pbr.glsl";
        algorithm_module = ResolvedInclude(
            input, ShaderModuleSlotRole::LightingModel, algorithm_module);
        const char *material_module =
            input.material_source_module
                && input.material_source_module[0]
                ? input.material_source_module
                : "material/pbr_surface_source.glsl";
        const char *ntb_module =
            input.ntb_module && input.ntb_module[0]
                ? input.ntb_module
                : "ntb/ntb_tangent_vbo_normalmap.glsl";
        const char *forward_module =
            input.forward_lighting_module
                && input.forward_lighting_module[0]
                ? input.forward_lighting_module
                : "compositor/forward_lighting.glsl";
        forward_module = ResolvedInclude(
            input, ShaderModuleSlotRole::OutputPolicy, forward_module);
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

        const char *surface_module =
            input.surface_module && input.surface_module[0]
                ? input.surface_module : "surface/material_surface.glsl";
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
        if (input.code_module_glsl)
            AddTemplateBlock(document, ShaderDocumentBlockKind::Module,
                AnsiString(input.code_module_glsl->c_str()),
                "ForwardLit.CodeModule");

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

    bool ComposeUnimplemented(
        const RenderTemplateID template_id,
        ShaderDocument &document)
    {
        document.Clear();
        const char *name = "Unimplemented";
        switch (template_id)
        {
        case RenderTemplateID::Decal: name = "Decal"; break;
        case RenderTemplateID::PostProcessSSAO: name = "PostProcessSSAO"; break;
        case RenderTemplateID::PostProcessDOF: name = "PostProcessDOF"; break;
        default: return false;
        }

        AddTemplateBlock(document, ShaderDocumentBlockKind::Version,
            AnsiString("#version 450\n"), name);
        AnsiString main_body = AnsiString("\n// ") + AnsiString(name)
            + AnsiString(" is registered but not implemented yet.\n"
                         "void main()\n"
                         "{\n"
                         "}\n");
        AddTemplateBlock(document, ShaderDocumentBlockKind::MainBody,
            main_body, name);
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
        ComposeInput resolved_input = input;
        if (!input.request && !input.resolved_template)
        {
           ShaderDocumentDiagnostic *diagnostic = out_diagnostics.Create();
           diagnostic->code = "template-no-request";
           diagnostic->message =
               "FragmentTemplateComposer requires a validated RenderTemplateRequest or ResolvedRenderTemplate; legacy composition fallback is disabled.";
           diagnostic->block_index = -1;
           diagnostic->source.stage = "fragment";
           diagnostic->source.logical_name = "FragmentTemplateComposer";
           return false;
        }

        if (input.resolved_template)
        {
           if (!input.resolved_template->IsValid())
               return false;
           if (input.request
            && input.request->GetHash()
                   != input.resolved_template->request.GetHash())
               return false;
           resolved_input.request = &input.resolved_template->request;
        }

        if (resolved_input.request)
        {
           RenderTemplateValidationDiagnostic diagnostic{};
           if (!ValidateRenderTemplateRequest(
                   *resolved_input.request,
                   GetShaderCodeModuleRegistry(),
                   diagnostic))
               return false;
           if (resolved_input.variant
            && (resolved_input.request->template_id
                   != resolved_input.variant->fragment_template
                 && resolved_input.request->template_id
                       != RenderTemplateID::ShadowCasterOpaque
                 && resolved_input.request->template_id
                       != RenderTemplateID::ShadowCasterMasked
              || resolved_input.request->template_version
                   != resolved_input.variant->template_version))
               return false;
        }

        if (resolved_input.request
         && resolved_input.request->template_id == RenderTemplateID::ForwardUnlit)
           return ComposeForwardUnlit(resolved_input, out_document);
        if (resolved_input.request
         && resolved_input.request->template_id == RenderTemplateID::Sky)
           return ComposeSky(resolved_input, out_document);
        if (resolved_input.request
         && (resolved_input.request->template_id == RenderTemplateID::ShadowCasterOpaque
          || resolved_input.request->template_id == RenderTemplateID::ShadowCasterMasked))
           return ComposeShadow(resolved_input, out_document);
        if (resolved_input.request
         && (resolved_input.request->template_id == RenderTemplateID::ForwardLitShadowedAO
          || resolved_input.request->template_id
                == RenderTemplateID::ForwardLitShadowedIdentityAO
          || resolved_input.request->template_id == RenderTemplateID::ForwardLitUnshadowedAO))
           return ComposeForwardLit(resolved_input, out_document);
        if (resolved_input.request
         && (resolved_input.request->template_id == RenderTemplateID::Decal
          || resolved_input.request->template_id == RenderTemplateID::PostProcessSSAO
          || resolved_input.request->template_id == RenderTemplateID::PostProcessDOF))
           return ComposeUnimplemented(
               resolved_input.request->template_id, out_document);

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
