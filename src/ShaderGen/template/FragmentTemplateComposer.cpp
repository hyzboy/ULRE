#include <hgl/mtl/FragmentTemplateComposer.h>
#include <hgl/log/Log.h>
#include <hgl/mtl/MaterialOutputContract.h>
#include <hgl/mtl/MaterialStageInterface.h>
#include <hgl/mtl/ShaderConfig.h>
#include <hgl/mtl/PipelineConfigEmitter.h>
#include <hgl/mtl/ShaderLibraryPath.h>
#include <hgl/util/hash/FNV1a.h>
#include "../compile/MaterialShaderEmitter.h"
#include <fstream>
#include <map>
#include <string>

namespace hgl::graph::mtl
{
    namespace
    {
        using namespace hgl::graph::mtl;
        using hgl::AnsiString;

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

        // 纹理声明里的 channels → 配置里的通道覆盖项。
        // 唯一发射器是 EmitShaderConfigDefines；这里只负责从材质声明搬运数据。
        // 返回 false 表示槽名超出配置缓冲，宁可让编译失败也不要静默改名。
        bool CollectTextureChannelOverrides(
            ShaderConfig &config,
            const FragmentTemplateComposer::ComposeInput &input)
        {
            if (!input.texture_declarations)
                return true;

            for (const MaterialTextureDeclaration &declaration :
                *input.texture_declarations)
            {
                if (declaration.channels == 0)
                    continue;

                ShaderConfigTextureChannel channel;
                if (!SetShaderConfigTextureChannelName(channel, declaration.name.c_str()))
                    return false;

                channel.channels = static_cast<unsigned char>(declaration.channels);
                config.texture_channels.Add(channel);
            }

            return true;
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

        /// 骨架文件名映射的唯一真源在 RenderTemplate.cpp：声明键阶段也要
        /// 用它定位骨架、扫出硬编码的 `#include` 基座，不能各存一份。
        const char *GetFragmentTemplateFile(const RenderTemplateID id) noexcept
        {
            return GetRenderTemplateFragmentSourceFile(id);
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
        // 判定与排版都委托 PipelineConfigEmitter——本函数只做「接线」，
        // 不允许在这里重写遍历/缺失处理逻辑（那会变成第二真源）。
        bool BuildAllSlotIncludeText(
            const FragmentTemplateComposer::ComposeInput &input,
            std::string &out)
        {
            const ResolvedRenderTemplate *resolved = input.resolved_template;
            if (!resolved)
                return false;

            PipelineConfig config;
            if (!EmitPipelineConfig(*resolved, config))
                return false;
            EmitPipelineConfigText(config, out);
            return true;
        }

        // 模板固定宏（HGL_USE_* / MTL_TEX_* / alpha）→ ShaderConfig → 唯一发射器。
        // 本函数只负责**判定**，不负责排版；宏的名字/顺序/格式一律由
        // EmitShaderConfigDefines 决定（见 inc/hgl/mtl/ShaderConfig.h）。
        bool BuildFragmentDefines(
            const RenderTemplateID id,
            const FragmentTemplateComposer::ComposeInput &input,
            std::string &defines)
        {
            ShaderConfig config;
            config.alpha_test = input.alpha_test;
            config.alpha_cutoff = input.alpha_cutoff;
            config.alpha_dither = input.dither;
            config.alpha_output = input.alpha_to_output;

            if (IsForwardLitTemplate(id))
            {
                config.profile = ShaderConfigProfile::ForwardLit;
                config.use_material_source_provider = true;
                config.use_scene_lighting = true;
            }
            else if (id == RenderTemplateID::ForwardUnlit)
            {
                config.profile = ShaderConfigProfile::ForwardUnlit;
                config.use_material_source_provider =
                    ResolvedInclude(input, ShaderModuleSlotRole::MaterialSourceProvider) != nullptr;
                config.use_scene_lighting = false;
            }
            else if (id == RenderTemplateID::ShadowCasterOpaque
                  || id == RenderTemplateID::ShadowCasterMasked)
            {
                // 深度/阴影 pass：只有 HGL_COVERAGE_ONLY + 共享的 alpha 三宏。
                // 纹理通道宏与 PCF 宏都不发射——本 pass 既不读 NTB 通道语义
                // （NTB 槽恒为 ntb_none），也没有 shadow_provider 槽。
                config.profile = ShaderConfigProfile::ShadowCaster;
                EmitShaderConfigDefines(config, defines);
                return true;
            }
            else
            {
                // Sky（及其它未外置模板）无 Define 块 —— 整段配置都不发射。
                // 注意这里必须**提前返回**：纹理通道 / 阴影 / alpha 宏同样不发射，
                // 否则 "#宏存在即非零" 的 #if defined() 判断会被带偏。
                config.profile = ShaderConfigProfile::None;
                EmitShaderConfigDefines(config, defines);
                return true;
            }

            if (!CollectTextureChannelOverrides(config, input))
                return false;

            // 阴影 PCF 采样宏：只有模板真的带 shadow_provider 槽时才发射。
            // 该宏落在 .tmpl 的 {{defines}} 块（早于 {{module_includes}}），
            // 因此 pcf_shadow.glsl 的 #ifndef 会取这里的值。
            config.emit_shadow_pcf =
                ResolvedInclude(input, ShaderModuleSlotRole::ShadowProvider) != nullptr;
            config.shadow_pcf_poisson_taps = input.shadow_pcf_poisson_taps;
            config.shadow_normal_offset = input.shadow_normal_offset;

            EmitShaderConfigDefines(config, defines);
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

        bool IsShadowCasterTemplate(const RenderTemplateID id) noexcept
        {
            return id == RenderTemplateID::ShadowCasterOpaque
                || id == RenderTemplateID::ShadowCasterMasked;
        }

        bool RequiresAlphaEvaluation(
            const FragmentTemplateComposer::ComposeInput &input) noexcept
        {
            return input.coverage_contract
                && input.coverage_contract->requires_alpha_evaluation;
        }

        /// ShadowCaster 骨架的 `{{alpha_resources}}` / `{{alpha_includes}}` 两槽。
        ///
        /// 深度/阴影 pass 有两条互斥形态：opaque 只需一个空 `main`，masked 还要
        /// 求 alpha，因而额外拖进 surface 接口、场景 UBO、alpha 合成器与材质源/
        /// NTB/surface 三个模块。两段都是**固定文本**，所以外置到骨架里，由这里
        /// 决定填什么；空串等于整行消失（见 ApplyFragmentTemplateSlot 的吞行约定）。
        bool BuildShadowAlphaIncludeText(
            const FragmentTemplateComposer::ComposeInput &input,
            std::string &out_resources,
            std::string &out_includes)
        {
            out_resources.clear();
            out_includes.clear();
            if (!RequiresAlphaEvaluation(input))
                return true;

            // BINDLESS_SET 等宏由 descriptor_macros 提供，scene_ubo 必须排在
            // surface_interface 之后。
            out_resources += IncludeTemplate("common/surface_interface.glsl").c_str();
            out_resources += IncludeTemplate("ubo/scene_ubo.glsl").c_str();

            out_includes += IncludeTemplate("common/alpha_compositor.glsl").c_str();

            // 材质源可缺省（opaque 模板的该槽 required=false）；masked 时由模板表
            // 强制存在，缺失会在 BuildAllSlotIncludeText 处先报错。
            const char *material_source_module = ResolvedInclude(
                input, ShaderModuleSlotRole::MaterialSourceProvider);
            if (material_source_module)
                out_includes += IncludeTemplate(material_source_module).c_str();

            const char *surface_module = ResolvedInclude(
                input, ShaderModuleSlotRole::SurfaceProvider);
            if (!surface_module)
                return false;
            // NTB 必须早于 surface：material_surface.glsl 的定义体里无条件调用
            // GetNTB（本 pass 只用 EvalAlpha，但 GLSL 在定义处就要解析符号）。
            const char *ntb_module = ResolvedInclude(
                input, ShaderModuleSlotRole::NTBProvider);
            if (!ntb_module)
                return false;
            out_includes += IncludeTemplate(ntb_module).c_str();
            out_includes += IncludeTemplate(surface_module).c_str();
            return true;
        }

        /// ShadowCaster 骨架的 `{{main_body}}` 槽。
        ///
        /// 唯一带控制流的骨架文本：是否求 alpha 由 coverage 契约决定，且
        /// `materialDataIndex` 只在契约要求 DataIndexID 时才可用——否则 wiring
        /// 没声明它，引用即编译错误。此前这里硬编码 `EvalAlpha(si, 0u)`，
        /// batch 内非 0 号物体的 opacity 槽会读到别人的行（表现为 masked 阴影
        /// 与 opaque 一样实心），所以这两个分支不能合并。
        bool BuildShadowMainBodyText(
            const FragmentTemplateComposer::ComposeInput &input,
            const std::string &wiring,
            std::string &out)
        {
            if (!RequiresAlphaEvaluation(input))
            {
                out = "void main()\n{\n}\n";
                return true;
            }

            // 前导空行：骨架里 include 段与 main 之间空一行，历史发射如此。
            out = "\nvoid main()\n{\n";
            const bool wiring_data_index =
                (input.coverage_contract->required_semantics
                 & GetInterStageSemanticMask(InterStageSemantic::DataIndexID)) != 0;

            if (input.fragment_inputs)
            {
                out += wiring;
                out += "    const float alpha = EvalAlpha(si, ";
                out += wiring_data_index ? "materialDataIndex" : "0u";
                out += ");\n";
            }
            else
            {
                // wiring 缺失（无 fragment_inputs）时的零值兜底。
                out +=
                    "    SurfaceInput si;\n"
                    "    si.worldPos = vec3(0.0);\n"
                    "    si.worldNormal = vec3(0.0, 0.0, 1.0);\n"
                    "    si.uv0 = vec2(0.0);\n"
                    "    si.uv1 = vec2(0.0);\n"
                    "    si.vertexColor = vec4(1.0);\n"
                    "    si.viewDir = vec3(0.0, 0.0, 1.0);\n"
                    "    si.screenPos = gl_FragCoord.xy;\n"
                    "    si.luminance = 1.0;\n"
                    "    si.styleID = 0u;\n"
                    "    const float alpha = EvalAlpha(si, 0u);\n";
            }
            out +=
                "    HGLApplyAlpha(alpha);\n"
                "}\n";
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
            std::string alpha_resources, alpha_includes, main_body;

            const bool is_shadow = IsShadowCasterTemplate(id);

            if (!BuildFragmentDefines(id, input, defines))
                return false;
            if (!BuildFragmentInputDeclarationText(
                    input.fragment_inputs, fragment_inputs))
                return false;
            // 深度/阴影 pass 的输出是深度/模板附件，允许整型附件；颜色 pass 不允许。
            if (!BuildOutputDeclarationText(
                    input.output_contract, is_shadow, output_declarations))
                return false;
            if (!BuildCodeModuleText(input.code_module_document, code_modules))
                return false;
            if (!BuildSurfaceWiringText(
                    input, IsForwardLitTemplate(id), wiring))
                return false;

            // ShadowCaster 的 alpha 资源 / include / main 三段（其它模板没有对应
            // 占位符，填了也无处落地）。
            if (is_shadow)
            {
                if (!BuildShadowAlphaIncludeText(
                        input, alpha_resources, alpha_includes))
                    return false;
                if (!BuildShadowMainBodyText(input, wiring, main_body))
                    return false;
            }

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

            // 必需 slot 的强制**只有一处**：上面那次 BuildAllSlotIncludeText —— 它按模板表
            // （RenderTemplate.cpp 的槽表）的 required 判定，且在本函数里**先于**此处执行。
            // 此处曾另按模板 ID 硬编码一份必需集合（同一事实的第二处表达，且与表中
            // required=false 的槽语义冲突），已整删（S4a，见评审档 §19.1）。

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
            ApplyFragmentTemplateSlot(tpl, "alpha_resources", alpha_resources);
            ApplyFragmentTemplateSlot(tpl, "alpha_includes", alpha_includes);
            ApplyFragmentTemplateSlot(tpl, "main_body", main_body);

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

    }

    // ── 接口契约哈希（不吃源文本）────────────────────────────────────────
    //
    // 接口维度定义「片元阶段对外声明的形状」，与实现文本无关。键里保留这个
    // 独立维度是为了失效排查：接口变了还是实现变了，一眼可分。
    uint64 FragmentTemplateComposer::GetInterfaceInputHash(
        const ComposeInput &input) noexcept
    {
        hgl::hash::FNV1aHasher64 hasher;

        hasher << static_cast<uint64>(
            input.output_contract
                ? GetOutputContractHash(*input.output_contract)
                : 0);
        hasher << static_cast<uint64>(input.alpha_to_output ? 1 : 0);

        const int fragment_input_count =
            input.fragment_inputs ? input.fragment_inputs->GetCount() : 0;
        hasher << static_cast<uint64>(fragment_input_count);
        // 逐项折叠走 CanonicalShaderContract.h 里 InterStageSemanticContractEntry
        // 的显式 operator<<（6 个语义字段），不吃 padding。
        for (int i = 0; i < fragment_input_count; ++i)
            hasher << (*input.fragment_inputs)[i];

        return hasher;
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
