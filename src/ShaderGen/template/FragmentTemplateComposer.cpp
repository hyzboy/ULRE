#include <hgl/mtl/FragmentTemplateComposer.h>
#include <hgl/log/Log.h>
#include <hgl/mtl/MaterialOutputContract.h>
#include <hgl/mtl/MaterialStageInterface.h>
#include <hgl/mtl/ShaderLibraryPath.h>
#include <fstream>
#include <map>
#include <string>

namespace hgl::graph::mtl
{
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

        // ── Step 3：外置 .tmpl 骨架 + 通用填槽循环 ──────────────────────────
        // 设计：每个渲染模板的"固定前导/收尾"骨架外置到
        // ShaderLibrary/fragment/<name>.glsl.tmpl（与 mesh 管线一致），
        // 仅保留占位符给真正动态的部分（defines / 各 slot 的 #include /
        // fragment inputs / output 声明 / code modules / surface 接线 / main 体）。
        // Composer 退化成"读 .tmpl → 填槽 → 发射"的通用循环，删除原本重复的
        // ComposeForwardLit / ComposeForwardUnlit / ComposeSky。
        //
        // 占位符替换约定（与 mesh 的 ApplyMeshTemplateSlot 同源，但额外吞掉
        // 占位符独占一行时其后的一个 '\n'，避免 .tmpl 行尾产生多余空行）：
        //   {{key}}  →  value；若紧接其后的字符是 '\n'，删除该 '\n'。
        // 因此每个占位符独占一行、且 value 以 '\n' 结尾时，不会多出空行。
        void ApplyFragmentTemplateSlot(
            std::string &text,
            const char *slot,
            const std::string &value)
        {
            if (!slot || !slot[0])
                return;

            const std::string marker = std::string("{{") + slot + "}}";
            size_t pos = 0;
            while ((pos = text.find(marker, pos)) != std::string::npos)
            {
                text.replace(pos, marker.size(), value);
                const size_t after = pos + value.size();
                if (after < text.size() && text[after] == '\n')
                    text.erase(after, 1);
                pos = after;
            }
        }

        // CRLF → LF：模板经 git 检出可能带 \r，归一化后生成结果与行尾策略无关
        // （与 mesh 管线 GetMeshShaderTemplate 一致）。
        std::string NormalizeEOL(const std::string &src)
        {
            std::string out;
            out.reserve(src.size());
            for (const char c : src)
                if (c != '\r')
                    out += c;
            return out;
        }

        const std::string &LoadFragmentShaderTemplate(const char *filename)
        {
            // 进程内缓存：模板在运行期不变，避免每次材质编译重复读盘。
            static std::map<std::string, std::string> cache;
            static const std::string empty;

            if (!filename || !filename[0])
                return empty;

            const std::string key(filename);
            const auto it = cache.find(key);
            if (it != cache.end())
                return it->second;

            const std::string path =
                GetShaderLibraryPath() + "/fragment/" + key;
            std::ifstream in(path, std::ios::binary);
            if (!in)
            {
                GLogError(
                    u8"[FragmentTemplateComposer] 模板文件缺失: %s", path.c_str());
                return empty;
            }

            std::string raw(
                (std::istreambuf_iterator<char>(in)),
                std::istreambuf_iterator<char>());
            return cache.emplace(key, NormalizeEOL(raw)).first->second;
        }

        const char *GetFragmentTemplateFile(const RenderTemplateID id) noexcept
        {
            switch (id)
            {
            case RenderTemplateID::ForwardLitShadowedAO:
            case RenderTemplateID::ForwardLitShadowedIdentityAO:
            case RenderTemplateID::ForwardLitUnshadowedAO:
                return "forward_lit.glsl.tmpl";
            case RenderTemplateID::ForwardUnlit:
                return "forward_unlit.glsl.tmpl";
            case RenderTemplateID::Sky:
                return "sky.glsl.tmpl";
            default:
                return nullptr;
            }
        }

        bool IsForwardLitTemplate(const RenderTemplateID id) noexcept
        {
            return id == RenderTemplateID::ForwardLitShadowedAO
                || id == RenderTemplateID::ForwardLitShadowedIdentityAO
                || id == RenderTemplateID::ForwardLitUnshadowedAO;
        }

        // fragment 输入（inter-stage varying）声明 → 单一文本块。
        bool BuildFragmentInputDeclarationText(
            const hgl::ValueArray<InterStageSemanticContractEntry> *fragment_inputs,
            std::string &out)
        {
            if (!fragment_inputs)
                return true;

            for (int index = 0; index < fragment_inputs->GetCount(); ++index)
            {
                AnsiString declaration;
                if (!BuildGLSLInterStageDeclaration(
                        (*fragment_inputs)[index], "in", declaration))
                    return false;
                out += declaration.c_str();
                out += "\n";
            }
            return true;
        }

        // 保留给 Shadow（块级发射）；其余模板走 BuildFragmentInputDeclarationText。
        bool AppendFragmentInputDeclarations(
            const hgl::ValueArray<InterStageSemanticContractEntry> *fragment_inputs,
            const char *logical_name,
            ShaderDocument &document)
        {
            std::string text;
            if (!BuildFragmentInputDeclarationText(fragment_inputs, text))
                return false;
            if (!text.empty())
                AddTemplateBlock(
                    document, ShaderDocumentBlockKind::Interface,
                    AnsiString(text.c_str()), logical_name);
            return true;
        }

        // 输出附件声明（layout(location=n) out ... + WriteMaterialOutput）→ 文本块。
        bool BuildOutputDeclarationText(
            const OutputContract *output_contract,
            const bool allow_integer_types,
            std::string &out)
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

                out += declaration;
            }
            return true;
        }

        bool AppendOutputDeclarations(
            const OutputContract *output_contract,
            const char *logical_name,
            const bool allow_integer_types,
            ShaderDocument &document)
        {
            std::string text;
            if (!BuildOutputDeclarationText(
                    output_contract, allow_integer_types, text))
                return false;
            if (!text.empty())
                AddTemplateBlock(
                    document, ShaderDocumentBlockKind::Interface,
                    AnsiString(text.c_str()), logical_name);
            return true;
        }

        // code module document（传递依赖闭包的内联 GLSL）原样拼接为单一文本块。
        bool BuildCodeModuleText(
            const ShaderDocument *code_module_document,
            std::string &out)
        {
            if (!code_module_document)
                return true;
            for (int index = 0;
                 index < code_module_document->GetBlockCount();
                 ++index)
                out += code_module_document->GetBlock(index).text.c_str();
            return true;
        }

        // 解析某 slot 的 #include；缺失（必需）则失败。
        bool BuildSlotIncludeText(
            const FragmentTemplateComposer::ComposeInput &input,
            const ShaderModuleSlotRole role,
            std::string &out)
        {
            const char *path = ResolvedInclude(input, role);
            if (!path)
                return false;
            out = "#include \"";
            out += path;
            out += "\"\n";
            return true;
        }

        // 按 template.slots 顺序发射全部模块 #include（ForwardLit 用）。
        // include 顺序唯一来源 = slots；可选 slot 未提供则跳过，必需缺失则失败。
        bool BuildAllSlotIncludeText(
            const FragmentTemplateComposer::ComposeInput &input,
            std::string &out)
        {
            const ResolvedRenderTemplate *resolved = input.resolved_template;
            if (!resolved || !resolved->definition)
                return false;

            const RenderTemplateDefinition &definition = *resolved->definition;
            for (hgl::uint32 index = 0; index < definition.slot_count; ++index)
            {
                const char *path =
                    ResolvedInclude(input, definition.slots[index].role);
                if (!path)
                {
                    if (definition.slots[index].required)
                        return false;
                    continue;
                }
                out += "#include \"";
                out += path;
                out += "\"\n";
            }
            return true;
        }

        // 模板固定宏（HGL_USE_* / MTL_TEX_* / alpha）。ForwardLit 恒为 1，
        // ForwardUnlit 按模块存在性赋值，Sky 无 Define 块。
        bool BuildFragmentDefines(
            const RenderTemplateID id,
            const FragmentTemplateComposer::ComposeInput &input,
            std::string &defines)
        {
            if (IsForwardLitTemplate(id))
            {
                defines += "#define HGL_USE_MATERIAL_SOURCE_PROVIDER 1\n";
                defines += "#define HGL_USE_NTB_PROVIDER 1\n";
                defines += "#define HGL_USE_SCENE_LIGHTING 1\n";
            }
            else if (id == RenderTemplateID::ForwardUnlit)
            {
                const char *material_source_module =
                    ResolvedInclude(input, ShaderModuleSlotRole::MaterialSourceProvider);
                const char *ntb_module =
                    ResolvedInclude(input, ShaderModuleSlotRole::NTBProvider);
                defines += "#define HGL_USE_MATERIAL_SOURCE_PROVIDER ";
                defines += material_source_module ? "1\n" : "0\n";
                defines += "#define HGL_USE_NTB_PROVIDER ";
                defines += ntb_module ? "1\n" : "0\n";
                defines += "#define HGL_USE_SCENE_LIGHTING 0\n";
            }
            else
            {
                // Sky（及其它未外置模板）无 Define 块。
                return true;
            }

            AppendTextureChannelDefines(defines, input);
            if (input.alpha_test)
            {
                defines += "#define HGL_ALPHA_TEST 1\n#define HGL_ALPHA_CUTOFF ";
                defines += std::to_string(input.alpha_cutoff);
                defines += "\n";
            }
            if (input.dither)
                defines += "#define HGL_ALPHA_DITHER 1\n";
            return true;
        }

        bool BuildSurfaceWiringText(
            const FragmentTemplateComposer::ComposeInput &input,
            const bool camera_ubo_available,
            std::string &out)
        {
            if (!input.fragment_inputs)
                return true;
            AnsiString wiring;
            if (!BuildGLSLMaterialSurfaceInput(
                    *input.fragment_inputs, camera_ubo_available, wiring))
                return false;
            out = wiring.c_str();
            return true;
        }

        bool ComposeFromFragmentTemplate(
            const FragmentTemplateComposer::ComposeInput &input,
            ShaderDocument &document)
        {
            const RenderTemplateID id = input.resolved_template
                ? input.resolved_template->request.template_id
                : RenderTemplateID::Unknown;
            const char *tpl_file = GetFragmentTemplateFile(id);
            if (!tpl_file)
                return false;

            std::string tpl = LoadFragmentShaderTemplate(tpl_file);
            if (tpl.empty())
                return false;

            std::string defines, fragment_inputs, output_declarations;
            std::string code_modules, wiring;
            std::string sky_provider_inc, output_policy_inc;
            std::string material_source_inc, surface_inc, module_includes;

            if (!BuildFragmentDefines(id, input, defines))
                return false;
            if (!BuildFragmentInputDeclarationText(
                    input.fragment_inputs, fragment_inputs))
                return false;
            if (!BuildOutputDeclarationText(
                    input.output_contract, false, output_declarations))
                return false;
            if (!BuildCodeModuleText(input.code_module_document, code_modules))
                return false;
            if (!BuildSurfaceWiringText(
                    input, IsForwardLitTemplate(id), wiring))
                return false;

            // 各 slot 的 #include：ForwardLit 走统一的 module_includes（slots 顺序）；
            // ForwardUnlit / Sky 按各自骨架中显式标注的占位符单独填。
            if (!BuildAllSlotIncludeText(input, module_includes))
                return false;
            if (!BuildSlotIncludeText(
                    input, ShaderModuleSlotRole::SkyProvider, sky_provider_inc))
                sky_provider_inc.clear();
            if (!BuildSlotIncludeText(
                    input, ShaderModuleSlotRole::OutputPolicy, output_policy_inc))
                output_policy_inc.clear();
            if (!BuildSlotIncludeText(
                    input, ShaderModuleSlotRole::MaterialSourceProvider,
                    material_source_inc))
                material_source_inc.clear();
            if (!BuildSlotIncludeText(
                    input, ShaderModuleSlotRole::SurfaceProvider, surface_inc))
                surface_inc.clear();

            // 必需 slot 缺失 → 失败（ForwardUnlit / Sky 的 output/sky/surface 为必需）。
            if (id == RenderTemplateID::ForwardUnlit
             && (output_policy_inc.empty()
              || material_source_inc.empty()
              || surface_inc.empty()))
                return false;
            if (id == RenderTemplateID::Sky
             && (sky_provider_inc.empty() || surface_inc.empty()))
                return false;

            // [SkyRoute 诊断] Sky 模板实际注入的 surface 模块（保留 ComposeSky 日志）。
            if (id == RenderTemplateID::Sky && !surface_inc.empty())
                GLogInfo(
                    "[SkyRoute] ComposeFromFragmentTemplate surface_module=%s",
                    surface_inc.c_str());

            ApplyFragmentTemplateSlot(tpl, "defines", defines);
            ApplyFragmentTemplateSlot(tpl, "module_includes", module_includes);
            ApplyFragmentTemplateSlot(tpl, "sky_provider_include", sky_provider_inc);
            ApplyFragmentTemplateSlot(tpl, "output_policy_include", output_policy_inc);
            ApplyFragmentTemplateSlot(tpl, "material_source_include", material_source_inc);
            ApplyFragmentTemplateSlot(tpl, "surface_include", surface_inc);
            ApplyFragmentTemplateSlot(tpl, "fragment_inputs", fragment_inputs);
            ApplyFragmentTemplateSlot(tpl, "output_declarations", output_declarations);
            ApplyFragmentTemplateSlot(tpl, "code_modules", code_modules);
            ApplyFragmentTemplateSlot(tpl, "surface_input_wiring", wiring);

            // 发射：Version 块 + 单 MainBody 块。Block 边界不进入最终 GLSL
            // （Serialize 仅拼接），但为满足 ValidateSourceDocument 的不变量
            // （block0=Version、末块=MainBody、每块 stage/logical_name 非空），
            // 采用 [Version, MainBody] 两块的规范形态。
            document.Clear();
            ShaderDocumentSource source;
            source.stage = "fragment";
            source.logical_name = "FragmentTemplate";
            document.Add(
                ShaderDocumentBlockKind::Version,
                AnsiString("#version 450\n"), source);
            source.logical_name = "FragmentTemplate.Body";
            document.Add(
                ShaderDocumentBlockKind::MainBody, AnsiString(tpl.c_str()), source);
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
    }

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
        if (template_id == RenderTemplateID::ShadowCasterOpaque
         || template_id == RenderTemplateID::ShadowCasterMasked)
          return ComposeShadow(input, out_document);

        if (GetFragmentTemplateFile(template_id))
          return ComposeFromFragmentTemplate(input, out_document);

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
