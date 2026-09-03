#include <hgl/mtl/FragmentTemplateComposer.h>
#include <hgl/mtl/CompositorAssembler.h>
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
}

namespace hgl::graph::mtl
{
    bool FragmentTemplateComposer::Compose(
        const ComposeInput &input,
        ShaderDocument &out_document,
        ShaderDocumentDiagnostics &out_diagnostics) const
    {
        if (input.request)
        {
            RenderTemplateValidationDiagnostic diagnostic{};
            if (!ValidateRenderTemplateRequest(
                    *input.request,
                    GetShaderCodeModuleRegistry(),
                    diagnostic))
                return false;
            if (input.variant
             && (input.request->template_id != input.variant->fragment_template
              || input.request->template_version != input.variant->template_version))
                return false;
        }

        if (input.request
         && input.request->template_id == RenderTemplateID::ForwardUnlit)
            return ComposeForwardUnlit(input, out_document);

        const std::string empty_code_module_glsl;
        const std::string &code_module_glsl = input.code_module_glsl
            ? *input.code_module_glsl
            : empty_code_module_glsl;

        // The former assembler remains the single source of byte-stable
        // emission during this migration step. New template implementations
        // replace this delegation one template at a time.
        CompositorAssembler::CompositorModuleOptions legacy_options{};
        legacy_options.sky_module = input.module_options.sky_module;
        legacy_options.direct_lighting_module =
            input.module_options.direct_lighting_module;
        legacy_options.indirect_lighting_module =
            input.module_options.indirect_lighting_module;
        legacy_options.lighting_algorithm_module =
            input.module_options.lighting_algorithm_module;
        legacy_options.material_source_module =
            input.module_options.material_source_module;
        legacy_options.ntb_module = input.module_options.ntb_module;
        legacy_options.forward_lighting_module =
            input.module_options.forward_lighting_module;
        legacy_options.enable_material_source_provider =
            input.module_options.enable_material_source_provider;
        legacy_options.enable_ntb_provider =
            input.module_options.enable_ntb_provider;
        legacy_options.enable_scene_lighting =
            input.module_options.enable_scene_lighting;
        legacy_options.alpha_test = input.module_options.alpha_test;
        legacy_options.alpha_cutoff = input.module_options.alpha_cutoff;
        legacy_options.dither = input.module_options.dither;
        legacy_options.fragment_inputs = input.module_options.fragment_inputs;
        legacy_options.output_contract = input.module_options.output_contract;
        legacy_options.coverage_contract = input.module_options.coverage_contract;

        CompositorAssembler legacy_assembler;
        return legacy_assembler.AssembleDocument(
            input.surface,
            input.pass,
            input.fragment_source,
            input.surface_module,
            legacy_options,
            code_module_glsl,
            out_document,
            out_diagnostics);
    }
}
