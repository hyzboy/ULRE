#include <hgl/mtl/FragmentTemplateComposer.h>
#include <hgl/mtl/ShaderCodeModuleRegistry.h>
#include <hgl/mtl/CompositorAssembler.h>
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

    bool ComposeForwardUnlit(
        const FragmentTemplateComposer::ComposeInput &input,
        ShaderDocument &document)
    {
        document.Clear();
        AddTemplateBlock(
            document, ShaderDocumentBlockKind::Version,
            AnsiString("#version 450\n"), "ForwardUnlit.Version");

        std::string defines;
        if (input.module_options.alpha_test)
        {
            defines += "#define HGL_ALPHA_TEST 1\n#define HGL_ALPHA_CUTOFF ";
            defines += std::to_string(input.module_options.alpha_cutoff);
            defines += "\n";
        }
        if (input.module_options.dither)
            defines += "#define HGL_ALPHA_DITHER 1\n";
        defines += "#define HGL_USE_MATERIAL_SOURCE_PROVIDER ";
        defines += input.module_options.enable_material_source_provider ? "1\n" : "0\n";
        defines += "#define HGL_USE_NTB_PROVIDER ";
        defines += input.module_options.enable_ntb_provider ? "1\n" : "0\n";
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
        if (input.module_options.enable_material_source_provider)
            AddTemplateBlock(
                document, ShaderDocumentBlockKind::Function,
                IncludeTemplate(input.module_options.material_source_module
                    && input.module_options.material_source_module[0]
                    ? input.module_options.material_source_module
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

        if (input.module_options.fragment_inputs)
        {
            std::string declarations;
            for (int index = 0;
                 index < input.module_options.fragment_inputs->GetCount();
                 ++index)
            {
                AnsiString declaration;
                if (!BuildGLSLInterStageDeclaration(
                        (*input.module_options.fragment_inputs)[index],
                        "in", declaration))
                    return false;
                declarations += declaration.c_str();
                declarations += "\n";
            }
            AddTemplateBlock(
                document, ShaderDocumentBlockKind::Interface,
                AnsiString(declarations.c_str()), "ForwardUnlit.FragmentInputs");
        }

        if (input.module_options.output_contract)
        {
            const OutputContract &output = *input.module_options.output_contract;
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

        if (input.code_module_glsl)
            AddTemplateBlock(
                document, ShaderDocumentBlockKind::Module,
                AnsiString(input.code_module_glsl->c_str()),
                "ForwardUnlit.CodeModule");

        std::string main_body = "\nvoid main()\n{\n";
        if (input.module_options.fragment_inputs)
        {
            AnsiString wiring;
            if (!BuildGLSLMaterialSurfaceInput(
                    *input.module_options.fragment_inputs, false, wiring))
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
            input.module_options.sky_module && input.module_options.sky_module[0]
                ? input.module_options.sky_module
                : "sky/sky_atmosphere.glsl";
        AddTemplateBlock(document, ShaderDocumentBlockKind::Function,
            IncludeTemplate(sky_module), "Sky.Provider", sky_module);
        AddTemplateBlock(document, ShaderDocumentBlockKind::Resource,
            IncludeTemplate("common/surface_interface.glsl"),
            "Sky.SurfaceInterface", "common/surface_interface.glsl");
        const char *surface_module =
            input.surface_module && input.surface_module[0]
                ? input.surface_module
                : "surface/sky_minimal_surface.glsl";
        AddTemplateBlock(document, ShaderDocumentBlockKind::Function,
            IncludeTemplate(surface_module), "Sky.Surface", surface_module);
        AddTemplateBlock(document, ShaderDocumentBlockKind::Function,
            IncludeTemplate("common/alpha_compositor.glsl"),
            "Sky.Alpha", "common/alpha_compositor.glsl");

        if (input.module_options.fragment_inputs)
        {
            std::string declarations;
            for (int index = 0;
                 index < input.module_options.fragment_inputs->GetCount();
                 ++index)
            {
                AnsiString declaration;
                if (!BuildGLSLInterStageDeclaration(
                        (*input.module_options.fragment_inputs)[index],
                        "in", declaration))
                    return false;
                declarations += declaration.c_str();
                declarations += "\n";
            }
            AddTemplateBlock(document, ShaderDocumentBlockKind::Interface,
                AnsiString(declarations.c_str()), "Sky.FragmentInputs");
        }

        if (input.module_options.output_contract)
        {
            const OutputContract &output = *input.module_options.output_contract;
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
        AddTemplateBlock(document, ShaderDocumentBlockKind::Define,
            AnsiString("#define HGL_COVERAGE_ONLY 1\n"), "ShadowCaster.Defines");
        if (input.code_module_glsl)
            AddTemplateBlock(document, ShaderDocumentBlockKind::Module,
                AnsiString(input.code_module_glsl->c_str()),
                "ShadowCaster.CodeModule");

        if (input.module_options.output_contract)
        {
            const OutputContract &output = *input.module_options.output_contract;
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

        if (!input.module_options.coverage_contract
            || !input.module_options.coverage_contract->requires_alpha_evaluation)
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
        if (input.module_options.material_source_module
            && input.module_options.material_source_module[0])
            AddTemplateBlock(document, ShaderDocumentBlockKind::Function,
                IncludeTemplate(input.module_options.material_source_module),
                "ShadowCaster.MaterialSource",
                input.module_options.material_source_module);
        const char *surface_module =
            input.surface_module && input.surface_module[0]
                ? input.surface_module
                : "surface/material_surface.glsl";
        AddTemplateBlock(document, ShaderDocumentBlockKind::Function,
            IncludeTemplate(surface_module), "ShadowCaster.Surface",
            surface_module);

        std::string declarations;
        if (input.module_options.fragment_inputs)
        {
            for (int index = 0;
                 index < input.module_options.fragment_inputs->GetCount();
                 ++index)
            {
                AnsiString declaration;
                if (!BuildGLSLInterStageDeclaration(
                        (*input.module_options.fragment_inputs)[index],
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
        if (input.module_options.fragment_inputs)
        {
            AnsiString wiring;
            if (!BuildGLSLMaterialSurfaceInput(
                    *input.module_options.fragment_inputs, false, wiring))
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
        if (input.module_options.alpha_test)
        {
            defines += "#define HGL_ALPHA_TEST 1\n#define HGL_ALPHA_CUTOFF ";
            defines += std::to_string(input.module_options.alpha_cutoff);
            defines += "\n";
        }
        if (input.module_options.dither)
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
            input.module_options.sky_module && input.module_options.sky_module[0]
                ? input.module_options.sky_module : "sky/sky_atmosphere.glsl";
        const char *direct_module =
            input.module_options.direct_lighting_module
                && input.module_options.direct_lighting_module[0]
                ? input.module_options.direct_lighting_module
                : "lighting/direct_cook_torrance_pbr.glsl";
        const char *indirect_module =
            input.module_options.indirect_lighting_module
                && input.module_options.indirect_lighting_module[0]
                ? input.module_options.indirect_lighting_module
                : "lighting/indirect_sky_ambient.glsl";
        const char *algorithm_module =
            input.module_options.lighting_algorithm_module
                && input.module_options.lighting_algorithm_module[0]
                ? input.module_options.lighting_algorithm_module
                : "lighting/forward_pbr.glsl";
        const char *material_module =
            input.module_options.material_source_module
                && input.module_options.material_source_module[0]
                ? input.module_options.material_source_module
                : "material/pbr_surface_source.glsl";
        const char *ntb_module =
            input.module_options.ntb_module && input.module_options.ntb_module[0]
                ? input.module_options.ntb_module
                : "ntb/ntb_tangent_vbo_normalmap.glsl";
        const char *forward_module =
            input.module_options.forward_lighting_module
                && input.module_options.forward_lighting_module[0]
                ? input.module_options.forward_lighting_module
                : "compositor/forward_lighting.glsl";
        const char *paths[] =
        {
            sky_module, direct_module, indirect_module, algorithm_module,
            material_module, ntb_module, forward_module
        };
        const char *names[] =
        {
            "ForwardLit.Sky", "ForwardLit.Direct", "ForwardLit.Indirect",
            "ForwardLit.LightingModel", "ForwardLit.MaterialSource",
            "ForwardLit.NTB", "ForwardLit.ForwardLighting"
        };
        for (int index = 0; index < 7; ++index)
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

        if (input.module_options.fragment_inputs)
        {
            std::string declarations;
            for (int index = 0;
                 index < input.module_options.fragment_inputs->GetCount();
                 ++index)
            {
                AnsiString declaration;
                if (!BuildGLSLInterStageDeclaration(
                        (*input.module_options.fragment_inputs)[index],
                        "in", declaration))
                    return false;
                declarations += declaration.c_str();
                declarations += "\n";
            }
            AddTemplateBlock(document, ShaderDocumentBlockKind::Interface,
                AnsiString(declarations.c_str()), "ForwardLit.FragmentInputs");
        }
        if (input.module_options.output_contract)
        {
            const OutputContract &output = *input.module_options.output_contract;
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
        if (input.module_options.fragment_inputs)
        {
            AnsiString wiring;
            if (!BuildGLSLMaterialSurfaceInput(
                    *input.module_options.fragment_inputs, true, wiring))
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

        const std::string empty_code_module_glsl;
        const std::string &code_module_glsl = input.code_module_glsl
            ? *input.code_module_glsl
            : empty_code_module_glsl;
        CompositorAssembler::CompositorModuleOptions legacy_options{};
        legacy_options.sky_module = input.module_options.sky_module;
        legacy_options.direct_lighting_module = input.module_options.direct_lighting_module;
        legacy_options.indirect_lighting_module = input.module_options.indirect_lighting_module;
        legacy_options.lighting_algorithm_module = input.module_options.lighting_algorithm_module;
        legacy_options.material_source_module = input.module_options.material_source_module;
        legacy_options.ntb_module = input.module_options.ntb_module;
        legacy_options.forward_lighting_module = input.module_options.forward_lighting_module;
        legacy_options.enable_material_source_provider = input.module_options.enable_material_source_provider;
        legacy_options.enable_ntb_provider = input.module_options.enable_ntb_provider;
        legacy_options.enable_scene_lighting = input.module_options.enable_scene_lighting;
        legacy_options.alpha_test = input.module_options.alpha_test;
        legacy_options.alpha_cutoff = input.module_options.alpha_cutoff;
        legacy_options.dither = input.module_options.dither;
        legacy_options.fragment_inputs = input.module_options.fragment_inputs;
        legacy_options.output_contract = input.module_options.output_contract;
        legacy_options.coverage_contract = input.module_options.coverage_contract;
        CompositorAssembler legacy_assembler;
        return legacy_assembler.AssembleDocument(
            input.surface, input.pass, input.fragment_source,
            input.surface_module, legacy_options, code_module_glsl,
            out_document, out_diagnostics);
    }
}
