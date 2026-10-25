// DomainShaderBuildBackend.cpp — 「不再生成着色器」的构建后端。
//
// ═══════════════════════════════════════════════════════════════════════════
// 这个后端做的是**减法**：
// ═══════════════════════════════════════════════════════════════════════════
//
// 生成器后端（ShaderGenProgramBuildBackend）把所有代码模块拼装成一份完整
// GLSL 再编译。本后端只做五件事：
//
//   1. 借 `BuildGenericMaterial` 拿到网格阶段、link spec、artifact 元数据、
//      `requires_recipe_*` 结论（这些与「片段算法怎么写」无关，废除生成器
//      不代表废除它们）；
//   2. 把片段阶段的最终 GLSL **整篇替换**成领域固定主着色器
//      （`ShaderLibrary/domain/...`）——它本身就是一个完整编译单元；
//   3. 按领域/pass/渲染状态发射 `generated/options.glsl`（Phase 2 的
//      `EmitDomainOptions`），写到缓存目录，不进源码树；
//   4. 把编译器插件的 include 根设成 { ShaderLibrary, options 所在目录 }，
//      立刻把主着色器编成 SPV；
//   5. 交回 `FinalizeShaderBuildContext` 走原有的缓存/落盘/元数据流程。
//
// 关键点：**主着色器不是生成物**。它没有 `{{...}}` 占位符、没有被拼接的
// 代码模块，因此这一层不需要任何文本拼装逻辑——这正是删除 ShaderGen 的
// 意义所在。唯一"生成"的东西是 options（宏定义 + 一个 ambient include）。
//
// ═══════════════════════════════════════════════════════════════════════════
// 为什么临时放在 src/ShaderGen 里
// ═══════════════════════════════════════════════════════════════════════════
//
// 它仍然调用 `BuildGenericMaterial`（生成期代码），所以不能进
// ShaderMaterialData——那会造成「数据层反依赖生成器」的倒挂。等网格阶段
// 也脱离生成器（阶段五），本文件连同 `BuildGenericMaterial` 一起搬走。

#include <hgl/mtl/ShaderProgramBuildBackend.h>
#include <hgl/mtl/ShaderBuildContext.h>
#include <hgl/mtl/ShaderCreateInfo.h>
#include <hgl/mtl/MaterialShaderCompiler.h>
#include <hgl/mtl/MaterialDefinitionRegistry.h>
#include <hgl/mtl/DomainOptions.h>
#include <hgl/mtl/DomainShaderTemplate.h>
#include <hgl/mtl/ShaderLibraryPath.h>
#include <hgl/mtl/ShaderCacheRoot.h>
#include <hgl/mtl/ShaderCompilerProfileAPI.h>
#include <hgl/mtl/ShaderProgramArtifactBuilder.h>
#include <hgl/graph/geo/GeometryVertexFormat.h>
#include <hgl/filesystem/FileSystem.h>
#include <hgl/io/FileOutputStream.h>
#include <hgl/log/Log.h>
#include <hgl/type/String.h>
#include <hgl/util/hash/FNV1a.h>

#include "builder/GenericMaterialBuilder.h"
#include "ShaderGenBackendInstall.h"

#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <string>

namespace hgl::graph::mtl
{
    namespace
    {
        /// 后端选择器：值为 `domain` 时用本文件的后端，否则沿用生成器后端。
        /// 默认（未设置）必须保持原行为——废除生成器是阶段五的事。
        constexpr const char *kBackendSelectorEnvironmentVariable =
            "ULRE_SHADER_BUILD_BACKEND";

        // ── 小工具 ────────────────────────────────────────────────────────

        /// 最近一次领域后端失败原因。
        /// `GLogError` 走的是日志通道——门禁/工具里通常看不到（stdout 里只剩
        /// 「build failed」），所以失败出口同时往这里记一份，调用方用
        /// `GetLastDomainShaderBuildBackendError()` 取走放进自己的诊断。
        /// 与 `GetLastBuildGenericMaterialError()` 同一动机。
        AnsiString g_last_domain_build_error;

        void DomainBuildFailed(const char8_t *format, ...)
        {
            char buffer[1024];
            va_list args;
            va_start(args, format);
            vsnprintf(buffer, sizeof(buffer),
                      reinterpret_cast<const char *>(format), args);
            va_end(args);

            g_last_domain_build_error = buffer;
            GLogError(u8"%s", buffer);
        }

        bool LoadTextFile(const OSString &path, AnsiString &out_text)
        {
            int64 file_size = 0;
            void *file_data = filesystem::LoadFileToMemory(path, file_size);
            if (!file_data)
                return false;

            if (file_size > 0)
                out_text = AnsiString(static_cast<const char *>(file_data),
                                      static_cast<int>(file_size));
            else
                out_text.Clear();

            delete[] static_cast<uint8 *>(file_data);
            return true;
        }

        bool WriteTextFile(const OSString &path, const AnsiString &text)
        {
            io::FileOutputStream *fos = io::CreateFileOutputStream(path);
            if (!fos)
                return false;

            const int64 expected = text.Length();
            const int64 written = expected > 0
                ? fos->Write(text.c_str(), expected)
                : 0;

            delete fos;

            return written == expected;
        }

        /// options 目录名由**文本内容**决定：内容相同 ⇒ 复用同一目录（写是幂等的），
        /// 内容不同 ⇒ 目录不同（不可能读到别人的 options）。
        void MakeOptionsKey(const AnsiString &options_text,
                            const MaterialDomain domain,
                            const ShaderProgramPurpose purpose,
                            char *out_key,
                            const size_t out_key_size)
        {
            hgl::hash::FNV1aHasher64 hasher;
            hasher.AppendBytes(options_text.c_str(), static_cast<size_t>(options_text.Length()));
            hasher << static_cast<uint64>(domain);
            hasher << static_cast<uint64>(purpose);

            snprintf(out_key, out_key_size, "%016llx",
                     static_cast<unsigned long long>(static_cast<uint64>(hasher)));
        }
    }//namespace

    // ═══════════════════════════════════════════════════════════════════════
    // 组装 BuildContext
    // ═══════════════════════════════════════════════════════════════════════

    namespace
    {
        /// 顶点格式没有 Normal 但材质要法线光照 ⇒ 主着色器要算面法线。
        /// 判据与 `GenericMaterialBuilder::PrepareResolvedDefinition` 同源
        /// （`MaterialWantsNormalLighting` + 顶点格式），两处必须一致：那边决定
        /// 顶点阶段是否剥掉 N/T/B 需求，这边决定片段阶段要不要 dFdx/dFdy。
        bool ResolveFaceNormalFallback(
            const MaterialDefinition &definition,
            const MaterialDefinitionBuildRequest &request) noexcept
        {
            if (!request.geometry_vertex_format
             || !MaterialWantsNormalLighting(definition))
                return false;

            return request.geometry_vertex_format->Find(VertexSemantic::Normal)
                == nullptr;
        }

        /// 与 `ResolveFaceNormalFallback` 同判据的互补分支：顶点格式**有** Normal。
        /// 口径直接对齐 `GenericMaterialBuilder::PrepareResolvedDefinition` ——
        /// 那里两条路各自 push 一个 compile_define，这里必须发同样的宏。
        bool ResolveVertexNormalPresent(
            const MaterialDefinition &definition,
            const MaterialDefinitionBuildRequest &request) noexcept
        {
            if (!request.geometry_vertex_format
             || !MaterialWantsNormalLighting(definition))
                return false;

            return request.geometry_vertex_format->Find(VertexSemantic::Normal)
                != nullptr;
        }

        /// 模板里是否真的挂了 shadow provider 槽。只有挂了的主着色器才会
        /// include `shadow/pcf_shadow.glsl`，也只有它需要 PCF 宏。
        bool HasShadowProviderSlot(const MaterialDefinitionBuildRequest &request) noexcept
        {
            return request.render_template_request.FindModuleRoot(
                       ShaderModuleSlotRole::ShadowProvider) != nullptr;
        }

        /// 环境光 provider 的 include 路径（`lighting/indirect_sky_ambient.glsl`）。
        /// 主着色器的固定 include 表里不含 ambient 模块，必须由 options 带进来。
        const char *GetAmbientIncludePath(
            const MaterialDefinitionBuildRequest &request) noexcept
        {
            const RenderTemplateModuleRoot *root =
                request.render_template_request.FindModuleRoot(
                    ShaderModuleSlotRole::AmbientLightProvider);

            if (!root || root->include_path.IsEmpty())
                return nullptr;

            return root->include_path.c_str();
        }

        bool BuildDomainOptions(
            const MaterialDefinition &definition,
            const MaterialDefinitionBuildRequest &request,
            const MaterialDomain domain,
            const ShaderProgramPurpose purpose,
            AnsiString &out_text)
        {
            const bool depth_purpose = IsDomainDepthPurpose(purpose);

            DomainOptionsInput input;
            input.profile = ResolveDomainOptionsProfile(
                depth_purpose,
                definition.material_source_module_name,
                definition.ntb_module_name);

            if (input.profile == DomainOptionsProfile::ForwardLit)
            {
                input.ambient_include_path = GetAmbientIncludePath(request);
                if (!input.ambient_include_path)
                {
                    DomainBuildFailed(
                        u8"[DomainBackend] 领域 %d 需要 ForwardLit options，但渲染模板没有"
                        u8" AmbientLightProvider 槽（材质 %s）",
                        static_cast<int>(domain),
                        definition.definition_id.c_str());
                    return false;
                }
            }

            // 深度/阴影 pass 自己就在写深度，模板里没有 shadow provider 槽。
            // EmitDomainOptions 内部也会按 profile 跳过，这里提前给个明确值。
            input.emit_shadow_pcf = !depth_purpose && HasShadowProviderSlot(request);

            // shadow_pcf_poisson_taps / shadow_normal_offset 用 DomainOptionsInput
            // 的默认值（16 / 1）——生成器侧同样是这两个常量
            // （FragmentTemplateComposer 的默认值，全流程无人改写）。

            input.face_normal_fallback = ResolveFaceNormalFallback(definition, request);
            input.vertex_normal_present = ResolveVertexNormalPresent(definition, request);

            // compile_defines（`.mi.toml` 的 `defines`）→ 逐条 `#define <name> 1`。
            // 只在本函数内活着：EmitDomainOptions 同步读完，不留悬垂指针。
            std::vector<const char *> definition_defines;
            definition_defines.reserve(definition.compile_defines.size());
            for (const std::string &name : definition.compile_defines)
            {
                if (!name.empty())
                    definition_defines.push_back(name.c_str());
            }
            if (!definition_defines.empty())
            {
                input.definition_defines = definition_defines.data();
                input.definition_define_count =
                    static_cast<uint32>(definition_defines.size());
            }

            input.texture_declarations = definition.texture_declarations.empty()
                ? nullptr
                : definition.texture_declarations.data();
            input.texture_declaration_count =
                static_cast<uint32>(definition.texture_declarations.size());

            ApplyRenderStateToDomainOptions(
                input, ResolveMaterialRenderState(definition, request.recipe));

            return EmitDomainOptions(input, out_text);
        }

        /// 把 options 写到缓存目录并返回**包含 `generated/` 的那一层**目录
        /// （编译器插件的第二 include 根就是它：`generated/x.glsl` 会去掉前缀
        /// 后在该目录下解析）。
        bool WriteOptionsToCache(
            const AnsiString &options_text,
            const MaterialDomain domain,
            const ShaderProgramPurpose purpose,
            OSString &out_options_root)
        {
            const OSString cache_root = GetShaderCacheRootPath();
            if (cache_root.IsEmpty())
            {
                DomainBuildFailed(u8"[DomainBackend] 着色器缓存根目录不可用，options.glsl 无处落盘");
                return false;
            }

            char key[32];
            MakeOptionsKey(options_text, domain, purpose, key, sizeof(key));

            const filesystem::Path options_root =
                filesystem::Path(cache_root)
                / OSString(OS_TEXT("domain-options"))
                / OSString(ToOSString(key));
            const OSString generated_dir =
                (options_root / OSString(OS_TEXT("generated"))).ToOSString();

            if (!filesystem::MakePath(generated_dir))
            {
                DomainBuildFailed(u8"[DomainBackend] 无法创建 options 目录：%s",
                    ShaderLibraryPathToUTF8(generated_dir).c_str());
                return false;
            }

            const OSString options_path =
                (options_root / OSString(OS_TEXT("generated"))
                 / OSString(OS_TEXT("options.glsl"))).ToOSString();

            if (!WriteTextFile(options_path, options_text))
            {
                DomainBuildFailed(u8"[DomainBackend] 写入 options.glsl 失败：%s",
                    ShaderLibraryPathToUTF8(options_path).c_str());
                return false;
            }

            out_options_root = options_root.ToOSString();
            return true;
        }

        /// 读领域主着色器源文件（`ShaderLibrary/domain/<领域>/<文件>.frag`）。
        bool LoadDomainMasterShader(
            const DomainShaderTemplate &domain_template,
            AnsiString &out_source)
        {
            const std::string library_root = GetShaderLibraryPath();
            if (library_root.empty())
            {
                DomainBuildFailed(u8"[DomainBackend] 找不到 ShaderLibrary 根目录");
                return false;
            }

            const OSString path = ResolveShaderLibraryModulePath(
                ToOSString(library_root.c_str()),
                domain_template.relative_path);

            if (!LoadTextFile(path, out_source))
            {
                DomainBuildFailed(u8"[DomainBackend] 读取领域主着色器失败：%s",
                    ShaderLibraryPathToUTF8(path).c_str());
                return false;
            }

            if (out_source.Length() <= 0)
            {
                DomainBuildFailed(u8"[DomainBackend] 领域主着色器是空文件：%s",
                    ShaderLibraryPathToUTF8(path).c_str());
                return false;
            }

            return true;
        }

        /// 用「主着色器 + options」直接编出片段 SPV。
        ///
        /// 必须在这里编、而不是留给 `CreateShaderDirect()`：include 根是插件里的
        /// **进程级**配置（`compile_info.includes`），只有在编译那一刻设对才有意义，
        /// 而 `FinalizeShaderBuildContext` 稍后还要编网格阶段——那时第二根必须已经
        /// 撤掉，否则它会读到上一份 options。
        bool CompileDomainMasterFragment(
            const AnsiString &source,
            const AnsiString &options_text,
            const OSString &options_root,
            const MaterialDomain domain,
            const ShaderProgramPurpose purpose,
            ShaderBuildContext &ctx)
        {
            ShaderCreateInfo *fragment = ctx.GetStageShader(ShaderStage::Fragment);
            if (!fragment)
            {
                DomainBuildFailed(u8"[DomainBackend] 构建上下文缺少片段阶段");
                return false;
            }

            // ── 把 options 的内容键钉进片段源 ────────────────────────────
            //
            // options 的全部差异都落在 `generated/options.glsl` 里，主着色器文本
            // 对「同领域 + 同 pass」是**逐字节相同**的。若 stage source digest 只
            // 覆盖主着色器，两种不同 options 就会算出同一个摘要（实测：
            // fwd 与 fwd-alpha 的主着色器文本完全一致，SPV 却是 65072 vs 67024）。
            // 摘要既是缓存命中的判据也是事后校验的判据，撞了就只剩缓存文件名这
            // 一道保险。把内容键作为溯源注释钉在文本尾部，摘要自然覆盖 options，
            // 同时让落盘的 `.frag` 能自述「这份 SPV 是哪组宏编出来的」。
            char options_key[32];
            MakeOptionsKey(options_text, domain, purpose,
                           options_key, sizeof(options_key));

            std::string fragment_source(source.c_str(),
                                        static_cast<size_t>(source.Length()));
            fragment_source += "\n// domain-options-key ";
            fragment_source += options_key;
            fragment_source += "\n";

            fragment->SetFinalGLSL(fragment_source);

            // ── 缓存优先 ────────────────────────────────────────────────
            //
            // 主着色器是**物理固定**的，所以「这份 options 编出来的 SPV」跨进程
            // 可复用，判据就是舞台自己的 source digest。先探缓存再编译：不做这
            // 一步，命中缓存也照样白烧一遍 glslang（`FinalizeShaderBuildContext`
            // 的命中分支只是不再覆盖已装好的 SPV）。
            // 探不到就照常编，后续 `FinalizeShaderBuildContext` 负责落盘。
            if (ShaderArtifactStore *store = ctx.GetArtifactStore())
            {
                if (ctx.HasProgramLink())
                {
                    const ShaderStageKey &stage_key =
                        ctx.GetProgramLink().fragment_stage;

                    const uint64 expected_digest =
                        ComputeGeneratedStageSourceDigest(
                            fragment->GetFinalGLSL().data(),
                            fragment->GetFinalGLSL().size());

                    ValueArray<uint8> cached_spv;
                    uint64 stored_digest = 0;
                    if (expected_digest != 0
                     && store->LoadStageSPV(stage_key, cached_spv, &stored_digest)
                     && stored_digest == expected_digest
                     && fragment->SetCachedSPVData(
                            cached_spv.GetData(), cached_spv.GetCount()))
                        return true;
                }
            }

            const std::string library_root = GetShaderLibraryPath();
            if (library_root.empty())
            {
                DomainBuildFailed(u8"[DomainBackend] 找不到 ShaderLibrary 根目录");
                return false;
            }

            const std::string options_root_utf8 =
                ShaderLibraryPathToUTF8(options_root);

            const char *roots[2] = { library_root.c_str(), options_root_utf8.c_str() };
            if (!SetShaderIncludeRoots(roots, 2))
            {
                DomainBuildFailed(u8"[DomainBackend] 设置 include 根失败：%s | %s",
                    library_root.c_str(), options_root_utf8.c_str());
                return false;
            }

            AnsiString diagnostic;
            const bool compiled = fragment->CompileFinalGLSLToSPVWithDiagnostic(diagnostic);

            // 无论成败都收回第二根：残留的 options 根会让后续编译读到错误宏。
            const char *library_only[1] = { library_root.c_str() };
            SetShaderIncludeRoots(library_only, 1);

            if (!compiled)
            {
                DomainBuildFailed(
                    u8"[DomainBackend] 领域主着色器编译失败：domain=%d purpose=%d\n%s",
                    static_cast<int>(domain),
                    static_cast<int>(purpose),
                    diagnostic.c_str() ? diagnostic.c_str() : "");
                return false;
            }

            return true;
        }

        /// 片段源被整篇换掉之后，与源文本相关的 artifact 元数据必须按**新源**重算。
        ///
        /// `BuildGenericMaterial` 是在换源**之前**建元数据的，所以它留下的
        /// `generated_source_digest` 描述的是那份根本没被编译的生成物。缓存装载
        /// 侧虽然不拿这个字段判命中（stage 命中看每阶段自己的 source digest），
        /// 但它会被写进 program metadata，并被缓存审计与回归断言当作「实际编译
        /// 源」的摘要使用——记错就是埋一个事后永远查不出来的偏差。
        bool RefreshProgramArtifactMetadata(
            const contract::PhysicalDeviceProfileLite *profile,
            ShaderBuildContext &ctx)
        {
            if (!ctx.HasProgramLink())
                return true;

            ShaderProgramArtifactMetadata metadata{};
            if (!BuildShaderProgramArtifactMetadata(profile, ctx, metadata))
            {
                DomainBuildFailed(u8"[DomainBackend] 换源后重建 program artifact 元数据失败");
                return false;
            }

            ctx.SetProgramArtifactMetadata(metadata);
            return true;
        }
    }//namespace

    // ═══════════════════════════════════════════════════════════════════════
    // 后端本体
    // ═══════════════════════════════════════════════════════════════════════

    namespace
    {
        class DomainShaderBuildBackend final : public ShaderProgramBuildBackend
        {
        public:

            ShaderBuildContext *CreateBuildContext(
                const contract::PhysicalDeviceProfileLite *profile,
                const MaterialDefinition &definition,
                const MaterialDefinitionBuildRequest &request) override
            {
                DomainShaderTemplate resolved_template;

                g_last_domain_build_error.Clear();

                // ── 1. 网格阶段/链接规格/元数据仍走生成器 ────────────────────
                //
                // 片段阶段要整篇换源，所以必须 `defer_finalize`：否则
                // BuildGenericMaterial 会把生成出来的片段 GLSL 编成 SPV，
                // 那是白烧一遍（而且是要被丢掉的代码）。
                MaterialDefinitionBuildRequest deferred_request = request;
                deferred_request.defer_finalize = true;

                ShaderBuildContext *ctx = BuildGenericMaterial(
                    profile, deferred_request, definition, nullptr);
                if (!ctx)
                {
                    DomainBuildFailed(u8"[DomainBackend] 网格阶段构建失败：材质 %s（%s）",
                        definition.definition_id.c_str(),
                        GetLastBuildGenericMaterialError());
                    return nullptr;
                }

                const bool ok = ApplyDomainMasterShader(
                    definition, request, *ctx, resolved_template);
                if (!ok)
                {
                    delete ctx;
                    return nullptr;
                }

                // 元数据重建必须在换源之后、`FinalizeBuildContext` 之前——
                // 落盘时写的就是这里的结果。
                if (!RefreshProgramArtifactMetadata(profile, *ctx))
                {
                    delete ctx;
                    return nullptr;
                }

                (void)resolved_template;
                return ctx;
            }

            bool FinalizeBuildContext(ShaderBuildContext *ctx) override
            {
                return FinalizeShaderBuildContext(ctx);
            }

        private:

            bool ApplyDomainMasterShader(
                const MaterialDefinition &definition,
                const MaterialDefinitionBuildRequest &request,
                ShaderBuildContext &ctx,
                DomainShaderTemplate &out_template)
            {
                const MaterialDomain domain = definition.domain;
                const ShaderProgramPurpose purpose = request.shader_program_purpose;

                // ── 2. 覆盖判定决定深度 pass 用哪份主着色器 ──────────────────
                //
                // 前向 pass 不需要区分：是否写出 alpha 由 options 的
                // `HGL_ALPHA_OUTPUT` 表达，一份文件足够。深度 pass 则必须分开——
                // 不需要覆盖判定时片段着色器是**彻头彻尾的空实现**（省掉整个
                // 纹理采样链），这是 Phase 1 实测的形状，不是偷懒。
                const ResolvedMaterialRenderState render_state =
                    ResolveMaterialRenderState(definition, request.recipe);
                const bool masked_coverage =
                    render_state.alpha_test || render_state.dither;

                if (!ResolveDomainShaderTemplate(
                        domain, purpose, masked_coverage, out_template))
                {
                    DomainBuildFailed(
                        u8"[DomainBackend] 领域 %d 没有对应的固定主着色器"
                        u8"（purpose=%d masked=%d，材质 %s）",
                        static_cast<int>(domain),
                        static_cast<int>(purpose),
                        masked_coverage ? 1 : 0,
                        definition.definition_id.c_str());
                    return false;
                }

                // ── 3. 生成 options ─────────────────────────────────────────
                AnsiString options_text;
                if (!BuildDomainOptions(
                        definition, request, domain, purpose, options_text))
                    return false;

                // ── 4. options 落盘（缓存目录，不写源码树）──────────────────
                OSString options_root;
                if (!WriteOptionsToCache(
                        options_text, domain, purpose, options_root))
                    return false;

                // ── 5. 主着色器替换片段源并立刻编译 ────────────────────────
                AnsiString source;
                if (!LoadDomainMasterShader(out_template, source))
                    return false;

                return CompileDomainMasterFragment(
                    source, options_text, options_root, domain, purpose, ctx);
            }
        };//class DomainShaderBuildBackend

        DomainShaderBuildBackend g_domain_shader_build_backend;
    }//namespace

    // ═══════════════════════════════════════════════════════════════════════
    // 安装 / 选择
    // ═══════════════════════════════════════════════════════════════════════

    const char *GetLastDomainShaderBuildBackendError()
    {
        const char *text = g_last_domain_build_error.c_str();
        return text ? text : "";
    }

    bool EmitDomainOptionsForDefinition(
        const MaterialDefinition &definition,
        const MaterialDefinitionBuildRequest &request,
        const MaterialDomain domain,
        const ShaderProgramPurpose purpose,
        AnsiString &out_text)
    {
        return BuildDomainOptions(definition, request, domain, purpose, out_text);
    }

    bool IsDomainShaderBuildBackendRequested()
    {
        const char *selector = getenv(kBackendSelectorEnvironmentVariable);
        if (!selector || !selector[0])
            return false;

        return std::strcmp(selector, "domain") == 0;
    }

    void InstallDomainShaderBuildBackend()
    {
        SetShaderProgramBuildBackend(&g_domain_shader_build_backend);
        InstallShaderCreateInfoSPVReleaser();
    }

    void UninstallDomainShaderBuildBackend()
    {
        UninstallShaderCreateInfoSPVReleaser();
        SetShaderProgramBuildBackend(nullptr);
    }
}//namespace hgl::graph::mtl
