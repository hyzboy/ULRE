#pragma once

#include <hgl/mtl/MaterialResolver.h>
#include <hgl/mtl/MaterialDomain.h>
#include <hgl/mtl/DomainEntrySource.h>
#include <hgl/mtl/DomainOptions.h>
#include <hgl/mtl/VkEnumText.h>
#include <hgl/mtl/SamplerPreset.h>
#include <hgl/mtl/ResolvedPassProfile.h>
#include <hgl/mtl/StageSPVCompiler.h>
#include <hgl/mtl/ShaderBuildContext.h>
#include <hgl/mtl/ShaderCreateInfo.h>
#include <hgl/mtl/ShaderLibraryPath.h>
#include <hgl/mtl/contract/ShaderGenProfileTargetVersion.h>
#include <hgl/mtl/ShaderArtifactStore.h>
#include <hgl/mtl/ShaderProgramArtifactBuilder.h>
#include <hgl/mtl/ShaderProgramManifest.h>
#include <hgl/mtl/ShaderComputeProgramManifest.h>
#include <hgl/asset/generated/ShaderCacheSerialize.gen.h>
#include <hgl/serialize/FlatCodec.h>
#include <hgl/mtl/ShaderProgramManifestKey.h>
#include <hgl/mtl/ShaderProgramManifestBackend.h>
#include <hgl/mtl/ShaderProgramBuildBackend.h>
#include <hgl/mtl/CanonicalShaderContract.h>
#include <hgl/mtl/ShaderSemanticRegistry.h>
#include <hgl/mtl/InterStageVaryingOptions.h>
#include <hgl/mtl/GlslModule.h>
#include <hgl/mtl/GlslModuleFile.h>
#include <hgl/mtl/GlslModuleRegistry.h>
#include <hgl/mtl/GlslModuleMetadata.h>
#include <hgl/mtl/ShaderCodeResourceManifest.h>
#include <hgl/mtl/ScenePassProfileResolver.h>
#include <hgl/ShaderCompilerAPI.h>
#include <hgl/common/RenderOptions.h>
#include <hgl/graph/geo/GeometryVertexFormat.h>
#include <hgl/graph/asset/PrimitiveAsset.h>
#include <hgl/graph/module/SSBOBufferRegistry.h>
#include <hgl/log/Log.h>
#include <hgl/filesystem/FileSystem.h>
#include <hgl/filesystem/Path.h>
#include <hgl/time/Time.h>
#include <hgl/ShaderCompilerAPI.h>
#include "ShaderCompiler/ShaderReflectionData.h"
#include "SPVParseFlat.h"
#include <fstream>
#include <filesystem>
#include "ShaderProgramBuild/builder/MaterialProgramAssembler.h"
#include <hgl/mtl/MeshShaderLimits.h>
#include <hgl/graph/font/CharQuadConfig.h>

// 契约枚举 ↔ 遗留 C++ 常量的编译期对拍（由 ShaderConstGen 生成）。
// 放在门禁 TU 里：这些 static_assert 需要 GPUCullSystem / EntityGPU / LightRow 等
// 重头文件，普通编译单元不该为此付代价。
#include <hgl/shader/generated/ShaderConstantsParity.gen.h>
#include "VertexBuilderCommon.h"
#include "StageBuildContextTest.h"
#include <hgl/mtl/MaterialVertexVaryingConfig.h>
#include <hgl/mtl/MeshShaderMode.h>
#include <hgl/mtl/ShaderProgramBuildBackend.h>
#include "ShaderProgramBuild/compile/ShaderProgramBuildBackendInstall.h"
#include <hgl/mtl/MaterialRecipeFile.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <hgl/type/StdString.h>

using namespace hgl::graph;
using namespace hgl::graph::mtl;

// 门禁公共支持层（阶段 5 拆分）：原单文件门控的工具函数、夹具表与 case 声明。
// case 实现按回归组分布在 GateInterface/GateMaterialization/GatePipeline/GateCache/GateGlsl.cpp；
// 本头实体均为内部链接，各 TU 各自复制一份。
namespace gate
{
    /// 用**领域后端**构建一次并收尾（跑完把后端槽位复原）。
    ///
    /// 门禁不依赖任何生成器入口：生成器已物理删除，而「编译出的二进制长什么样」
    /// 这类判据本来就只该问后端。领域后端内部强制 `defer_finalize`（SPV 编译要等
    /// include 根设对），因此调用方必须显式收尾 —— 收尾之后 `HasProgramLink()` 才成立。
    ///
    /// 失败时 `error` 取领域后端/网格构建的报错；两者都没有时给一句兜底，
    /// 免得诊断变成空行（GLogError 不进 stdout，门禁里看不见）。
    static std::unique_ptr<ShaderBuildContext> BuildViaDomainBackend(
        const ResolvedMaterial &definition,
        const MaterialBuildRequest &request,
        std::string &error)
    {
        InstallDomainShaderBuildBackend();
        std::unique_ptr<ShaderBuildContext> ctx;
        ShaderProgramBuildBackend *backend = GetShaderProgramBuildBackend();
        if (!backend)
            error = "domain backend slot empty";
        else
        {
            ctx.reset(backend->CreateBuildContext(nullptr, definition, request));
            if (!ctx)
            {
                const char *domain_error = GetLastDomainShaderBuildBackendError();
                const hgl::AnsiString &mesh_error = GetLastMaterialProgramAssemblyError();
                if (domain_error && domain_error[0])
                    error = domain_error;
                else if (!mesh_error.IsEmpty())
                    error = mesh_error.c_str();
                else
                    error = "unknown error";
            }
            else if (!backend->FinalizeBuildContext(ctx.get()))
            {
                error = "finalize failed";
                ctx.reset();
            }
        }
        InstallDomainShaderBuildBackend();
        return ctx;
    }

    static hgl::uint64 StableID(const char *text)
    {
        if (!(text && text[0]))
            return 0;

        hgl::hash::FNV1aHasher64 h;
        h << text;
        return h;
    }

    // mesh↔fragment stage 接口一致性校验（原生产头 ShaderStageBuildContext.h，
    // 生产路径从不调用——生成器不校验接口，错位由 glslang 编译失败兜底；
    // 仅回归门使用，故移入此处。mesh 是唯一顶点路径，参数命名 mesh）
    static bool HasCompatibleStageInterface(
        const ShaderStageBuildContext &mesh,
        const ShaderStageBuildContext &fragment) noexcept
    {
        for (int i = 0; i < fragment.inputs.GetCount(); ++i)
        {
            const ShaderStageInterfaceVariable &fragment_input = fragment.inputs[i];
            bool found = false;

            for (int j = 0; j < mesh.outputs.GetCount(); ++j)
            {
                const ShaderStageInterfaceVariable &mesh_output = mesh.outputs[j];
                if (mesh_output.location == fragment_input.location
                 && mesh_output.value_type == fragment_input.value_type
                 && mesh_output.flags == fragment_input.flags)
                {
                    found = true;
                    break;
                }
            }

            if (!found)
                return false;
        }

        return true;
    }

    static bool IsKnownRegressionGroup(const char *group)
    {
        if (!group)
            return false;

        return std::strcmp(group, "all") == 0
            || std::strcmp(group, "glsl") == 0
            || std::strcmp(group, "interface") == 0
            || std::strcmp(group, "descriptor") == 0
            || std::strcmp(group, "cache") == 0
            || std::strcmp(group, "materialization") == 0
            || std::strcmp(group, "pipeline") == 0
            || std::strcmp(group, "reflection") == 0
            || std::strcmp(group, "module-invariants") == 0;
    }

    static bool IsRegressionGroupSelected(
        const char *selected_group,
        const char *case_group)
    {
        return std::strcmp(selected_group, "all") == 0
            || std::strcmp(selected_group, case_group) == 0;
    }

    struct GateResult
    {
        std::string name;
        bool passed = false;
        std::vector<std::string> diagnostics;
    };

    /// `ShaderLibrary` 下 `.glsl` 代码模块的期望总数。
    ///
    /// 刻意写成「门自己持有的常量」而不是去问被测代码：这条断言的价值就是
    /// 在模块被误删/误加时硬失败，跟着实现走就等于没测。历史上每次增删模块
    /// 都在这里改一行并留下理由，变更记录见 `RunGlslModuleRegistryCase`。
    /// 89 = 88 + `ambient_light_ibl_oct`（八面体展开版 IBL 天光 provider，
    /// 与 `ambient_light_ibl` 同构、只换纹理寻址方式）。
    /// 90 = 89 + `ambient_light_sh`（SH9 球谐天光 provider，唯一不读贴图的间接光实现）。
    constexpr int kGlslModuleCount = 97;

    // 仓库根路径（由 CMake 注入，避免硬编码盘符）
    static std::string RepoRootPath(const char *suffix)
    {
        return std::string(ULRE_REPO_ROOT) + "/" + suffix;
    }

    static hgl::OSString RepoRootOSPath(const char *suffix)
    {
        return hgl::ToOSString(RepoRootPath(suffix));
    }

    static std::string ReadFileText(const std::string &path);


    /// 递归读一棵目录下的**全部**文本文件。
    ///
    /// 固定库是唯一真源：生成器删除之后，凡是「生成器该产出什么」的判据都只能
    /// 改成「库里实际是什么」。按扩展名过滤会漏掉 `.tmpl`，所以这里只跳过目录。
    static std::vector<std::pair<std::string, std::string>> LoadTextTree(
        const std::filesystem::path &root)
    {
        std::vector<std::pair<std::string, std::string>> files;
        std::error_code ec;
        if (!std::filesystem::is_directory(root, ec))
            return files;

        for (const auto &entry :
             std::filesystem::recursive_directory_iterator(root, ec))
        {
            if (ec)
                break;
            if (!entry.is_regular_file())
                continue;
            std::string text = ReadFileText(entry.path().string());
            if (text.empty())
                continue;
            files.emplace_back(
                entry.path().lexically_relative(root).string() + "|"
                    + entry.path().string(),
                std::move(text));
        }
        std::sort(
            files.begin(), files.end(),
            [](const auto &a, const auto &b) { return a.first < b.first; });
        return files;
    }

    /// 固定网格主着色器的根目录。
    ///
    /// 只有 `topology/`：三种几何模式（VertexPassthrough / LineQuad /
    /// CharQuad）的全部形状 master 都在这里，形状名即文件名。`mesh/fixed/`
    /// 那套哈希命名（`<shape_key>.mesh`）已随生成器残留一起删除——它是
    /// 「按顶点格式算形状键再查表」的遗物，形状数随格式表组合爆炸。
    static std::vector<std::pair<std::string, std::string>>
    LoadFixedMeshMasters()
    {
        std::vector<std::pair<std::string, std::string>> masters;
        for (const char *suffix : {"ShaderLibrary/topology"})
        {
            const std::filesystem::path root(RepoRootPath(suffix));
            const std::string prefix = root.filename().string() + "/";
            for (auto &entry : LoadTextTree(root))
            {
                entry.first = prefix + entry.first;
                masters.emplace_back(std::move(entry));
            }
        }
        std::sort(
            masters.begin(), masters.end(),
            [](const auto &a, const auto &b) { return a.first < b.first; });
        return masters;
    }

    /// 相对路径（诊断里用的短名；key 形如 `相对路径|绝对路径`）。
    static std::string TreeEntryName(
        const std::pair<std::string, std::string> &entry)
    {
        const size_t sep = entry.first.find('|');
        return sep == std::string::npos
            ? entry.first
            : entry.first.substr(0, sep);
    }

    /// 诊断限流：固定库全量扫描可能一次抓出几十条同类问题，
    /// 刷屏反而看不见第一个根因。
    static void AddCappedDiagnostic(
        GateResult &result, const std::string &message)
    {
        if (result.diagnostics.size() < 24)
            result.diagnostics.emplace_back(message);
    }

    static bool StartsWith(const std::string &text, size_t offset,
                           const char *prefix)
    {
        const size_t length = std::strlen(prefix);
        return text.size() >= offset + length
            && text.compare(offset, length, prefix) == 0;
    }

    /// 跳过空白（GLSL 里的声明要容忍任意排版）。
    static void SkipBlanks(const std::string &text, size_t &offset)
    {
        while (offset < text.size()
            && (text[offset] == ' ' || text[offset] == '\t'
                || text[offset] == '\r' || text[offset] == '\n'))
            ++offset;
    }

    /// 读一个标识符（字母/数字/下划线）。
    static std::string ReadIdentifier(const std::string &text, size_t &offset)
    {
        const size_t begin = offset;
        while (offset < text.size()
            && (std::isalnum(static_cast<unsigned char>(text[offset]))
                || text[offset] == '_'))
            ++offset;
        return text.substr(begin, offset - begin);
    }

    /// 去掉 `//` 行注释与 `/* */` 块注释（保留换行，便于按偏移取原文本片段）。
    ///
    /// 形状 master 的「不得含预处理分支」判据必须走这里：按原文本直接查子串会把
    /// **注释里的说明文字**也算成违规——`// 本文件刻意不留 #ifdef` 这种正当注释
    /// 会被判死。判据的对象必须是**代码**，不是注释。
    static std::string StripGlslComments(const std::string &src)
    {
        std::string out;
        out.reserve(src.size());

        for (size_t i = 0; i < src.size();)
        {
            if (src[i] == '/' && i + 1 < src.size() && src[i + 1] == '/')
            {
                while (i < src.size() && src[i] != '\n')
                    ++i;
            }
            else if (src[i] == '/' && i + 1 < src.size() && src[i + 1] == '*')
            {
                i += 2;
                while (i + 1 < src.size() && !(src[i] == '*' && src[i + 1] == '/'))
                {
                    if (src[i] == '\n')
                        out += '\n';
                    ++i;
                }
                i = (i + 1 < src.size()) ? i + 2 : src.size();
            }
            else
            {
                out += src[i];
                ++i;
            }
        }

        return out;
    }

    /// 取出以 `prefix` 开头的一整行（去掉行尾 CR/空白与行首缩进）。
    ///
    /// 用于把「库里写的宏定义原文」当成期望值：测试不再自己复制一份字符串，
    /// 否则库文本改了而测试没改，两边一起漂移也照样绿。
    static std::string ExtractDefineLine(const std::string &src, const char *prefix)
    {
        const size_t at = src.find(prefix);
        if (at == std::string::npos)
            return std::string();

        const size_t eol = src.find('\n', at);
        std::string line = src.substr(at, (eol == std::string::npos ? src.size() : eol) - at);

        while (!line.empty()
               && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t'))
            line.pop_back();

        return line;
    }

    /// `text` 里是否出现独立记号 `word`（前后不是标识符字符）。
    ///
    /// 声明审计用它是为了「容忍排版、不容忍改写」：`in  uint` 与 `in uint` 是同一
    /// 件事，而 `uint` 与 `uvec2` 不是。
    static bool ContainsWord(const std::string &text, const char *word)
    {
        const size_t length = std::strlen(word);
        if (length == 0)
            return false;

        for (size_t at = text.find(word); at != std::string::npos;
             at = text.find(word, at + 1))
        {
            const bool left_ok = at == 0
                || !(std::isalnum(static_cast<unsigned char>(text[at - 1]))
                     || text[at - 1] == '_');
            const size_t after = at + length;
            const bool right_ok = after >= text.size()
                || !(std::isalnum(static_cast<unsigned char>(text[after]))
                     || text[after] == '_');
            if (left_ok && right_ok)
                return true;
        }
        return false;
    }

    /// 取 `#if defined(<macro>)` … `#endif` 之间的代码（不含两端的指令行）。
    /// 找不到返回空串——调用方据此区分「这一段没声明」。
    static std::string ExtractConditionalBlock(
        const std::string &text, const std::string &macro)
    {
        const std::string open = "#if defined(" + macro + ")";
        const size_t begin = text.find(open);
        if (begin == std::string::npos)
            return std::string();

        const size_t body = begin + open.size();
        const size_t end = text.find("#endif", body);
        if (end == std::string::npos)
            return std::string();

        return text.substr(body, end - body);
    }

    /// `symbol` 在 `text` 里是否被写过（`symbol[...] = …`，等号必须在分号之前）。
    static bool IsVaryingWritten(
        const std::string &text, const std::string &symbol)
    {
        const std::string probe = symbol + "[";
        size_t scan = 0;
        while ((scan = text.find(probe, scan)) != std::string::npos)
        {
            const size_t bracket = text.find(']', scan);
            if (bracket == std::string::npos)
                return false;

            const size_t assignment = text.find('=', bracket);
            const size_t terminator = text.find(';', bracket);
            if (assignment != std::string::npos
                && (terminator == std::string::npos || assignment < terminator))
                return true;

            scan = bracket + 1;
        }
        return false;
    }

    /// 统一阶段间 varying 文件的**自洽审计**。
    ///
    /// 迁移前这条判据必须对 14 个 `.mesh` 各跑一遍：每个 varying 组合各有一份
    /// master，「声明对不对」实际上是在检查「文件名有没有被手抄错」。接口集中到
    /// `module/stage/interstage_varying.glsl` 之后，同一批不变量只需检查**一份文件**，
    /// 而且检查对象从「某个组合碰巧写了什么」变成「注册表里每条**已接线**语义
    /// 是否两端成对、location / 类型 / 插值 / 维数是否与注册表与调度契约一致」。
    ///
    /// 「哪些语义已接线」不在这里维护第二份清单：直接拿真实发射器
    /// （EmitInterStageVaryingOptions）对单语义 mask 试发一次，发得出来就算接线。
    /// 于是「发射器认为能发」与「GLSL 里真有对应声明」之间不存在
    /// 「两边各记一份、漂移了也照样绿」的缝隙。
    static void AuditUnifiedInterStageVarying(GateResult &result)
    {
        const std::string source = ReadFileText(
            RepoRootPath("ShaderLibrary/module/stage/interstage_varying.glsl"));
        if (source.empty())
        {
            result.diagnostics.emplace_back(
                "module/stage/interstage_varying.glsl is missing or empty");
            return;
        }

        const std::string writer = ReadFileText(
            RepoRootPath("ShaderLibrary/module/stage/interstage_varying_writer.glsl"));
        if (writer.empty())
        {
            result.diagnostics.emplace_back(
                "module/stage/interstage_varying_writer.glsl is missing or empty");
            return;
        }

        const std::string text = StripGlslComments(source);
        std::string writer_text = StripGlslComments(writer);

        // 写出方不只有标准写出函数：CharQuad 的 `fragStyleID` 由主着色器自己的
        // 钩子写（`WriteCharQuadPrimitiveOutputs`），线段的写出钩子也在主着色器里。
        // 只扫 `interstage_varying_writer.glsl` 会把「声明了、由别的钩子写」误判成
        // 「声明了却没写」。所以判据是「语料里某处真的写了」。
        for (auto &master : LoadFixedMeshMasters())
        {
            writer_text += "\n";
            writer_text += StripGlslComments(master.second);
        }

        // 两段的分界必须是「网格段在前、片段段在后」：两段读的是同一批
        // `HGL_VARYING_*` 与 location 宏，各自声明接口的一端；顺序反了等于把
        // 两套接口拼在一起，本审计也按「前段必须是 out、后段必须是 in」定位。
        const size_t mesh_marker =
            text.find("#if defined(HGL_MESH_OUTPUT_VARYINGS)");
        const size_t frag_marker =
            text.find("#if defined(HGL_FRAGMENT_INPUT_VARYINGS)");
        if (mesh_marker == std::string::npos
            || frag_marker == std::string::npos
            || mesh_marker > frag_marker)
        {
            result.diagnostics.emplace_back(
                "module/stage/interstage_varying.glsl 必须依次含"
                " 网格段 (#if defined(HGL_MESH_OUTPUT_VARYINGS)) 与"
                " 片段段 (#if defined(HGL_FRAGMENT_INPUT_VARYINGS))");
            return;
        }

        const std::string mesh_half = text.substr(0, frag_marker);
        const std::string frag_half = text.substr(frag_marker);

        uint32_t wired = 0;
        for (uint32_t index = 1; index < 32; ++index)
        {
            const InterStageSemantic semantic =
                static_cast<InterStageSemantic>(index);
            const InterStageSemanticInfo *info =
                GetInterStageSemanticInfo(semantic);
            if (!info || !info->shader_symbol || !info->location_macro)
                continue;

            // 「已接线」的判据是发射器本身：只有这一条语义也发得出来，才算接通。
            hgl::AnsiString emitted;
            if (!EmitInterStageVaryingOptions(
                    GetInterStageSemanticMask(semantic), emitted))
                continue;
            ++wired;

            const std::string symbol(info->shader_symbol);
            static const char *const kLocationPrefix = "HGL_LOCATION_";
            const std::string varying_macro =
                std::string("HGL_VARYING_")
                + (std::string(info->location_macro)
                   .substr(std::strlen(kLocationPrefix)));

            if (std::string(emitted.c_str()).find(varying_macro)
                    == std::string::npos)
            {
                AddCappedDiagnostic(
                    result, std::string("interstage varying 发射器未给出 ")
                        + varying_macro + "（开关名与 GLSL 文件不一致）");
            }

            const std::string mesh_block =
                ExtractConditionalBlock(mesh_half, varying_macro);
            const std::string frag_block =
                ExtractConditionalBlock(frag_half, varying_macro);
            if (mesh_block.empty() && frag_block.empty())
            {
                AddCappedDiagnostic(
                    result, std::string("module/stage/interstage_varying.glsl: ")
                        + varying_macro + " 已接线，两段却都没有声明 "
                        + symbol);
                continue;
            }
            if (mesh_block.empty() || frag_block.empty())
            {
                AddCappedDiagnostic(
                    result, std::string("module/stage/interstage_varying.glsl: ")
                        + varying_macro + " 只在"
                        + (mesh_block.empty() ? "片段段" : "网格段")
                        + "声明了 " + symbol + "，另一端缺失（接口必须成对）");
                continue;
            }

            const char *expected_type = GetGLSLTypeName(
                info->value_shape.scalar_type,
                info->value_shape.component_count);
            if (!expected_type)
            {
                AddCappedDiagnostic(
                    result, std::string("ShaderSemanticRegistry 的 ") + symbol
                        + " 没有可用的 GLSL 类型");
                continue;
            }

            const bool flat =
                info->interpolation == InterStageInterpolation::Flat;

            // ── 网格段：`out` 数组，维数取自调度宏 ──
            //
            // 维数写错不会编译失败（GLSL 数组越界是未定义行为），所以这条必须由
            // 门禁守住：逐顶点语义按 max_vertices、Flat 按 max_primitives。
            const std::string expected_dimension =
                std::string("[")
                + (flat ? "ULRE_MESH_MAX_PRIMITIVES" : "ULRE_MESH_MAX_VERTS")
                + "]";
            if (!ContainsWord(mesh_block, info->location_macro)
                || !ContainsWord(mesh_block, "out")
                || !ContainsWord(mesh_block, expected_type)
                || !ContainsWord(mesh_block, info->shader_symbol)
                || mesh_block.find(expected_dimension) == std::string::npos
                || ((mesh_block.find("perprimitiveEXT") != std::string::npos)
                    != flat))
            {
                AddCappedDiagnostic(
                    result, std::string("module/stage/interstage_varying.glsl 网格段: ")
                        + varying_macro + " 的声明必须是 `layout(location="
                        + info->location_macro + ") "
                        + (flat ? "perprimitiveEXT " : "") + "out "
                        + expected_type + " " + symbol + expected_dimension
                        + ";`");
            }

            // ── 片段段：`in` 标量，Flat 带 `flat perprimitiveEXT` ──
            if (!ContainsWord(frag_block, info->location_macro)
                || !ContainsWord(frag_block, "in")
                || !ContainsWord(frag_block, expected_type)
                || !ContainsWord(frag_block, info->shader_symbol)
                || frag_block.find('[') != std::string::npos
                || ((frag_block.find("flat perprimitiveEXT") != std::string::npos)
                    != flat))
            {
                AddCappedDiagnostic(
                    result, std::string("module/stage/interstage_varying.glsl 片段段: ")
                        + varying_macro + " 的声明必须是 `layout(location="
                        + info->location_macro + ") "
                        + (flat ? "flat perprimitiveEXT " : "") + "in "
                        + expected_type + " " + symbol + ";`");
            }

            // ── 网格段声明了就必须真的写出去 ──
            if (!IsVaryingWritten(writer_text, symbol))
            {
                AddCappedDiagnostic(
                    result, std::string("module/stage/interstage_varying_writer.glsl: ")
                        + symbol + " 声明了却从未写入");
            }
        }

        if (wired == 0)
        {
            result.diagnostics.emplace_back(
                "interstage varying 发射器放行了零条语义"
                "（统一接口文件因此没有任何 varying）");
        }

        // 反向：文件里出现的每个开关都必须是发射器真会发的，否则就是死声明——
        // 它永远不会被打开，也永远不会被发现写错了。
        for (size_t at = text.find("HGL_VARYING_"); at != std::string::npos;
             at = text.find("HGL_VARYING_", at + 1))
        {
            size_t end = at + std::strlen("HGL_VARYING_");
            while (end < text.size()
                && (std::isalnum(static_cast<unsigned char>(text[end]))
                    || text[end] == '_'))
                ++end;
            const std::string macro = text.substr(at, end - at);

            bool emittable = false;
            for (uint32_t index = 1; index < 32 && !emittable; ++index)
            {
                const InterStageSemanticInfo *info =
                    GetInterStageSemanticInfo(
                        static_cast<InterStageSemantic>(index));
                if (!info || !info->location_macro)
                    continue;
                static const char *const kLocationPrefix = "HGL_LOCATION_";
                if (macro != std::string("HGL_VARYING_")
                        + (std::string(info->location_macro)
                           .substr(std::strlen(kLocationPrefix))))
                    continue;

                hgl::AnsiString probe;
                emittable = EmitInterStageVaryingOptions(
                    GetInterStageSemanticMask(
                        static_cast<InterStageSemantic>(index)),
                    probe);
            }

            if (!emittable)
            {
                AddCappedDiagnostic(
                    result, "module/stage/interstage_varying.glsl 含死开关 "
                        + macro + "（发射器永远不会定义它）");
            }
        }

        // ── 片段 stage 的键必须吃阶段间接口开关 ──
        //
        // 把声明搬进统一文件，等于把 varying 集合从 `.frag` 文本里**拿走**：
        // 迁移前片段文本随 `HGL_MATERIAL_*` 这类片段 options 变，迁移后它只随
        // `generated/common_options.glsl` 的 `HGL_VARYING_*` 变。于是片段 stage
        // 的 `compile_inputs` 若不含这份摘要，就退化回「只看文本」，同 domain +
        // 同 fragment options + 不同 varying 集合会撞键 —— 第二个材质直接复用
        // 第一个的 SPV，管线接口对不上。
        const std::string assembler = ReadFileText(
            RepoRootPath(
                "src/ShaderProgramBuild/builder/MaterialProgramAssembler.cpp"));
        if (assembler.empty())
        {
            result.diagnostics.emplace_back(
                "MaterialProgramAssembler.cpp is missing or empty");
            return;
        }

        const size_t fragment_hasher =
            assembler.find("fragment_compile_inputs_hasher");
        if (fragment_hasher == std::string::npos)
        {
            result.diagnostics.emplace_back(
                "MaterialProgramAssembler.cpp 不再含 fragment_compile_inputs_hasher"
                "（片段 stage 键的构成被改写了，本审计需要同步）");
            return;
        }

        const size_t fragment_hash_end = assembler.find(
            "const uint64 fragment_compile_inputs_hash", fragment_hasher + 1);
        const std::string fragment_key_inputs =
            assembler.substr(
                fragment_hasher,
                fragment_hash_end == std::string::npos
                    ? std::string::npos
                    : fragment_hash_end - fragment_hasher);
        if (fragment_key_inputs.find("interstage_varying_options_digest")
            == std::string::npos)
        {
            result.diagnostics.emplace_back(
                "MaterialProgramAssembler.cpp: 片段 compile_inputs 未折入 "
                "interstage_varying_options_digest —— 统一文件之后片段文本不再"
                "随 varying 集合变化，同 domain 同 fragment options 的两份材质"
                "会撞键并复用错误的 SPV");
        }
    }

    /// 固定库网格阶段主着色器的**结构审计**。
    ///
    /// 这段判据原先住在 `meshgen/MeshModeDescriptor.h` 的
    /// `resolve_stage1_input` / `resolve_stage2_mapping` / `resolve_stage3_projection`
    /// 与 `MeshShaderVaryingGen.h` 的写规则表里——那是「生成器打算产出什么」。
    /// 生成器删除之后，同一批不变量必须改成对**库里实际文本**的直接断言。
    ///
    /// **阶段间 varying 的声明审计已经不在这里**：接口集中到
    /// `module/stage/interstage_varying.glsl` 之后，同一批不变量只剩一份检查对象，
    /// 见 AuditUnifiedInterStageVarying()。本函数现在只看主着色器自身的结构：
    ///
    /// ① 拓扑契约 `layout(triangles|lines, max_vertices = N, max_primitives = M)` 是字面量
    ///    （与 varying 数组维数宏、CPU 派发是同一份事实）；② 每份形状 master 都走
    ///    统一 varying 文件，且不得再自己 `layout(location=...)`（否则又出现第二个
    ///    真源）；③ Stage2/Stage3 只能来自 `generated/mesh_options.glsl`；④ 形状
    ///    master 不得含预处理分支，唯一例外是 `#if defined(HGL_VARYING_*)` ——
    ///    写 varying 的钩子必须读声明用的同一批开关。
    static void AuditFixedMeshMasters(GateResult &result)
    {
        const auto masters = LoadFixedMeshMasters();
        if (masters.empty())
        {
            result.diagnostics.emplace_back(
                "fixed mesh master directories are missing or empty");
            return;
        }

        uint32_t char_quad_files = 0;
        for (const auto &file : masters)
        {
            const std::string name = TreeEntryName(file);
            // 判据一律针对**代码**：注释里的说明文字不算违规（见 StripGlslComments）。
            const std::string text = StripGlslComments(file.second);

            // ── ② 接口声明的唯一去处 ──
            //
            // 曾经每个 varying 组合各一份 master，文件名就是接口（`linequad_256_
            // dataid_lum.mesh`），于是「材质要传什么」被物化进磁盘、并与片段侧手抄的
            // `layout(in)` 只能靠人对齐。集中到统一文件后，master 对接口一无所知；
            // 这里断言这个形态不再被回退，并顺带确认它确实处在网格段（否则统一文件
            // 会 `#error`，但报错点在 include 里，不如在这里指出文件名）。
            if (text.find("#include \"module/stage/interstage_varying.glsl\"")
                == std::string::npos)
            {
                AddCappedDiagnostic(
                    result, name + ": 形状 master 必须 include"
                                  " module/stage/interstage_varying.glsl（接口声明"
                                  "只能有这一处真源）");
            }
            if (text.find("HGL_MESH_OUTPUT_VARYINGS") == std::string::npos)
            {
                AddCappedDiagnostic(
                    result, name + ": 形状 master 必须在 include 统一文件前"
                                  " 定义 HGL_MESH_OUTPUT_VARYINGS");
            }
            // 本文件再写一条 `layout(location=` 就等于把「接口是什么」拆回两份，
            // 且两端的成对性再也无法由一处保证。
            if (text.find("layout(location=") != std::string::npos)
            {
                AddCappedDiagnostic(
                    result, name + ": 使用统一 varying 文件时不得再自己声明"
                                  " layout(location=...) 阶段间 varying");
            }

            // 形状 master 只允许形状层面的差异，任何 `#ifdef` 都是把选择权偷偷还给
            // 编译期分支——那正是生成器残留的形态。唯一合法的分支是 varying 写出
            // 钩子里的 `#if defined(HGL_VARYING_*)`：它读的正是声明用的那批开关，
            // 删掉它就会「声明了却不写」，留着别的开关则是把 vary 集合又搬回文件。
            // 扫描对象是已剥掉注释的代码文本（见 StripGlslComments）：文档里讨论
            // 这些关键字是正当的。
            if (text.find("#ifdef") != std::string::npos
                || text.find("#ifndef") != std::string::npos
                || text.find("#elif") != std::string::npos)
            {
                AddCappedDiagnostic(
                    result, name + ": 形状 master 不得含预处理分支"
                                  "（#ifdef / #ifndef / #elif）；varying 钩子"
                                  "只允许 `#if defined(HGL_VARYING_*)`");
            }
            else
            {
                static const char kAllowedBranch[] = "#if defined(HGL_VARYING_";
                for (size_t at = text.find("#if ");
                     at != std::string::npos;
                     at = text.find("#if ", at + 1))
                {
                    if (text.compare(at, std::strlen(kAllowedBranch),
                                     kAllowedBranch) != 0)
                    {
                        AddCappedDiagnostic(
                            result, name + ": 形状 master 的 `#if` 只允许"
                                          " `#if defined(HGL_VARYING_*)`"
                                          "（varying 钩子必须按声明的开关写）");
                        break;
                    }
                }
            }

            // ── 调度契约：max_vertices / max_primitives ──
            uint32_t max_vertices = 0;
            uint32_t max_primitives = 0;
            {
                // 两种合法发射拓扑：triangles（形状 master 与 LineQuad 的 quad 撑宽）
                // 与 lines（LineList 原生 1px 线，linelist_*.mesh）。
                size_t layout = text.find("layout(triangles, max_vertices =");
                if (layout == std::string::npos)
                    layout = text.find("layout(lines, max_vertices =");
                if (layout == std::string::npos)
                {
                    AddCappedDiagnostic(
                        result, name + ": 缺少 layout(triangles/lines, ...) 输出声明");
                    continue;
                }
                const size_t first =
                    text.find("max_vertices =", layout);
                const size_t second =
                    text.find("max_primitives =", first);
                if (first == std::string::npos || second == std::string::npos)
                {
                    AddCappedDiagnostic(
                        result, name + ": 输出声明缺少 max_vertices/max_primitives");
                    continue;
                }
                max_vertices = uint32_t(std::strtoul(
                    text.c_str() + first + std::strlen("max_vertices ="),
                    nullptr, 10));
                max_primitives = uint32_t(std::strtoul(
                    text.c_str() + second + std::strlen("max_primitives ="),
                    nullptr, 10));
                if (max_vertices == 0 || max_primitives == 0)
                {
                    AddCappedDiagnostic(
                        result, name + ": max_vertices/max_primitives 解析失败");
                    continue;
                }
            }

            // ── ④ Stage2/Stage3 组合 ──
            //
            // 形状 master 不自己挑 Stage2/Stage3——两者都来自 `generated/mesh_options.glsl`。
            // 所以判据分两支：带 options 的形状 master 必须正好 include 一次、且不得再
            // 直接引入 Stage2/Stage3（否则与 options 里的重复定义打架）；拓扑专用源
            // （CharQuad / LineQuad）仍自带模块，按原来的规则查。
            const bool char_quad =
                text.find("#include \"module/vertex/stream_text_char_quad.glsl\"")
                != std::string::npos;
            const size_t options_include =
                text.find("#include \"generated/mesh_options.glsl\"");
            if (options_include != std::string::npos)
            {
                if (text.find("#include \"generated/mesh_options.glsl\"",
                              options_include + 1) != std::string::npos)
                {
                    AddCappedDiagnostic(
                        result, name
                            + ": generated/mesh_options.glsl 只能 include 一次");
                }
                if (text.find("#include \"vertex/s2_") != std::string::npos
                    || text.find("#include \"vertex/s3_") != std::string::npos)
                {
                    AddCappedDiagnostic(
                        result, name
                            + ": 形状 master 的 Stage2/Stage3 必须来自"
                              " generated/mesh_options.glsl，不得直接 include");
                }
            }
            else if (char_quad)
            {
                ++char_quad_files;
                if (text.find("#include \"vertex/s2_") != std::string::npos)
                {
                    AddCappedDiagnostic(
                        result, name
                            + ": CharQuad 自带位置拼装，不应再引入 Stage2 模块");
                }
            }
            else
            {
                const size_t stage2 = text.find("#include \"vertex/s2_");
                const size_t stage3 = text.find("#include \"vertex/s3_");
                if (stage2 == std::string::npos || stage3 == std::string::npos)
                {
                    AddCappedDiagnostic(
                        result, name + ": 缺少 Stage2/Stage3 顶点变换模块");
                }
                else if (text.find("#include \"vertex/s2_", stage2 + 1)
                      != std::string::npos)
                {
                    AddCappedDiagnostic(
                        result, name + ": Stage2 模块只能有一个");
                }
                else if (stage3 < stage2)
                {
                    AddCappedDiagnostic(
                        result, name + ": Stage3 必须排在 Stage2 之后");
                }
            }
        }

        // 统一接口文件本身的自洽（成对性 / location / 类型 / 插值 / 维数 / 写出）。
        AuditUnifiedInterStageVarying(result);

        if (char_quad_files == 0)
            result.diagnostics.emplace_back(
                "fixed mesh master corpus lost its CharQuad master");
    }

    static bool CheckVertexEntries(const std::vector<SerializedVertexEntry> &actual,
                                   const std::vector<SerializedVertexEntry> &expected,
                                   std::string &diagnostic)
    {
        if (actual.size() != expected.size())
        {
            diagnostic = "vertex entry count mismatch";
            return false;
        }

        for (size_t i = 0; i < expected.size(); ++i)
        {
            if (actual[i].semantic != expected[i].semantic
             || actual[i].format != expected[i].format)
            {
                diagnostic = "vertex entry semantic/format mismatch at index "
                    + std::to_string(i);
                return false;
            }
        }

        return true;
    }

    /// 领域 → 合成材质定义。
    ///
    /// definition 中间层（`.definition.toml`）移除后，「按 ID 取定义」一律换成
    /// 「按领域合成定义」：契约来自 `MaterialDomainContract`，配方只负责覆盖。
    static bool BuildResolvedMaterialForDomain(const MaterialDomain domain,
                                           ResolvedMaterial &out_definition)
    {
        MaterialRecipe recipe{};
        recipe.domain = domain;
        return ResolveMaterialFromRecipe(recipe, out_definition);
    }




    /// 按 result id 读 `OpConstant` 的整型字面量。
    ///
    /// 词法：`word1 = 结果类型 id`，`word2 = 结果 id`，`word3.. = 字面量`。
    static bool FindSPVConstantU32(
        const ShaderFlatView &view,
        const uint32_t result_id,
        uint32_t &out_value)
    {
        constexpr uint32_t kOpConstant = 43u;

        for (uint32_t i = 5u; i < view.spv_word_count; )
        {
            const uint32_t word = view.spv_words[i];
            const uint32_t word_count = word >> 16;
            if (word_count == 0u)
                return false;

            if ((word & 0xFFFFu) == kOpConstant
             && word_count >= 4u
             && i + 3u < view.spv_word_count
             && view.spv_words[i + 2u] == result_id)
            {
                out_value = view.spv_words[i + 3u];
                return true;
            }

            i += word_count;
        }

        return false;
    }

    /// 读 `OpExecutionModeId`（opcode 331）的实参并解析成常量值。
    ///
    /// 新版本 glslang 把 mesh 的局部尺寸降级为 `OpExecutionModeId %main
    /// LocalSizeId %cx %cy %cz`（opcode 331 / 模式 38），实参是常量 id 而不是
    /// 字面量，因此必须再查一次 `OpConstant`。
    static bool FindSPVExecutionModeIdConstant(
        const ShaderFlatView &view,
        const uint32_t execution_mode,
        const uint32_t operand_index,
        uint32_t &out_value)
    {
        constexpr uint32_t kOpExecutionModeId = 331u;

        for (uint32_t i = 5u; i < view.spv_word_count; )
        {
            const uint32_t word = view.spv_words[i];
            const uint32_t word_count = word >> 16;
            if (word_count == 0u)
                return false;

            if ((word & 0xFFFFu) == kOpExecutionModeId
             && i + 3u < view.spv_word_count
             && view.spv_words[i + 2u] == execution_mode)
            {
                const uint32_t id_index = i + 3u + operand_index;
                if (id_index >= view.spv_word_count)
                    return false;
                return FindSPVConstantU32(
                    view, view.spv_words[id_index], out_value);
            }

            i += word_count;
        }

        return false;
    }

    /// 读 mesh 阶段的 `local_size_x`：先试字面量 `OpExecutionMode LocalSize`，
    /// 再退回 `OpExecutionModeId LocalSizeId`（较新 glslang 的默认产物）。
    static bool FindSPVLocalSizeX(const ShaderFlatView &view, uint32_t &out_value);

    /// 从已反射的 SPIR-V 里取 `OpExecutionMode` 的一个字面量。
    ///
    /// 词法：`word0 = (word_count << 16) | opcode`，`word1 = 目标 id`，
    /// `word2 = 执行模式枚举`，`word3.. = 字面量`。`OpExecutionMode` 的 opcode 是 16。
    static bool FindSPVExecutionModeLiteral(
        const ShaderFlatView &view,
        const uint32_t execution_mode,
        const uint32_t literal_index,
        uint32_t &out_value)
    {
        constexpr uint32_t kOpExecutionMode = 16u;

        for (uint32_t i = 5u; i < view.spv_word_count; )
        {
            const uint32_t word = view.spv_words[i];
            const uint32_t word_count = word >> 16;
            if (word_count == 0u)
                return false;

            if ((word & 0xFFFFu) == kOpExecutionMode
             && i + 3u < view.spv_word_count
             && view.spv_words[i + 2u] == execution_mode
             && i + 3u + literal_index < view.spv_word_count)
            {
                out_value = view.spv_words[i + 3u + literal_index];
                return true;
            }

            i += word_count;
        }

        return false;
    }

    static bool FindSPVLocalSizeX(const ShaderFlatView &view, uint32_t &out_value)
    {
        constexpr uint32_t kLocalSize   = 17u;
        constexpr uint32_t kLocalSizeId = 38u;

        return FindSPVExecutionModeLiteral(view, kLocalSize, 0, out_value)
            || FindSPVExecutionModeIdConstant(view, kLocalSizeId, 0, out_value);
    }






    // ── Q0 缓存身份的几何夹具 ────────────────────────────────────────────
    //
    // 放在命名空间作用域：`emit-fixed-mesh` 也要用同一份几何去捕获网格源。
    // 形状键只吃（顶点节点配置、varying、位置格式、mesh 模式、invocation 上限），
    // 与模板/profile 无关，所以捕获侧可以用路由器选模板而不必复现这条用例
    // 手选的模板 ID；但**几何不能抄两份**——抄歪了只会静默少覆盖一个形状。
    static const GeometryVertexFormat kCacheIdentityLitGeometryA{
        {VertexSemantic::Position, VF_V3F},
        {VertexSemantic::TexCoord, VF_V2F},
        {VertexSemantic::Normal,   VF_V3F}
    };
    // 「同语义、不同 ABI」的对照几何：TexCoord 从 VF_V2F（8B）换成 VF_V2HF（RG16F，4B），
    // s1_uv 模块随之从 s1_uv 变成 s1_uv_rg16f —— 顶点输入文本与流哈希必须不同。
    //
    // 这里刻意不再用 VF_V2I16 制造对照：2 分量位置只能配 2D 映射的材质，而 `lit`
    // 是 Passthrough3D，那份 GLSL 的 `vec4(vec3, 1.0)`/`vec4(vec2, 1.0)` 根本编不过。
    // 生成期已把「位置分量数 ↔ 映射分量数」做成硬门，ABI 对照只能换非位置语义。
    static const GeometryVertexFormat kCacheIdentityLitGeometryB{
        {VertexSemantic::Position, VF_V3F},
        {VertexSemantic::TexCoord, VF_V2HF},
        {VertexSemantic::Normal,   VF_V3F}
    };
    // 引擎标准紧凑几何（SimpleSphere / BasicLitMeshes / LoadGeometry / LoadScene 等示例）：
    // TexCoord 为 VF_V2HF（RG16F），Normal 为 VF_V2UN8（RG8 octahedral）。
    static const GeometryVertexFormat kCacheIdentityLitGeometryStandard{
        {VertexSemantic::Position, VF_V3F},
        {VertexSemantic::TexCoord, VF_V2HF},
        {VertexSemantic::Normal,   VF_V2UN8}
    };
    static const GeometryVertexFormat kCacheIdentityColorGeometry{
        {VertexSemantic::Position, VF_V3F},
        {VertexSemantic::Color,    VF_V4UN8}
    };
    static const GeometryVertexFormat kCacheIdentityPureColorGeometry{
        {VertexSemantic::Position, VF_V3F}
    };
    static const GeometryVertexFormat kCacheIdentityLuminanceGeometry{
        {VertexSemantic::Position,  VF_V3F},
        {VertexSemantic::Luminance, VF_V1UN8}
    };

















    static std::string ReadFileText(const std::string &path)
    {
        std::ifstream in(path, std::ios::binary);
        if (!in)
            return {};
        return std::string(
            std::istreambuf_iterator<char>(in),
            std::istreambuf_iterator<char>());
    }

    static bool ContainsAnyToken(
        const std::string &text,
        std::initializer_list<const char *> tokens) noexcept
    {
        for (const char *token : tokens)
        {
            if (!token || !token[0])
                continue;
            if (text.find(token) != std::string::npos)
                return true;
        }
        return false;
    }


    /// 去掉 // 行注释与 /* */ 块注释。字段提取必须先剥注释：结构上方的注释里出现过
    /// pc_root.addr_global_addresses 这类字样，不剥会被当成字段名收进来。
    static void StripCommentTokens(std::string &text)
    {
        std::string out;
        out.reserve(text.size());
        for (size_t i = 0; i < text.size();)
        {
            if (text[i] == '/' && i + 1 < text.size() && text[i + 1] == '/')
            {
                while (i < text.size() && text[i] != '\n') ++i;
                continue;
            }
            if (text[i] == '/' && i + 1 < text.size() && text[i + 1] == '*')
            {
                i += 2;
                while (i + 1 < text.size() && !(text[i] == '*' && text[i + 1] == '/')) ++i;
                i = (i + 1 < text.size()) ? i + 2 : text.size();
                continue;
            }
            out.push_back(text[i++]);
        }
        text.swap(out);
    }

    /// 取「声明名 == name，且其后（跳过空白）紧跟 `{`」的记录体（不含花括号本身）。
    ///
    /// 为什么要按名字+花括号定位，而不是按 `"struct Xxx"` 文本定位：C++ 侧结构可以带
    /// 属性宏（`struct alignas(64) HGL_SSBO_BDA LightRow`）或对齐说明符，字面量前缀会漂移；
    /// GLSL 侧则写成 `buffer XxxRef {`。统一判据 = 名字前后不是标识符字符，且名字后紧跟 `{`，
    /// 两端都成立。`kLightRowCapacity` / `LightRowBufferRef` 这类别名会被前后缀检查挡掉。
    static bool FindRecordBody(const std::string &raw_text,
                               const char *name,
                               std::string &out_body)
    {
        const size_t name_len = std::strlen(name);
        if (name_len == 0)
            return false;

        const auto is_ident = [](const char ch)
        {
            return std::isalnum(static_cast<unsigned char>(ch)) || ch == '_';
        };

        for (size_t pos = raw_text.find(name);
             pos != std::string::npos;
             pos = raw_text.find(name, pos + 1))
        {
            if (pos > 0 && is_ident(raw_text[pos - 1]))
                continue;
            const size_t after = pos + name_len;
            if (after < raw_text.size() && is_ident(raw_text[after]))
                continue;

            size_t cursor = after;
            while (cursor < raw_text.size()
                && std::isspace(static_cast<unsigned char>(raw_text[cursor])))
                ++cursor;

            if (cursor >= raw_text.size() || raw_text[cursor] != '{')
                continue;

            const size_t close = raw_text.find('}', cursor);
            if (close == std::string::npos)
                return false;

            out_body = raw_text.substr(cursor + 1, close - cursor - 1);
            return true;
        }

        return false;
    }

    /// 取记录体（`FindRecordBody`）内的「uint64_t <名字>」字段名（保序）。
    static std::vector<std::string> ExtractU64Fields(const std::string &raw_text,
                                                     const char *name)
    {
        std::vector<std::string> fields;
        std::string text = raw_text;
        StripCommentTokens(text);

        std::string body;
        if (!FindRecordBody(text, name, body))
            return fields;
        const char type[] = "uint64_t";
        const size_t type_len = sizeof(type) - 1;

        for (size_t pos = 0; (pos = body.find(type, pos)) != std::string::npos;)
        {
            pos += type_len;
            while (pos < body.size() && std::isspace(static_cast<unsigned char>(body[pos])))
                ++pos;

            size_t end = pos;
            while (end < body.size()
                && (std::isalnum(static_cast<unsigned char>(body[end])) || body[end] == '_'))
                ++end;

            if (end > pos)
                fields.emplace_back(body.substr(pos, end - pos));

            pos = end;
        }
        return fields;
    }



    /// 逐行式结构字段取块器（E.entity-gpu-struct-parity 用；**不能**沿用
    /// ExtractU64Fields 的花括号配对路子 —— §7.6 实测：`EntityGPU` 成员初始化式
    /// `= {}` 里含 `}`，`text.find('}')` 把块尾**提前**截断、恒产出 0 字段）。
    /// 规则：命中 "struct <marker>" 行进入块体；跳过以 `{` 开头的行；遇以 `}`
    /// 开头的行终止。每行去 `=...` 初始化式、去结尾 `;`、去 type 与 `[` 间空格，
    /// 再拆 `type name` / `type name[dim]`（dim 可为宏名）。注释已由
    /// StripCommentTokens 剥掉 —— 与 ExtractU64Fields 同一前置，防注释字样混入。
    struct StructFieldEntry
    {
        std::string type;   // 归一后的类型（uint32_t→uint，uint8_t→uint8）
        std::string name;
        std::string dim;    // 数组长度原文（归一后）；标量字段为空
    };

    /// 类型/长度归一（§7.6 第 4 条）：两端写法不同但语义同一的符号折到同一口径，
    /// 否则恒不相等（假红）。
    static void NormalizeStructField(StructFieldEntry &field)
    {
        if (field.type == "uint32_t") field.type = "uint";
        if (field.type == "uint8_t")  field.type = "uint8";
        // T6b：`LightRow` 是 GLSL 镜像行 ⇒ 数学类型要与 GLSL 写法折到同一口径
        if (field.type == "Vector4f") field.type = "vec4";
        if (field.type == "Color4f")  field.type = "vec4";
        if (field.type == "Vector4u") field.type = "uvec4";
        if (field.type == "Vector2f") field.type = "vec2";
        if (field.type == "Matrix4f") field.type = "mat4";
        if (field.dim == "kEntityGPUSlotCount") field.dim = "ENTITY_GPU_SLOT_COUNT";
    }

    static std::vector<StructFieldEntry> ExtractStructFieldsLineWise(
        const std::string &raw_text,
        const char *marker)
    {
        std::vector<StructFieldEntry> fields;
        std::string text = raw_text;
        StripCommentTokens(text);

        bool in_block = false;
        std::istringstream stream(text);
        std::string line;
        while (std::getline(stream, line))
        {
            // 行首/行尾空白（含 CRLF 残留的 \r）
            const size_t lhs = line.find_first_not_of(" \t\r");
            if (lhs == std::string::npos)
                continue;
            const size_t rhs = line.find_last_not_of(" \t\r");
            line = line.substr(lhs, rhs - lhs + 1);

            if (!in_block)
            {
                // 块头 = 以 struct 开头、含 marker、且块体 `{` 另起一行的声明行。
                if (line.rfind("struct ", 0) == 0
                 && line.find(marker) != std::string::npos
                 && line.find('{') == std::string::npos)
                    in_block = true;
                continue;
            }

            if (line[0] == '}')
                break;
            if (line[0] == '{')
                continue;

            // 去初始化式（`= 0` / `= {}` —— 这正是花括号配对版被截断的源头）
            const size_t eq = line.find('=');
            if (eq != std::string::npos)
                line = line.substr(0, eq);
            // 去结尾 `;`（先剥回绕的空白）
            const size_t semi = line.find_last_not_of(" \t\r");
            if (semi == std::string::npos)
                continue;
            line = line.substr(0, semi + 1);
            if (!line.empty() && line.back() == ';')
                line.pop_back();
            // `row [16]` → `row[16]`：类型与名字、名字与 `[` 之间的空白统一拆掉
            for (std::string::size_type at = line.find(" [");
                 at != std::string::npos; at = line.find(" [", at))
                line.erase(at, 1);

            std::istringstream parts(line);
            StructFieldEntry field;
            std::string rest;
            if (!(parts >> field.type) || !(parts >> rest))
                continue;
            const size_t bracket = rest.find('[');
            if (bracket == std::string::npos)
            {
                field.name = rest;
            }
            else
            {
                field.name = rest.substr(0, bracket);
                const size_t close = rest.find(']');
                field.dim = (close == std::string::npos)
                    ? rest.substr(bracket + 1)
                    : rest.substr(bracket + 1, close - bracket - 1);
            }
            // 标识符防御：只收名字合法的字段（挡住漏网的非成员行）
            if (field.name.empty()
             || !(std::isalpha(static_cast<unsigned char>(field.name[0]))
                  || field.name[0] == '_'))
                continue;
            NormalizeStructField(field);
            fields.push_back(field);
        }
        return fields;
    }

    /// 收集文本里的 `#define <名字> <十进制值>`（注释已由 StripCommentTokens 剥掉）——
    /// 结构字段数组长度写的是宏名时，**值从定义处现读**，门内不复制任何数字（单一真源：
    /// 改 #define 即改判定；读不出值 = fail-fast，与容量契约同纪律）。
    static std::vector<std::pair<std::string, size_t>> CollectDefineConstants(
        const std::string &comment_stripped_text)
    {
        std::vector<std::pair<std::string, size_t>> defines;
        std::istringstream stream(comment_stripped_text);
        std::string line;
        while (std::getline(stream, line))
        {
            std::istringstream parts(line);
            std::string hash, name, value;
            if (!(parts >> hash >> name >> value) || hash != "#define")
                continue;
            try
            {
                defines.emplace_back(name, std::stoull(value));
            }
            catch (...)
            {
                // 非十进制值的宏与结构长度无关，跳过（长度用到它时仍会 fail-fast）
            }
        }
        return defines;
    }

    /// 结构长度宏的两处来源：GLSL 文件本体 + ShaderConstGen 生成物。
    /// `ENTITY_GPU_SLOT_COUNT` 等已收敛为 `inc/hgl/shader/contract/*.h` 的契约常量
    /// （GLSL 侧由 ShaderLibrary/contract/engine_constants.glsl 提供），文件本体只剩
    /// `#include`，所以查找表必须并上生成物。本文件内的定义优先。
    static std::vector<std::pair<std::string, size_t>> CollectDefineConstantsWithContract(
        const std::string &comment_stripped_text)
    {
        auto defines = CollectDefineConstants(comment_stripped_text);

        std::string contract_text =
            ReadFileText(RepoRootPath("ShaderLibrary/contract/engine_constants.glsl"));
        StripCommentTokens(contract_text);
        for (auto &entry : CollectDefineConstants(contract_text))
            defines.push_back(std::move(entry));

        return defines;
    }

    /// 字段的存储尺寸（本结构词汇表内）：uint=4 / uint8=1；数组 = 元素 × 长度。
    /// dim 为字面量直接用；为宏名则查 defines（CollectDefineConstants 产出）。
    static bool StructFieldStorageSize(const StructFieldEntry &field,
                                       const std::vector<std::pair<std::string, size_t>> &defines,
                                       size_t &out_bytes)
    {
        // 词汇表：uint8=1 / uint=float=4 / vec2=8 / vec4=uvec4=16 / mat4=64（本门只对拍
        // 逐行式取块器的产物，字段类型都在这张表内；未登记类型按 4 处理会在总尺寸处报红）
        if (field.type == "uint8")      out_bytes = 1u;
        else if (field.type == "vec2")  out_bytes = 8u;
        else if (field.type == "vec4")  out_bytes = 16u;
        else if (field.type == "uvec4") out_bytes = 16u;
        else if (field.type == "mat4")  out_bytes = 64u;
        else                            out_bytes = 4u;
        if (field.dim.empty())
            return true;
        try
        {
            out_bytes *= std::stoull(field.dim);
            return true;
        }
        catch (...)
        {
        }
        for (const auto &entry : defines)
        {
            if (entry.first == field.dim)
            {
                out_bytes *= entry.second;
                return true;
            }
        }
        return false;
    }

    /// 取 `token` 之后**首个十进制整数**（吃 `= 0` / `= 0u` / `#define X 0u` 三种写法）；
    /// 不许跨行取，找不到返回 -1（调用方据此报错，不静默当 0）。
    static int ExtractFirstIntAfter(const std::string &text, const std::string &token)
    {
        const size_t at = text.find(token);
        if (at == std::string::npos)
            return -1;

        size_t i = at + token.size();
        for (; i < text.size(); ++i)
        {
            const char ch = text[i];
            if (ch >= '0' && ch <= '9')
                break;
            if (ch == '\n')
                return -1;
        }
        if (i >= text.size())
            return -1;

        int value = 0;
        for (; i < text.size() && text[i] >= '0' && text[i] <= '9'; ++i)
            value = value * 10 + (text[i] - '0');
        return value;
    }





    /// 「表归属」分类表（**唯一真源**）：每个地址字段必须**恰好**属于一侧 ——
    /// `WorldAddresses`（世界私有：多世界同帧各写自己的）或 `GlobalAddresses`（设备级全局：跨世界共享的资源池）。
    /// 新增地址字段必须在此登记，否则门报「未分类字段」（见 §6 契约 ④）。
    struct AddressOwnershipEntry
    {
        const char *field;
        bool world_owned;
    };

    static const AddressOwnershipEntry kAddressOwnershipTable[] =
    {
        // ── 世界私有（随世界：相机行表 / 实体行表 / 实体 4-ID 表 / L2W / 光源行 / 影子数据 / 环境光块）──
        //   `addr_sky` 已于 2026-10-07 随 sky 段整删退出（表内不再登记）
        { "addr_camera_info",           true  },
        { "addr_shadow",                true  },
        { "addr_entity_gpu",           true  },   // T3：实体行表（世界私有；实体属世界 ⇒ 设备级会重演"跨世界同行冲突"）
        { "addr_l2w",                  true  },   // T3d：L2W 矩阵池（世界私有 TransformAssignmentBuffer；按帧槽切份）
        { "addr_entity_render_ids",    true  },   // T3d-3a：实体 4-ID 表（世界私有 EntityRenderIDTable；GPU 消费者取池口径 id 的唯一通路）
        { "addr_lights",               true  },   // T6b：光源行表（世界私有 LightRowTable；行 0 = 主方向光，按帧槽切份）
        { "addr_ambient_env",          true  },   // T1：环境光 provider 参数块（世界私有；单份 ⇒ 全帧槽同址）
        // ── 设备级全局（跨世界共享的资源池 + 每帧槽字段）──
        { "addr_mesh_draw_params_pool", false },
        { "addr_geometry_aabb",         false },   // B3-i：GeometryAABB 池（设备级全局；与 MeshDrawParams 同 ID 索引）
        { "addr_pbr_surface",           false },
        { "addr_emissive_surface",      false },
        { "addr_transmission_surface",  false },
        { "addr_pbr_advanced_surface",  false },
        { "addr_color_palette",         false },
        { "addr_viewport",              false },
    };




















    // ══════════════════════════════════════════════════════════════════════
    // golden 文本比对基础设施
    //
    // 本文件大量断言形如 contains(fs, "layout(set=2, binding=0)") 的写法会把
    // 测试与具体产物格式双向锁死：格式一变就要改断言。凡是能用「整份快照逐行
    // 比对」表达的判据，一律走这里。
    //
    // golden 缺失时**自动生成**并通过（首跑引导）——生成后须人工审阅并入库；
    // 已存在时不一致即 FAIL，并落地 .actual 便于 diff。
    // ══════════════════════════════════════════════════════════════════════
    static bool ReadGoldenFile(const std::string &path, std::string &out)
    {
        std::ifstream in(path, std::ios::binary);
        if (!in)
            return false;

        out.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
        return true;
    }

    static bool WriteTextFile(const std::string &path, const std::string &text)
    {
        std::error_code ec;
        std::filesystem::create_directories(
            std::filesystem::path(path).parent_path(), ec);

        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        if (!out)
            return false;

        out.write(text.data(), std::streamsize(text.size()));
        return bool(out);
    }

    /// 行尾归一（golden 入库后 git 可能改行尾，比对不应受其影响）
    static std::string NormalizeDumpEOL(const std::string &src)
    {
        std::string out;
        out.reserve(src.size());
        for (const char c : src)
            if (c != '\r')
                out += c;
        return out;
    }

    /// golden 比对（首跑引导 + 首个差异行定位）。
    ///
    /// 缺失即生成并通过（提示人工审阅入库），存在即逐行严格比对；不一致时
    /// 落地 `<golden>.actual` 并只报**首个**差异行——快照动辄数百行，把全文
    /// 塞进诊断会把真正的第一处变化淹掉。
    static void CompareAgainstGolden(
        const std::string &golden_path,
        const std::string &dump,
        const char *tag,
        std::vector<std::string> &diagnostics)
    {
        std::string golden;
        if (!ReadGoldenFile(golden_path, golden))
        {
            if (!WriteTextFile(golden_path, dump))
            {
                diagnostics.emplace_back(
                    std::string("cannot write golden: ") + golden_path);
                return;
            }

            std::printf(
                "[%s] golden 已生成（首跑引导），请审阅后入库: %s\n",
                tag, golden_path.c_str());
            return;
        }

        const std::string normalized_golden = NormalizeDumpEOL(golden);
        if (normalized_golden == dump)
            return;

        const std::string actual_path = golden_path + ".actual";
        WriteTextFile(actual_path, dump);

        size_t pos = 0;
        size_t line = 1;
        while (pos < dump.size() && pos < normalized_golden.size()
            && dump[pos] == normalized_golden[pos])
        {
            if (dump[pos] == '\n')
                ++line;
            ++pos;
        }

        const auto line_of = [](const std::string &text, const size_t at)
        {
            if (text.empty())
                return std::string();

            const size_t clamped = std::min(at, text.size() - 1);
            const size_t begin = text.rfind('\n', clamped) == std::string::npos
                ? 0 : text.rfind('\n', clamped) + 1;
            const size_t end = text.find('\n', clamped);
            return text.substr(begin,
                (end == std::string::npos ? text.size() : end) - begin);
        };

        diagnostics.emplace_back(
            std::string("golden mismatch: ") + tag
            + " line " + std::to_string(line)
            + "\n  golden: " + line_of(normalized_golden, pos)
            + "\n  actual: " + line_of(dump, pos)
            + "\n  actual 全文: " + actual_path);
    }


    /// 发行形态**查表**链路端到端门
    ///
    /// 上一个门只验数据容器；这个门验的是运行时真正会走的那条路：装 Manifest 后端
    /// → 用语义键查表 → 装载 program metadata 与两个 stage 的 SPV → 拼出可直接
    /// 提交给 Vulkan 的 `ShaderBuildContext`。全过程不碰 GLSL、不碰编译器。
    ///
    /// 同时断言「查不到就硬失败」：`ambient_module_name` 一变，键就变，后端必须
    /// 返回 nullptr 而不是凑一份近似产物。
    /// 把一次烘焙的「正确答案」写成文本，供另一进程（发行形态探针）对照。
    /// 放在清单同目录下，探针只认「含 program_manifest.pmf 的那个目录」这一个路径。
    /// 字段名与 RuntimeFormProbe.cpp 的 LoadExpectation() 严格对应，改一处必须同步另一处。
    static bool WriteManifestExpectation(
        const ShaderArtifactStore &store,
        const ResolvedMaterial &definition,
        const ShaderProgramManifestEntry &entry)
    {
        const hgl::OSString manifest_os =
            MakeShaderProgramManifestPath(store.GetRootPath());
        const std::filesystem::path manifest_path(manifest_os.c_str());

        std::ofstream out(
            manifest_path.parent_path() / "expectation.txt", std::ios::binary);
        if (!out)
            return false;

        const ShaderProgramKey program_key = entry.program_link.BuildKey();
        out << "domain=" << GetMaterialDomainName(definition.domain) << "\n"
            << "program_digest=" << program_key.GetDigest() << "\n"
            << "lookup_key=" << entry.lookup_key << "\n"
            << "mesh_stage_digest=" << program_key.mesh_stage_digest << "\n"
            << "fragment_stage_digest=" << program_key.fragment_stage_digest << "\n"
            << "mesh_spv_bytes=" << sizeof(uint32_t) * 4 << "\n"
            << "fragment_spv_bytes=" << sizeof(uint32_t) * 4 << "\n"
            << "requires_recipe_runtime_rows="
            << (entry.requires_recipe_runtime_rows ? 1 : 0) << "\n"
            << "requires_recipe_payload_row="
            << (entry.requires_recipe_payload_row ? 1 : 0) << "\n";
        return out.good();
    }

    /// 把命令行给的目录解析成「真正含 program_manifest.pmf 的目录」。
    /// ShaderArtifactStore 会在传入根下面再建一层 `shader-cache/`，这里把这一跳
    /// 藏掉，避免命令行与探针必须知道这个内部布局。
    static bool ResolveEmittedCacheDir(
        const std::filesystem::path &root, hgl::OSString &out)
    {
        std::error_code ec;

        if (std::filesystem::exists(root / "program_manifest.pmf", ec))
        {
            out = hgl::OSString(root.wstring().c_str());
            return true;
        }

        const std::filesystem::path nested = root / "shader-cache";
        if (std::filesystem::exists(nested / "program_manifest.pmf", ec))
        {
            out = hgl::OSString(nested.wstring().c_str());
            return true;
        }

        return false;
    }



    /// 结构快照 / 发射顺序快照共用的夹具几何。
    ///
    /// 刻意给全 6 个语义：领域契约会按自己的 mask 取子集，多余的不会被用到；
    /// 给全才能让「同一张表覆盖所有领域」成立。
    static const GeometryVertexFormat kReflectionCoverageGeometry{
        {VertexSemantic::Position, VF_V3F},
        {VertexSemantic::TexCoord, VF_V2F},
        {VertexSemantic::Normal, VF_V3F},
        {VertexSemantic::Color, VF_V4F},
        {VertexSemantic::TransformID, VK_FORMAT_R32_UINT},
        {VertexSemantic::Size, VF_V2F}
    };

    static const GeometryVertexFormat kReflectionCoveragePaletteGeometry{
        {VertexSemantic::Position, VF_V3F},
        {VertexSemantic::TexCoord, VF_V2F},
        {VertexSemantic::Normal, VF_V3F},
        {VertexSemantic::Color, VK_FORMAT_R8_UINT},
        {VertexSemantic::TransformID, VK_FORMAT_R32_UINT},
        {VertexSemantic::Size, VF_V2F}
    };

    /// 2D 位置几何：`Position` 用 `VF_V2F`。
    ///
    /// `LineQuad` 的端点读取按 `position_format` 分叉：`R32G32_SFLOAT` 要把
    /// `vec2` 补成 `vec3(..., 0.0)`，其余直读 `vec3`。两条分支的 GLSL 不同，
    /// 只有几何位置格式能选中它们——共享表恒为 `VF_V3F`，vec2 那条永远走不到。
    static const GeometryVertexFormat kReflectionCoverageGeometry2D{
        {VertexSemantic::Position, VF_V2F},
        {VertexSemantic::TexCoord, VF_V2F},
        {VertexSemantic::Normal, VF_V3F},
        {VertexSemantic::Color, VF_V4F},
        {VertexSemantic::TransformID, VK_FORMAT_R32_UINT},
        {VertexSemantic::Size, VF_V2F}
    };

    struct ReflectionCoverageVariant
    {
        /// 身份只由领域决定：矩阵按领域取定义，配方/资产不再携带定义 ID。
        MaterialDomain domain;
        const char *variant_slug;
        ShaderProgramPurpose purpose;
        PassProfileID profile_id;
        bool has_geometry;   // CharQuad 文本材质无需几何顶点格式（mesh 自声明 SSBO）
        /// 强制 alpha_test（覆盖材质定义里的渲染状态）。
        ///
        /// ShadowCaster 骨架有两条互斥分支：不带 alpha 的 `void main(){}` 和带
        /// alpha 求值的 masked 分支。前者被普通 Lit 材质覆盖，后者只有「材质
        /// 声明了 alpha_test/dither」才会走到——现有 9 个示例材质一个都没有，
        /// 于是那条分支在快照矩阵里**完全没有网**。这里显式构造 masked 变体，
        /// 让外置骨架重构不至于把一条无覆盖的分支改坏。
        bool alpha_test;
        /// 图元类型。mesh 发射模式的选择优先级是
        /// `definition.mesh_shader_mode` > `primitive_type` 推断：
        /// `CharQuad` 靠材质定义显式指定，`LineQuad` 则只能由 `Lines` 推出来。
        /// 默认 `Triangles` = 前 14 个变体全部走 `VertexPassthrough`。
        PrimitiveType primitive_type = PrimitiveType::Triangles;
        /// 位置属性用 `VF_V2F`（见 `kReflectionCoverageGeometry2D`）。
        ///
        /// 只对 `position_mapping` 需要 vec2 Position 的材质成立
        /// （`NDCLift` / `ZeroOneToNDC` / `PixelToLocal` / `LiftXY_*`）；
        /// `Passthrough3D` 必须配 3 分量。两者不一致时生成器发射期即硬失败
        /// （`VertexNodeConfigResolver::CheckPositionComponentCount`），
        /// 而不是产出一份编不过的 GLSL。
        bool vec2_position = false;
        /// 表面模块（仅 `no_material_source` 领域有效）。
        ///
        /// Sky 的两个变体若不在这里分叉，就会解析出同一个默认主着色器
        /// （`sky_minimal.frag`）——两条用例其实是同一条，`sky_cube` 那条
        /// 等于没有覆盖。
        const char *surface_module_name = nullptr;
    };

    /// 快照矩阵：四条发射路径（ForwardLit / ForwardUnlit / ShadowCaster / Sky）
    /// × 领域差异（有纹理 / 纯顶点 / 无顶点格式的 SSBO 材质）。
    ///
    /// 两张快照（结构、发射顺序）共用此表——矩阵只有一份，新增材质不会漏测其中一张。
    static const ReflectionCoverageVariant kReflectionCoverageVariants[] =
    {
        { MaterialDomain::StandardOpaque, "lit-forward-opaque",
          ShaderProgramPurpose::ForwardColor,
          PassProfileID::ForwardLitShadowedAO, true, false },
        { MaterialDomain::StandardOpaque, "lit-depth-only",
          ShaderProgramPurpose::DepthOnly,
          PassProfileID::ShadowCasterOpaque, true, false },
        { MaterialDomain::StandardOpaque, "lit-shadow-depth",
          ShaderProgramPurpose::ShadowDepth,
          PassProfileID::ShadowCasterOpaque, true, false },
        { MaterialDomain::StandardOpaque, "lit-depth-masked",
          ShaderProgramPurpose::DepthOnly,
          PassProfileID::ShadowCasterMasked, true, true },
        { MaterialDomain::StandardOpaque, "lit-shadow-masked",
          ShaderProgramPurpose::ShadowDepth,
          PassProfileID::ShadowCasterMasked, true, true },
        { MaterialDomain::VertexPalette, "vertex-palette-color-forward",
          ShaderProgramPurpose::ForwardColor,
          PassProfileID::ForwardUnlit, true, false },
        { MaterialDomain::VertexColor, "vertex-color-forward",
          ShaderProgramPurpose::ForwardColor,
          PassProfileID::ForwardUnlit, true, false },
        { MaterialDomain::VertexLuminance, "vertex-luminance-forward",
          ShaderProgramPurpose::ForwardColor,
          PassProfileID::ForwardUnlit, true, false },
        { MaterialDomain::UnlitTexture, "unlit-texture-forward",
          ShaderProgramPurpose::ForwardColor,
          PassProfileID::ForwardUnlit, true, false,
          PrimitiveType::Triangles, true },
        { MaterialDomain::DebugNormal, "debug-normal-forward",
          ShaderProgramPurpose::ForwardColor,
          PassProfileID::ForwardUnlit, true, false },
        { MaterialDomain::Sky, "sky-cube-forward",
          ShaderProgramPurpose::ForwardColor,
          PassProfileID::Sky, true, false,
          PrimitiveType::Triangles, false, "surface_sky_cube" },
        { MaterialDomain::Sky, "sky-minimal-forward",
          ShaderProgramPurpose::ForwardColor,
          PassProfileID::Sky, true, false },
        { MaterialDomain::Checker2D, "checkerboard-2d-forward",
          ShaderProgramPurpose::ForwardColor,
          PassProfileID::ForwardUnlit, true, false,
          PrimitiveType::Triangles, true },
        { MaterialDomain::Text2D, "text-gpu-charquad",
          ShaderProgramPurpose::ForwardColor,
          PassProfileID::ForwardUnlit, false, false },
        // LineQuad 是第三种 mesh 发射模式，只能由 `primitive_type = Lines` 推出来
        // （材质定义里没有对应字段）。这些用例覆盖 LineQuad 固定拓扑 master 与
        // `module/stage/line_quad_body.glsl` 的端点读取路径，避免该独立几何路径退化后漏报。
        { MaterialDomain::VertexColor, "vertex-color-lines-quad",
          ShaderProgramPurpose::ForwardColor,
          PassProfileID::ForwardUnlit, true, false,
          PrimitiveType::Lines, false },
        // 2D 位置需要与顶点格式匹配的 2D 映射模块。用 checkerboard_2d
        // （NDCLift）覆盖 VF_V2F 夹具，避免把 Passthrough3D 的 vec3 ABI 错配进来。
        { MaterialDomain::Checker2D, "checkerboard-2d-lines-quad-2d",
          ShaderProgramPurpose::ForwardColor,
          PassProfileID::ForwardUnlit, true, false,
          PrimitiveType::Lines, true },
        // LineList 是第四种发射模式（原生 1px 线图元），与 CharQuad 一样由
        // `definition.mesh_shader_mode` 显式指定（VertexPaletteLines 领域契约携带），
        // 不依赖 primitive_type 推断。覆盖 linelist_128.mesh 与 line_list_body.glsl。
        { MaterialDomain::VertexPaletteLines, "vertex-palette-lines-list",
          ShaderProgramPurpose::ForwardColor,
          PassProfileID::ForwardUnlit, true, false,
          PrimitiveType::Lines, false },
    };

    /// 采样器预设表是进程级单例，`BuildSamplerMacros` 靠它把 `Trilinear`
    /// 翻成 `#define TrilinearSampler 2u`。没装载时宏就整体消失——GLSL 仍能生成，
    /// 只是少几行 `#define`，静默且致命。
    ///
    /// 单跑 `pipeline` 组时装载发生在 `materialization` 组里，跨组依赖会让快照
    /// 随「跑哪个组」而变。快照门必须自带前置条件，否则 golden 自身不可信。
    static bool EnsureSamplerPresetsLoaded(std::string &diagnostic)
    {
        auto &lib = SamplerPresetLibrary::Instance();
        if (lib.GetCount() != 0)
            return true;

        const std::string path = GetShaderLibraryPath() + "/contract/sampler.toml";
        const hgl::filesystem::Path sampler_toml(hgl::ToOSString(path));
        if (lib.Load(sampler_toml.ToOSString()))
            return true;

        diagnostic = "cannot load sampler presets from " + path;
        return false;
    }

    static ScenePassProfile MakeReflectionCoverageProfile(
        const PassProfileID profile_id)
    {
        switch (profile_id)
        {
            case PassProfileID::ForwardLitShadowedAO:
            case PassProfileID::ForwardLitShadowedIdentityAO:
                return MakeIdentityForwardLitProfile();
            case PassProfileID::ForwardUnlit:
                return MakeForwardUnlitProfile();
            case PassProfileID::Sky:
                return MakeSkyProfile();
            default:
                return MakeShadowCasterProfile();
        }
    }

    /// 按快照矩阵构建一个材质实例。失败时把原因写进 `diagnostic` 并返回 nullptr。
    static std::unique_ptr<ShaderBuildContext> BuildReflectionCoverageVariant(
        const ReflectionCoverageVariant &variant, std::string &diagnostic,
        FixedFragmentSource *out_fixed = nullptr,
        hgl::AnsiString *out_mesh_options = nullptr,
        hgl::AnsiString *out_common_options = nullptr)
    {
        ResolvedMaterial definition{};
        if (!BuildResolvedMaterialForDomain(variant.domain, definition))
        {
            diagnostic = std::string("domain definition unavailable: ")
                + GetMaterialDomainName(variant.domain);
            return nullptr;
        }

        // 必须在解析 template request 之前生效：coverage 契约（是否需要求 alpha、
        // 需要哪些 interstage 语义）由 `ResolveMaterialRenderState` 从这里读。
        if (variant.alpha_test)
        {
            definition.default_render_state.alpha_test = true;
            definition.default_render_state.alpha_cutoff = 0.5f;
        }

        // 表面形态必须在 `AppendMaterialPassProfileRoots` / 固定片段解析之前落定，
        // 否则 Sky 的 cube 变体会静默退回默认的 sky_minimal 主着色器。
        if (variant.surface_module_name)
            definition.surface_module_name = variant.surface_module_name;

        MaterialBuildRequest request{};
        request.recipe.domain = definition.domain;
        request.primitive_type = variant.primitive_type;
        request.geometry_vertex_format =
            variant.has_geometry
                ? (definition.vertex_varying.emit_vertex_color_from_palette
                       ? &kReflectionCoveragePaletteGeometry
                       : (variant.vec2_position
                              ? &kReflectionCoverageGeometry2D
                              : &kReflectionCoverageGeometry))
                : nullptr;
        request.defer_finalize = true;
        request.shader_program_purpose = variant.purpose;

        PassProfileValidationDiagnostic template_diagnostic{};
        if (!ResolveScenePassProfileRequest(
                variant.profile_id, hgl::graph::ShaderStage::Fragment,
                MakeReflectionCoverageProfile(variant.profile_id),
                request.render_template_request, template_diagnostic)
         || !AppendMaterialPassProfileRoots(
                definition, request.render_template_request))
        {
            diagnostic = std::string("template request resolve failed: ")
                + variant.variant_slug;
            return nullptr;
        }

        // 片段源走领域固定源：本门要证的是「拿真实生产会用的那份固定片段，
        // 与网格阶段拼在一起后，接口在 SPIR-V 里仍然成立」。若这里退回生成器
        // 的片段拼装，证的就是一条已经不存在的路径。
        FixedFragmentSource fixed;
        if (!ResolveFixedFragmentSource(definition, request, fixed))
        {
            diagnostic = std::string("fixed fragment source failed: ")
                + variant.variant_slug + " | " + GetLastDomainShaderBuildBackendError();
            return nullptr;
        }
        FixedStageInputs fixed_stage;
        fixed_stage.fragment_source = fixed.source.c_str();
        fixed_stage.fragment_interface_hash = fixed.interface_hash;
        fixed_stage.fragment_options_digest = fixed.options_digest;

        // 网格 options：生产侧由后端落盘并挑成对目录；本门手写编译，自己收下文本。
        MaterialProgramBuildCapture capture{};
        std::unique_ptr<ShaderBuildContext> ctx(
            AssembleMaterialProgram(nullptr, request, definition, &capture, fixed_stage));
        if (!ctx)
        {
            diagnostic = std::string("material build failed: ") + variant.variant_slug;
            return nullptr;
        }
        if (out_mesh_options)
            *out_mesh_options = capture.mesh_options;
        if (out_common_options)
            *out_common_options = capture.interstage_varying_options;

        if (out_fixed)
            *out_fixed = std::move(fixed);

        return ctx;
    }

    /// 固定 mesh 源目录里所有 `.mesh` 的文本。
    ///
    /// 6.5 之后网格阶段**只有**固定源一条路径：目录里没有对应形状，该域就没有
    /// mesh 阶段可编译，没有生成器兜底。因此这张表既是数据源也是覆盖率的判据。
    static bool LoadFixedMeshCatalog(
        std::vector<std::string> &out, std::string &diagnostic)
    {
        out.clear();

        // 形状主着色器（`topology/*.mesh`）是唯一的固定源：形状名即文件名，
        // 三种几何模式（VertexPassthrough / LineQuad / CharQuad）全在此目录。
        const std::filesystem::path dirs[1] =
        {
            RepoRootPath("ShaderLibrary/topology")
        };

        try
        {
            bool seen_any_dir = false;
            for (const std::filesystem::path &dir : dirs)
            {
                if (!std::filesystem::exists(dir)
                 || !std::filesystem::is_directory(dir))
                    continue;
                seen_any_dir = true;

                for (const auto &entry : std::filesystem::directory_iterator(dir))
                {
                    if (!entry.is_regular_file()
                     || entry.path().extension() != ".mesh")
                        continue;

                    const std::string text = ReadFileText(entry.path().string());
                    if (text.empty())
                    {
                        diagnostic = "cannot read fixed mesh source: "
                            + entry.path().string();
                        return false;
                    }
                    out.push_back(text);
                }
            }

            if (!seen_any_dir)
            {
                diagnostic = "fixed mesh source directory missing: "
                    + dirs[0].string();
                return false;
            }
        }
        catch (const std::filesystem::filesystem_error &)
        {
            diagnostic = "cannot enumerate fixed mesh source directories";
            return false;
        }

        if (out.empty())
        {
            diagnostic = "fixed mesh source directory is empty: "
                + dirs[0].string();
            return false;
        }
        return true;
    }

    static bool FixedMeshCatalogHas(
        const std::vector<std::string> &catalog, const std::string &glsl)
    {
        // 生产文本就是形状 master 本身，逐字节比对，不再有需要剥离的溯源注释：
        // options 摘要已经从文本里搬出去，改为显式进 stage 键的 compile_inputs_hash
        // （见 ShaderStageKey.h）。文本一旦掺进非 GLSL 的行，这里的比对就该报错，
        // 而不是宽容地剥掉。
        std::string body = glsl;

        auto trim_tail = [](std::string &text)
        {
            while (!text.empty())
            {
                const char last = text.back();
                if (last != '\n' && last != '\r' && last != ' ' && last != '\t')
                    break;
                text.pop_back();
            }
        };
        trim_tail(body);
        if (body.empty())
            return false;

        for (const std::string &text : catalog)
        {
            std::string normalized = text;
            trim_tail(normalized);
            if (normalized == body)
                return true;
        }
        return false;
    }


    // ═════════════════════════════════════════════════════════════════════════
    // DH.domain-backend-equivalence
    //
    // 领域后端产物的**冻结基线**（反射文本 + SPV 体积）。
    //
    // 历史上这条用例是「生成器 vs 领域后端」的现场比对（阶段 4）。生成器在
    // 阶段 5.2 被物理删除之后就没有第二条路径可现场比了，继续保留只会变成
    // 同一份输出的自证 —— 因此判据降级为「与仓库内 golden 逐字对齐」，即
    // 15 个形状的变更探测器：任何领域主着色器、options 发射、include 接线的
    // 漂移都会在这里现形，并精确指出是哪个形状变了。
    //
    // 为什么不能只靠 scripts/ci/verify-domain-master-shaders.ps1：
    //   那个脚本证明的是「主着色器 + frag_options.glsl（由 DomainOptionsRegression 写出）
    //   能编译」。它绕开了引擎：options 是那个工具自己 emit 的，主着色器是脚本
    //   手选的。**接线**（后端选了哪份文件、options 里的字段从哪来、include 根
    //   怎么给）完全没有被它覆盖。这里补的就是这一段。
    //
    // 判据（与脚本一致，别换成逐字节比较）：
    //   · SPIR-V 逐字节比较无效——同一份源码两次编译就差几十字节（内嵌内容哈希）；
    //   · 比 reflection 文本（稳定）+ SPV 字节数（抓 reflection 看不见的多余声明）。
    //     编译器/插件升级导致字节数变化时，golden 会精确指出是哪个形状变了，
    //     按提示重录并人工确认差异即可。
    //
    // 本用例**不**做 mesh ↔ fragment varying 链接校验（见 ③ 的说明）。
    //
    // 防"自证"：request 不带 artifact store（默认 nullptr）⇒ 不碰磁盘缓存，
    // 否则比到的就不是本次编译。
    // ═════════════════════════════════════════════════════════════════════════

    struct DomainBackendEquivShape
    {
        const char *label;
        MaterialDomain domain;
        ShaderProgramPurpose purpose;
        bool alpha_test;
        // 领域内可选的表面模块形态（配方唯一能自选的模块槽）：Sky 的
        // sky_minimal / sky_cube 差别只在这里，golden 的反射形状随之不同。
        const char *surface_module_name = nullptr;
    };

    /// 等价矩阵：每个形状 = 「某个材质定义 + 某个 pass」的编译单元。
    /// 与前缀无关：条目顺序被 `DH.domain-backend-cache` 按下标引用，**不要重排**，
    /// 新增条目一律追加到末尾。
    static const DomainBackendEquivShape kDomainBackendEquivShapes[] =
    {
        { "fwd",          MaterialDomain::StandardOpaque, ShaderProgramPurpose::ForwardColor, false },
        { "fwd-alpha",    MaterialDomain::StandardOpaque, ShaderProgramPurpose::ForwardColor, true  },
        { "shadow",       MaterialDomain::StandardOpaque, ShaderProgramPurpose::ShadowDepth,  false },
        { "shadow-alpha", MaterialDomain::StandardOpaque, ShaderProgramPurpose::ShadowDepth,  true  },
        { "sky",          MaterialDomain::Sky,            ShaderProgramPurpose::ForwardColor, false,
          "surface_sky_cube" },
        { "unlit-tex",    MaterialDomain::UnlitTexture,   ShaderProgramPurpose::ForwardColor, false },
        { "vert-color",   MaterialDomain::VertexColor,    ShaderProgramPurpose::ForwardColor, false },
        { "vert-lum",     MaterialDomain::VertexLuminance, ShaderProgramPurpose::ForwardColor, false },
        { "vert-pal",     MaterialDomain::VertexPalette,  ShaderProgramPurpose::ForwardColor, false },
        { "unlit-solid",  MaterialDomain::UnlitSolid,     ShaderProgramPurpose::ForwardColor, false },
        { "checker-2d",   MaterialDomain::Checker2D,      ShaderProgramPurpose::ForwardColor, false },
        { "checker-3d",   MaterialDomain::Checker3D,      ShaderProgramPurpose::ForwardColor, false },
        { "err-check-3d", MaterialDomain::ErrorChecker3D, ShaderProgramPurpose::ForwardColor, false },
        { "text-gpu",     MaterialDomain::Text2D,         ShaderProgramPurpose::ForwardColor, false },
        { "debug-normal", MaterialDomain::DebugNormal,    ShaderProgramPurpose::ForwardColor, false },
        // 尾部追加（顺序被 DH.domain-backend-cache 按下标引用）：原生线图元领域。
        { "vert-pal-lines", MaterialDomain::VertexPaletteLines, ShaderProgramPurpose::ForwardColor, false },
    };

    /// reflection 的稳定文本形式。只取跨编译器稳定的字段，不含任何指令偏移。
    static std::string FormatDomainEquivReflection(const ShaderFlatView &view)
    {
        std::string text;
        char line[192];

        std::snprintf(line, sizeof(line), "entrypoint=%s\n",
                      view.entrypoint ? view.entrypoint : "");
        text += line;

        std::snprintf(line, sizeof(line), "inputs=%u outputs=%u push_constants=%u subpass=%u\n",
                      view.input_count, view.output_count,
                      view.push_constant_count, view.subpass_input_count);
        text += line;

        for (uint32_t i = 0; i < view.input_count; ++i)
        {
            const SFAttribute &a = view.inputs[i];
            std::snprintf(line, sizeof(line), "in  loc=%u name=%s base=%u vec=%u col=%u\n",
                          static_cast<unsigned>(a.location), a.name,
                          static_cast<unsigned>(a.basetype),
                          static_cast<unsigned>(a.vec_size),
                          static_cast<unsigned>(a.col_count));
            text += line;
        }

        for (uint32_t i = 0; i < view.output_count; ++i)
        {
            const SFAttribute &a = view.outputs[i];
            std::snprintf(line, sizeof(line), "out loc=%u name=%s base=%u vec=%u col=%u\n",
                          static_cast<unsigned>(a.location), a.name,
                          static_cast<unsigned>(a.basetype),
                          static_cast<unsigned>(a.vec_size),
                          static_cast<unsigned>(a.col_count));
            text += line;
        }

        for (uint32_t t = 0; t < SF_DESCRIPTOR_TYPE_COUNT; ++t)
        {
            for (uint32_t i = 0; i < view.resource_count[t]; ++i)
            {
                const SFDescriptor &d = view.resources[t][i];
                std::snprintf(line, sizeof(line), "res type=%u set=%u binding=%u array=%u name=%s\n",
                              t,
                              static_cast<unsigned>(d.set),
                              static_cast<unsigned>(d.binding),
                              static_cast<unsigned>(d.array_size),
                              d.name);
                text += line;
            }
        }

        for (uint32_t i = 0; i < view.push_constant_count; ++i)
        {
            const SFPushConstant &pc = view.push_constants[i];
            std::snprintf(line, sizeof(line), "pc  offset=%u size=%u name=%s\n",
                          pc.offset, pc.size, pc.name);
            text += line;
        }

        return text;
    }

    /// 两条路径共用的构建请求。几何格式由调用方持有（request 只存指针）。
    static bool BuildDomainEquivRequest(
        const DomainBackendEquivShape &shape,
        ResolvedMaterial &definition,
        GeometryVertexFormat &geometry,
        MaterialBuildRequest &request,
        std::string &diagnostic)
    {
        // 位置格式由领域的**规范顶点节点配置**决定，不是常量 VF_V3F：
        // `Vec2Position` 领域（UnlitTexture / Checker2D 等 2D 贴面）要求两个分量的
        // 位置流，给 VF_V3F 会让 `ResolveVertexABI` 直接失败。
        VkFormat position_format = VF_V3F;
        {
            VertexShaderNodeConfig canonical;
            if (GetMaterialDomainCanonicalVertexNodeConfig(
                    definition.domain, canonical)
             && canonical.input == VertexInputMode::Vec2Position)
                position_format = VF_V2F;
        }
        if (!geometry.Add(VertexSemantic::Position, position_format, 0, 0))
        {
            diagnostic = "geometry.Add(Position) failed";
            return false;
        }

        // 其余顶点语义**由领域契约推导**，不再手写「有 NTB 就加 UV0+Normal」。
        // 领域契约（`required_vertex_semantic_mask`）是领域材质顶点前提的单一真源，
        // 手抄一遍只会给「抄漏」留机会：漏了 UV0，`ResolveVertexABI` 直接失败
        // （实测 UnlitTexture 就是这么挂的），而不是给出清晰的诊断。
        {
            hgl::ValueArray<GlslModuleSemanticRequirement> requirements;
            if (!GetMaterialDomainVertexSemanticRequirements(
                    definition.domain, requirements))
            {
                diagnostic = "GetMaterialDomainVertexSemanticRequirements failed";
                return false;
            }

            struct SemanticFormat
            {
                VertexSemantic semantic;
                VkFormat format;
            };
            static const SemanticFormat kExtraSemantics[] =
            {
                { VertexSemantic::TexCoord,    VF_V2F },
                { VertexSemantic::Normal,      VF_V3F },
                { VertexSemantic::Tangent,     VF_V3F },
                { VertexSemantic::Bitangent,   VF_V3F },
                { VertexSemantic::Color,       VF_V4F },
                { VertexSemantic::TransformID, VF_V1U },
                { VertexSemantic::Size,        VF_V1F },
                { VertexSemantic::Luminance,   VF_V1F },
            };

            for (const SemanticFormat &entry : kExtraSemantics)
            {
                const GlslModuleSemantic wanted =
                    GetGlslModuleSemanticFromVertexSemantic(entry.semantic);
                if (wanted == GlslModuleSemantic::Unknown)
                    continue;

                bool required = false;
                for (int i = 0; i < requirements.GetCount(); ++i)
                {
                    if (requirements[i].semantic == wanted)
                    {
                        required = true;
                        break;
                    }
                }

                // Color 的格式取决于**顶点色来源**：调色板查表形态传的是 1 字节索引，
                // 不是 RGBA 颜色。判据取自合成后的 varyings（契约驱动），不按领域名，
                // 否则新增同类领域时这里会静默沿用 RGBA 格式。
                const VkFormat format =
                    entry.semantic == VertexSemantic::Color
                     && definition.vertex_varying.emit_vertex_color_from_palette
                        ? VK_FORMAT_R8_UINT
                        : entry.format;

                if (required && !geometry.Add(entry.semantic, format, 0, 0))
                {
                    diagnostic = "geometry.Add(contract semantic) failed";
                    return false;
                }
            }
        }

        request.recipe.domain = definition.domain;
        if (shape.surface_module_name)
            request.recipe.surface_module_name = shape.surface_module_name;
        request.recipe.render_state_overrides.has_alpha_test = shape.alpha_test;
        request.recipe.render_state_overrides.alpha_test = shape.alpha_test;
        request.recipe.render_state_overrides.has_alpha_cutoff = shape.alpha_test;
        request.recipe.render_state_overrides.alpha_cutoff = 0.5f;
        request.geometry_vertex_format = &geometry;
        request.shader_program_purpose = shape.purpose;

        // 配方填齐后**重新合成**定义：`shape.surface_module_name` 是在首次合成
        // 之后才写进 `request.recipe` 的，定义必须随之刷新。（历史上这里还有第二个
        // 理由——`definition.surface_module_name` 借用配方指针、悬空——那个已经
        // 修掉了：定义自持字符串，生命周期不再耦合配方。）
        if (!ResolveMaterialFromRecipe(request.recipe, definition))
        {
            diagnostic = "ResolveMaterialFromRecipe(request.recipe) failed";
            return false;
        }

        // 模板选择**必须**走引擎的路由器：它按「深度？/有 NTB？/有材质源？」
        // 选模板与 profile，并顺带挂上材质模块根。早期这里手写过一份等价
        // 分支，Sky 立刻炸了（SkyCube 没有 NTB provider，路由到 ForwardUnlit
        // 而不是 ForwardLitShadowedAO）——不要再复制这些规则。
        if (!SelectScenePassProfileRequest(
                definition, shape.purpose, request.recipe,
                request.render_template_request, nullptr))
        {
            diagnostic = "SelectScenePassProfileRequest failed";
            return false;
        }

        // 路由器只负责「选哪个模板 + 哪个 profile」；材质自身的能力
        // （surface / ntb / shadow … 槽）要靠这一步搬进请求，否则
        // `ValidatePassProfileRequest` 会以「槽缺失」拒绝构建。
        if (!AppendMaterialPassProfileRoots(
                definition, request.render_template_request))
        {
            diagnostic = "AppendMaterialPassProfileRoots failed";
            return false;
        }

        return true;
    }



    struct CacheIdentityEmitShape
    {
        const char *label;
        MaterialDomain domain;
        const GeometryVertexFormat *geometry;
        PrimitiveType primitive_type = PrimitiveType::Triangles;
    };

    static const CacheIdentityEmitShape kCacheIdentityEmitShapes[] =
    {
        { "cache-lit-abi-a",   MaterialDomain::StandardOpaque, &kCacheIdentityLitGeometryA },
        { "cache-lit-abi-b",   MaterialDomain::StandardOpaque, &kCacheIdentityLitGeometryB },
        { "cache-lit-abi-std", MaterialDomain::StandardOpaque, &kCacheIdentityLitGeometryStandard },
        { "cache-color",       MaterialDomain::VertexColor,    &kCacheIdentityColorGeometry },
        { "load-geometry-wire", MaterialDomain::UnlitSolid,
          &kCacheIdentityPureColorGeometry, PrimitiveType::Lines },
        { "plane-grid-luminance-wire", MaterialDomain::VertexLuminance,
          &kCacheIdentityLuminanceGeometry, PrimitiveType::Lines },
    };

    /// 网格主着色器文本必须是「纯 GLSL」。
    ///
    /// options 的差异不再靠文本尾部的一行溯源注释传递，而是显式进 stage 键的
    /// `compile_inputs_hash`（见 `ShaderStageKey.h`）。于是这里只需要守一件事：
    /// 生产侧没有把摘要重新塞回被编译的文本。
    ///
    /// 为什么这条要单独断言：`FixedMeshCatalogHas` 现在逐字节比对形状 master，
    /// 文本一旦掺进非 GLSL 行，覆盖率会整片报「缺源」——症状像是「形状 master 丢了」，
    /// 实际根因是有人把注释契约加了回来。而历史上的两种标记形态
    /// （`// mesh-options-digest`、`// domain-options-key`）正是这种返祖的指纹。
    ///
    /// 返回空串 = 干净；否则返回可直接落进诊断的说明。
    static std::string CheckMeshTextIsPure(const std::string &final_glsl)
    {
        static const char *const kLegacyMarkers[] =
        {
            "// mesh-options-digest",
            "// domain-options-key",
        };

        for (const char *marker : kLegacyMarkers)
        {
            if (final_glsl.find(marker) != std::string::npos)
                return std::string("网格主着色器文本里出现了历史上的溯源注释「")
                     + marker
                     + "」：options 摘要必须走 stage 键的 compile_inputs_hash，"
                       "不能写进被编译的文本";
        }

        return std::string();
    }

    static bool CheckOneFixedMeshSource(
        const char *label,
        const ShaderBuildContext &ctx,
        const std::vector<std::string> &catalog,
        int &covered,
        int &missing)
    {
        const ShaderCreateInfo *mesh = ctx.GetStageShader(ShaderStage::Mesh);
        if (!mesh)
        {
            std::fprintf(stdout, "[emit-fixed-mesh] %s: no mesh stage\n", label);
            ++missing;
            return false;
        }

        const std::string &glsl = mesh->GetFinalGLSL();
        if (glsl.empty())
        {
            std::fprintf(stdout, "[emit-fixed-mesh] %s: empty mesh GLSL\n", label);
            ++missing;
            return false;
        }

        // 文本纯净性先于覆盖率判定：掺了注释说明生产侧返祖了，此时「形状 master
        // 缺失」只是它的副作用（逐字节对不上），先报根因。
        // 计数器交给调用方统一 ++failed：这里不是「缺源」类问题。
        const std::string purity_problem = CheckMeshTextIsPure(glsl);
        if (!purity_problem.empty())
        {
            std::fprintf(stdout, "[emit-fixed-mesh] %s: %s\n",
                         label, purity_problem.c_str());
            return false;
        }

        if (FixedMeshCatalogHas(catalog, glsl))
        {
            ++covered;
            std::fprintf(stdout, "[covered] %s\n", label);
            return true;
        }

        ++missing;
        std::fprintf(stdout,
                     "[missing] %s: 网格阶段文本在 ShaderLibrary/topology 下没有"
                     "对应 master（形状 master 需人工按形状新增）\n",
                     label);
        return false;
    }
} // namespace gate

namespace gate
{
    // —— case 声明（实现见对应 Gate*.cpp；默认参数只在此声明）——
    int EmitFixedMeshCatalog();
    GateResult RunAddressOwnershipCase(const bool world_side);
    GateResult RunAuthoritativeMaterialCacheIdentityCase();
    GateResult RunBatchInfoStructParityCase();
    GateResult RunBindingMacroSingleSourceCase();
    GateResult RunCanonicalShaderContractCase();
    GateResult RunCharQuadWorkgroupContractCase();
    GateResult RunComposedStageInterfaceCase();
    GateResult RunComputeProgramCacheCase(
        const std::filesystem::path &store_root,
        const bool keep_artifacts = false);
    GateResult RunDepthVaryingMaskParityCase();
    GateResult RunDomainBackendCacheCase();
    GateResult RunDomainBackendEquivalenceCase();
    GateResult RunDrawTriangleVertexColor2DCase();
    GateResult RunEntityGPUStructParityCase();
    GateResult RunFallbackInferenceCase();
    GateResult RunFixedMeshVaryingInvariantsCase();
    GateResult RunGeneratedInterfaceReflectionCase();
    GateResult RunGlobalAddressesStructParityCase();
    GateResult RunGlobalSSBOBindingKeyCase();
    GateResult RunGlslModuleFileCase();
    GateResult RunGlslModuleMetadataValidationCase();
    GateResult RunLightRowStructParityCase();
    GateResult RunMaterialBDAFixedBindingsCase();
    GateResult RunMaterialDomainContractCase();
    GateResult RunMaterialDomainCoverageCase();
    GateResult RunMaterialFallbackDefinitionCase();
    GateResult RunMaterialIdentityCase();
    GateResult RunMaterialLibraryContractCase();
    GateResult RunMaterialOutputContractCase();
    GateResult RunMaterialRecipeParseDetailCase();
    GateResult RunMaterialSemanticABIParityCase();
    GateResult RunMaterialVertexABICharacterizationCase();
    GateResult RunMaterialVertexABIResolutionCase();
    GateResult RunMaterializationSharedInstanceCase();
    GateResult RunMeshDrawParamsHeadParityCase();
    GateResult RunModuleIncludeDeclarationContractCase();
    GateResult RunModuleInvariantsCase();
    GateResult RunPrimitiveVariantPurposeCase();
    GateResult RunProgramManifestCase();
    GateResult RunProgramManifestLookupCase(
        const std::filesystem::path &store_root,
        const bool keep_artifacts = false);
    GateResult RunProviderResourceManifestCase();
    GateResult RunResolvedMaterialRenderStateCase();
    GateResult RunResolvedStageCacheIdentityCase();
    GateResult RunSPVBufferReferenceReflectionCase();
    GateResult RunSPVReflectionCase();
    GateResult RunSamplerPresetLibraryCase();
    GateResult RunSamplerReferenceContractCase();
    GateResult RunShaderLibraryPathCase();
    GateResult RunShaderSemanticRegistryCase();
    GateResult RunTransformGraphCompositionCase();
    GateResult RunTransformGraphModelCase();
    GateResult RunUnifiedForwardSkeletonCase();
    GateResult RunUnifiedMaterialBaselineCase();
    GateResult RunUnifiedMaterialContractCase();
    GateResult RunUnifiedPureColorFragmentCase();
    GateResult RunVkEnumTextCase();
    GateResult RunWorldAddressesStructParityCase();
} // namespace gate
