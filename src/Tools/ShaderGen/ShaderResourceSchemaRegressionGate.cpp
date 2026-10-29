#include <hgl/mtl/MaterialDefinitionRegistry.h>
#include <hgl/mtl/MaterialDefinitionFile.h>
#include <hgl/mtl/BuiltinMaterialDefinitions.h>
#include <hgl/mtl/MaterialDomain.h>
#include <hgl/mtl/DomainShaderTemplate.h>
#include <hgl/mtl/DomainOptions.h>
#include <hgl/mtl/VkEnumText.h>
#include <hgl/mtl/SamplerPreset.h>
#include <hgl/mtl/ResolvedRenderTemplate.h>
#include <hgl/mtl/StageSPVCompiler.h>
#include <hgl/mtl/ShaderBuildContext.h>
#include <hgl/mtl/ShaderCreateInfo.h>
#include <hgl/mtl/ShaderLibraryPath.h>
#include <hgl/mtl/contract/ShaderGenProfileTargetVersion.h>
#include <hgl/mtl/ShaderArtifactStore.h>
#include <hgl/mtl/ShaderProgramArtifactBuilder.h>
#include <hgl/mtl/ShaderProgramManifest.h>
#include <hgl/mtl/ShaderComputeProgramManifest.h>
#include <hgl/mtl/ShaderProgramManifestKey.h>
#include <hgl/mtl/ShaderProgramManifestBackend.h>
#include <hgl/mtl/ShaderProgramBuildBackend.h>
#include <hgl/mtl/CanonicalShaderContract.h>
#include <hgl/mtl/ShaderSemanticRegistry.h>
#include <hgl/mtl/InterStageVaryingOptions.h>
#include <hgl/mtl/ShaderCodeModule.h>
#include <hgl/mtl/ShaderCodeModuleFile.h>
#include <hgl/mtl/ShaderCodeModuleRegistry.h>
#include <hgl/mtl/ShaderCodeModuleMetadata.h>
#include <hgl/mtl/ShaderCodeResourceManifest.h>
#include <hgl/mtl/SceneRenderTemplateResolver.h>
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
#include "VertexBuilderCommon.h"
#include "StageBuildContextTest.h"
#include <hgl/mtl/MaterialVertexVaryingConfig.h>
#include <hgl/mtl/MeshShaderMode.h>
#include <hgl/mtl/ShaderProgramBuildBackend.h>
#include "ShaderProgramBuild/compile/ShaderProgramBuildBackendInstall.h"

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

namespace
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
        const MaterialDefinition &definition,
        const MaterialDefinitionBuildRequest &request,
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
    /// 都在这里改一行并留下理由，变更记录见 `RunShaderCodeModuleRegistryCase`。
    constexpr int kShaderCodeModuleCount = 84;

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
    /// 只有 `mesh/shape/`：三种几何模式（VertexPassthrough / LineQuad /
    /// CharQuad）的全部形状 master 都在这里，形状名即文件名。`mesh/fixed/`
    /// 那套哈希命名（`<shape_key>.mesh`）已随生成器残留一起删除——它是
    /// 「按顶点格式算形状键再查表」的遗物，形状数随格式表组合爆炸。
    static std::vector<std::pair<std::string, std::string>>
    LoadFixedMeshMasters()
    {
        std::vector<std::pair<std::string, std::string>> masters;
        for (const char *suffix : {"ShaderLibrary/mesh/shape"})
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
    /// `common/interstage_varying.glsl` 之后，同一批不变量只需检查**一份文件**，
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
            RepoRootPath("ShaderLibrary/common/interstage_varying.glsl"));
        if (source.empty())
        {
            result.diagnostics.emplace_back(
                "common/interstage_varying.glsl is missing or empty");
            return;
        }

        const std::string writer = ReadFileText(
            RepoRootPath("ShaderLibrary/common/interstage_varying_writer.glsl"));
        if (writer.empty())
        {
            result.diagnostics.emplace_back(
                "common/interstage_varying_writer.glsl is missing or empty");
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
                "common/interstage_varying.glsl 必须依次含"
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
                    result, std::string("common/interstage_varying.glsl: ")
                        + varying_macro + " 已接线，两段却都没有声明 "
                        + symbol);
                continue;
            }
            if (mesh_block.empty() || frag_block.empty())
            {
                AddCappedDiagnostic(
                    result, std::string("common/interstage_varying.glsl: ")
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
                    result, std::string("common/interstage_varying.glsl 网格段: ")
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
                    result, std::string("common/interstage_varying.glsl 片段段: ")
                        + varying_macro + " 的声明必须是 `layout(location="
                        + info->location_macro + ") "
                        + (flat ? "flat perprimitiveEXT " : "") + "in "
                        + expected_type + " " + symbol + ";`");
            }

            // ── 网格段声明了就必须真的写出去 ──
            if (!IsVaryingWritten(writer_text, symbol))
            {
                AddCappedDiagnostic(
                    result, std::string("common/interstage_varying_writer.glsl: ")
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
                    result, "common/interstage_varying.glsl 含死开关 "
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
    /// `common/interstage_varying.glsl` 之后，同一批不变量只剩一份检查对象，
    /// 见 AuditUnifiedInterStageVarying()。本函数现在只看主着色器自身的结构：
    ///
    /// ① 拓扑契约 `layout(triangles, max_vertices = N, max_primitives = M)` 是字面量
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
            if (text.find("#include \"common/interstage_varying.glsl\"")
                == std::string::npos)
            {
                AddCappedDiagnostic(
                    result, name + ": 形状 master 必须 include"
                                  " common/interstage_varying.glsl（接口声明"
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
                const size_t layout = text.find("layout(triangles, max_vertices =");
                if (layout == std::string::npos)
                {
                    AddCappedDiagnostic(
                        result, name + ": 缺少 layout(triangles, ...) 输出声明");
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
                text.find("#include \"vertex/s1_text_char_quad.glsl\"")
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

    static GateResult RunMaterialVertexABICharacterizationCase()
    {
        GateResult result;
        result.name = "K.material-vertex-abi-characterization";

        const auto check_entries = [&](const char *name,
                                       const GeometryVertexFormat &geometry,
                                       const vertex_builder_common::VertexSemanticDecl *decls,
                                       const uint32_t decl_count,
                                       const std::vector<SerializedVertexEntry> &expected)
        {
            const vertex_builder_common::VertexBuildInput input{
                PrimitiveType::Triangles, &geometry, decls, decl_count
            };
            const auto actual = vertex_builder_common::BuildVertexEntries(input);
            std::string diagnostic;
            if (!CheckVertexEntries(actual, expected, diagnostic))
                result.diagnostics.emplace_back(std::string(name) + ": " + diagnostic);
        };

        // Position-only geometry is the baseline for both 2D and 3D paths.
        {
            const GeometryVertexFormat geometry{{VertexSemantic::Position, VF_V3F}};
            const vertex_builder_common::VertexSemanticDecl decls[] = {
                {VertexSemantic::Position, VF_V3F}
            };
            check_entries("position-only", geometry, decls, 1,
                {{VF_V3F, VertexSemantic::Position}});
        }

        // UV and color are independently resolved from Geometry, not by location.
        {
            const GeometryVertexFormat geometry{
                {VertexSemantic::Position, VF_V3F},
                {VertexSemantic::TexCoord, VF_V2HF},
                {VertexSemantic::Color, VF_V4UN8}
            };
            const vertex_builder_common::VertexSemanticDecl decls[] = {
                {VertexSemantic::Position, VF_V3F},
                {VertexSemantic::TexCoord, VF_V2F},
                {VertexSemantic::Color, VF_V4F}
            };
            check_entries("uv-and-color", geometry, decls, 3,
                {{VF_V3F, VertexSemantic::Position},
                 {VF_V2HF, VertexSemantic::TexCoord},
                 {VF_V4UN8, VertexSemantic::Color}});
        }

        // Normal providers may expose only Normal, Normal+Tangent, or full NTB.
        {
            const GeometryVertexFormat geometry{
                {VertexSemantic::Position, VF_V3F},
                {VertexSemantic::Normal, VF_V3F}
            };
            const vertex_builder_common::VertexSemanticDecl decls[] = {
                {VertexSemantic::Position, VF_V3F},
                {VertexSemantic::Normal, VF_V3F}
            };
            check_entries("normal-only", geometry, decls, 2,
                {{VF_V3F, VertexSemantic::Position},
                 {VF_V3F, VertexSemantic::Normal}});
        }
        {
            const GeometryVertexFormat geometry{
                {VertexSemantic::Position, VF_V3F},
                {VertexSemantic::Normal, VF_V3F},
                {VertexSemantic::Tangent, VF_V3HF}
            };
            const vertex_builder_common::VertexSemanticDecl decls[] = {
                {VertexSemantic::Position, VF_V3F},
                {VertexSemantic::Normal, VF_V3F},
                {VertexSemantic::Tangent, VF_V3F}
            };
            check_entries("normal-and-tangent", geometry, decls, 3,
                {{VF_V3F, VertexSemantic::Position},
                 {VF_V3F, VertexSemantic::Normal},
                 {VF_V3HF, VertexSemantic::Tangent}});
        }
        {
            const GeometryVertexFormat geometry{
                {VertexSemantic::Position, VF_V3F},
                {VertexSemantic::Normal, VF_V3F},
                {VertexSemantic::Tangent, VF_V3F},
                {VertexSemantic::Bitangent, VF_V3F}
            };
            const vertex_builder_common::VertexSemanticDecl decls[] = {
                {VertexSemantic::Position, VF_V3F},
                {VertexSemantic::Normal, VF_V3F},
                {VertexSemantic::Tangent, VF_V3F},
                {VertexSemantic::Bitangent, VF_V3F}
            };
            check_entries("full-ntb", geometry, decls, 4,
                {{VF_V3F, VertexSemantic::Position},
                 {VF_V3F, VertexSemantic::Normal},
                 {VF_V3F, VertexSemantic::Tangent},
                 {VF_V3F, VertexSemantic::Bitangent}});
        }

        // Packed normal formats remain a Geometry-owned format and preserve
        // their ABI identity for the legacy path.
        {
            const GeometryVertexFormat geometry{
                {VertexSemantic::Position, VF_V3F},
                {VertexSemantic::Normal, PF_A2BGR10UN, 4}
            };
            const vertex_builder_common::VertexSemanticDecl decls[] = {
                {VertexSemantic::Position, VF_V3F},
                {VertexSemantic::Normal, VF_V3F}
            };
            check_entries("packed-normal", geometry, decls, 2,
                {{VF_V3F, VertexSemantic::Position},
                 {PF_A2BGR10UN, VertexSemantic::Normal}});
        }

        // Multiple color-related attributes must remain independently
        // addressable until the provider graph replaces this legacy ABI.
        {
            const GeometryVertexFormat geometry{
                {VertexSemantic::Position, VF_V3F},
                {VertexSemantic::Color, VF_V4UN8},
                {VertexSemantic::Luminance, VF_V1UN8}
            };
            const vertex_builder_common::VertexSemanticDecl decls[] = {
                {VertexSemantic::Position, VF_V3F},
                {VertexSemantic::Color, VF_V4F},
                {VertexSemantic::Luminance, VF_V1F}
            };
            check_entries("multi-color-attributes", geometry, decls, 3,
                {{VF_V3F, VertexSemantic::Position},
                 {VF_V4UN8, VertexSemantic::Color},
                 {VF_V1UN8, VertexSemantic::Luminance}});
        }

        // Equivalent floating-point formats resolve to their own Geometry
        // formats while keeping the same semantic declaration shape.
        {
            const GeometryVertexFormat rg16_geometry{{VertexSemantic::Position, VF_V2HF}};
            const GeometryVertexFormat rg32_geometry{{VertexSemantic::Position, VF_V2F}};
            const vertex_builder_common::VertexSemanticDecl decls[] = {
                {VertexSemantic::Position, VF_V3F}
            };
            const vertex_builder_common::VertexBuildInput rg16_input{
                PrimitiveType::Triangles, &rg16_geometry, decls, 1
            };
            const vertex_builder_common::VertexBuildInput rg32_input{
                PrimitiveType::Triangles, &rg32_geometry, decls, 1
            };
            const auto rg16_entries = vertex_builder_common::BuildVertexEntries(rg16_input);
            const auto rg32_entries = vertex_builder_common::BuildVertexEntries(rg32_input);
            if (rg16_entries.size() != rg32_entries.size()
             || rg16_entries.size() != 1
             || rg16_entries[0].semantic != rg32_entries[0].semantic
             || rg16_entries[0].format != VF_V2HF
             || rg32_entries[0].format != VF_V2F)
            {
                result.diagnostics.emplace_back("equivalent-format ABI shape mismatch");
            }
        }

        // Every special-purpose file definition must expose the legacy
        // position-first contract while Phase 4 is introduced incrementally.
        static const char *material_ids[] = {
            "builtin/pure_color",
            "builtin/checkerboard_2d",
            "builtin/checkerboard_3d",
            "builtin/error_checkerboard_3d",
            "builtin/text_gpu",
            "builtin/text_gpu_bitmap"
        };
        for (const char *id : material_ids)
        {
            MaterialDefinition definition{};
            if (!TryGetMaterialDefinitionByID(id, definition))
            {
                result.diagnostics.emplace_back(std::string("missing file definition: ") + id);
                continue;
            }
            if (definition.vertex_semantic_requirements.IsEmpty())
            {
                result.diagnostics.emplace_back(std::string("invalid file vertex ABI: ") + id);
            }
        }

        result.passed = result.diagnostics.empty();
        return result;
    }

    static GateResult RunMaterialSemanticABIParityCase()
    {
        GateResult result;
        result.name = "L.material-semantic-abi-parity";

        static const char *definition_ids[] = {
            "builtin/pure_color",
            "builtin/text_gpu",
            "builtin/text_gpu_bitmap",
            "builtin/checkerboard_2d",
            "builtin/checkerboard_3d",
            "builtin/error_checkerboard_3d"
        };

        for (const char *id : definition_ids)
        {
            MaterialDefinition definition{};
            if (!TryGetMaterialDefinitionByID(id, definition))
            {
                result.diagnostics.emplace_back(std::string("missing migrated definition: ") + id);
                continue;
            }

            if (definition.vertex_provider_policy != MaterialVertexProviderPolicy::GeometryOnly)
            {
                result.diagnostics.emplace_back(std::string("unexpected provider policy: ") + id);
                continue;
            }

            if (definition.vertex_semantic_requirements.IsEmpty())
                result.diagnostics.emplace_back(
                    std::string("empty semantic-only ABI: ") + id);
        }

        result.passed = result.diagnostics.empty();
        return result;
    }

    /// 深度/阴影 purpose 的 varying 裁剪必须完全由 coverage.required_semantics 决定。
    ///
    /// 反证（历史缺陷）：裁剪按语义逐项手写白名单时，StyleID / WorldPosition /
    /// WorldNormal / FragDirection 永远被清零 —— 即使 coverage 在要求它们。
    /// 生成的 shadow program 于是满足不了自己的 coverage 契约且不报错：
    /// FS 侧 si 字段恒为零值，alpha 求值静默走错分支。
    static GateResult RunDepthVaryingMaskParityCase()
    {
        GateResult result;
        result.name = "L0.depth-varying-mask-parity";

        // ── ① 定义表对语义枚举的覆盖（新增语义未登记 = 正反两向同时漏）──
        // WorldTangent / WorldBinormal 是材质表面内部量，不存在跨阶段字段。
        for (hgl::uint32 value = 1;
             value < static_cast<hgl::uint32>(InterStageSemantic::RANGE_SIZE);
             ++value)
        {
            const InterStageSemantic semantic =
                static_cast<InterStageSemantic>(value);
            if (semantic == InterStageSemantic::WorldTangent
             || semantic == InterStageSemantic::WorldBinormal)
                continue;

            bool covered = false;
            for (const MaterialVaryingSemanticBinding &binding :
                    kMaterialVaryingSemanticBindings)
            {
                if (binding.semantic == semantic)
                {
                    covered = true;
                    break;
                }
            }
            if (!covered)
                result.diagnostics.emplace_back(
                    "varying/semantic 定义表漏登记语义 value="
                    + std::to_string(value));
        }

        // ── ② 声明 → 掩码（正向）：两条入口必须同源 ──
        MaterialVertexVaryingConfig declared{};
        declared.emit_data_index_id = true;
        declared.emit_world_pos = true;
        declared.emit_world_normal = true;
        declared.emit_uv0 = true;
        declared.emit_vertex_color = true;
        declared.emit_vertex_color_from_palette = true;
        declared.emit_frag_direction = true;
        declared.emit_luminance = true;
        declared.emit_style_id = true;
        declared.use_transform_id_attr = true;

        const InterStageSemanticMask declared_mask =
            GetInterStageSemanticMask(InterStageSemantic::DataIndexID)
          | GetInterStageSemanticMask(InterStageSemantic::WorldPosition)
          | GetInterStageSemanticMask(InterStageSemantic::WorldNormal)
          | GetInterStageSemanticMask(InterStageSemantic::UV0)
          | GetInterStageSemanticMask(InterStageSemantic::Color)
          | GetInterStageSemanticMask(InterStageSemantic::FragDirection)
          | GetInterStageSemanticMask(InterStageSemantic::Luminance)
          | GetInterStageSemanticMask(InterStageSemantic::StyleID);
        if (BuildMaterialInterStageSemanticMaskFromVarying(declared)
                != declared_mask
         || GetMaterialInterStageSemanticMask(declared) != declared_mask)
        {
            result.diagnostics.emplace_back(
                "varying → 掩码 与登记表不一致（正反两向不同源）");
        }

        MaterialDefinition definition{};
        definition.vertex_varying = declared;

        // ── ③ 掩码裁剪：要什么留什么，多字段语义由定义自己挑字段 ──
        MaterialCoverageContract masked_coverage{};
        masked_coverage.mode = MaterialCoverageMode::AlphaTest;
        masked_coverage.requires_alpha_evaluation = true;
        masked_coverage.required_semantics =
            GetInterStageSemanticMask(InterStageSemantic::WorldNormal)
          | GetInterStageSemanticMask(InterStageSemantic::StyleID)
          | GetInterStageSemanticMask(InterStageSemantic::DataIndexID)
          | GetInterStageSemanticMask(InterStageSemantic::Color);

        const MaterialVertexVaryingConfig shadow =
            ResolveMaterialVertexVaryingConfig(
                definition, ShaderProgramPurpose::ShadowDepth,
                masked_coverage);
        if (!shadow.emit_world_normal)
            result.diagnostics.emplace_back(
                "shadow varying 未恢复 WorldNormal（coverage 要求但取不到）");
        if (!shadow.emit_style_id)
            result.diagnostics.emplace_back(
                "shadow varying 未恢复 StyleID（coverage 要求但取不到）");
        if (!shadow.emit_data_index_id)
            result.diagnostics.emplace_back(
                "shadow varying 未恢复 DataIndexID（coverage 要求但取不到）");
        if (!shadow.emit_vertex_color
         || !shadow.emit_vertex_color_from_palette)
            result.diagnostics.emplace_back(
                "Color 语义的多个输出字段被替定义挑选（定义声明了两个，结果缺一个）");
        if (shadow.emit_world_pos || shadow.emit_uv0
         || shadow.emit_frag_direction || shadow.emit_luminance)
            result.diagnostics.emplace_back(
                "shadow varying 保留了 coverage 未要求的语义");
        if (!shadow.use_transform_id_attr)
            result.diagnostics.emplace_back(
                "顶点属性选择 use_transform_id_attr 被跨阶段裁剪误伤");
        if (BuildMaterialInterStageSemanticMaskFromVarying(shadow)
                != masked_coverage.required_semantics)
        {
            result.diagnostics.emplace_back(
                "裁剪结果反推掩码 ≠ coverage.required_semantics");
        }

        // ── ④ 不评估 alpha ⇒ 一条跨阶段语义都不留（顶点属性选择仍保留）──
        MaterialCoverageContract opaque_coverage{};
        const MaterialVertexVaryingConfig depth_only =
            ResolveMaterialVertexVaryingConfig(
                definition, ShaderProgramPurpose::DepthOnly,
                opaque_coverage);
        if (BuildMaterialInterStageSemanticMaskFromVarying(depth_only) != 0)
            result.diagnostics.emplace_back(
                "不评估 alpha 的 depth program 仍带着跨阶段语义");
        if (!depth_only.use_transform_id_attr)
            result.diagnostics.emplace_back(
                "不评估 alpha 的 depth program 丢了顶点属性选择");

        // ── ⑤ 前向 purpose 不受裁剪影响（原样带出定义声明）──
        const MaterialVertexVaryingConfig forward =
            ResolveMaterialVertexVaryingConfig(
                definition, ShaderProgramPurpose::ForwardColor,
                opaque_coverage);
        if (BuildMaterialInterStageSemanticMaskFromVarying(forward)
                != declared_mask)
        {
            result.diagnostics.emplace_back(
                "前向 purpose 的 varying 被裁剪（只应作用于 depth/shadow）");
        }

        result.passed = result.diagnostics.empty();
        return result;
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

    /// CharQuad 的 mesh 工作组尺寸必须与 CPU dispatch 常量**逐位一致**。
    ///
    /// 反证（历史缺陷）：生成侧用 min(TOML 声明, 64) 再经设备能力二次钳制，
    /// 而 TextRenderPipeline 固定按 TEXT_CHARQUAD_MAX_INVOCATIONS 整除算
    /// DispatchMesh 组数、拿不到 shader 的 local_size_x。两者一旦分叉，
    /// 每个工作组只处理不足 64 个字符，尾部字符整段漏画且无任何报错。
    static GateResult RunCharQuadWorkgroupContractCase()
    {
        GateResult result;
        result.name = "L0.charquad-workgroup-contract";

        MaterialDefinition definition{};
        if (!TryGetMaterialDefinitionByID("builtin/text_gpu", definition))
        {
            result.diagnostics.emplace_back("missing definition: builtin/text_gpu");
            result.passed = false;
            return result;
        }

        const auto build = [](const contract::PhysicalDeviceProfileLite *profile,
                              const MaterialDefinition &def,
                              const char *mesh_source_override,
                              FixedFragmentSource *out_fixed = nullptr,
                              MaterialProgramBuildCapture *out_capture = nullptr)
            -> std::unique_ptr<ShaderBuildContext>
        {
            MaterialDefinitionBuildRequest request{};
            request.recipe.mtl_def_id = def.definition_id;
            request.shader_program_purpose = ShaderProgramPurpose::ForwardColor;
            // 只到 GLSL 文本为止：本用例断言的是发射文本里的 local_size_x，
            // 不需要（也不该依赖）SPV 编译。
            request.defer_finalize = true;

            RenderTemplateValidationDiagnostic template_diagnostic{};
            const SceneRenderTemplateProfile scene_profile =
                MakeForwardUnlitProfile();
            if (!ResolveSceneRenderTemplateRequest(
                    RenderTemplateID::ForwardUnlit,
                    hgl::graph::ShaderStage::Fragment,
                    scene_profile, request.render_template_request,
                    template_diagnostic)
             || !AppendMaterialRenderTemplateRoots(
                    def, request.render_template_request))
            {
                return std::unique_ptr<ShaderBuildContext>();
            }

            // 本用例断言的是**网格阶段**发射文本里的 local_size_x 与设备上限
            // 守卫，片段根本不参与。片段源照样按领域固定源填进请求——不填就
            // 会退回生成器的片段拼装链（阶段 5.2 后那条链不复存在）。
            FixedFragmentSource fixed;
            if (!ResolveFixedFragmentSource(def, request, fixed))
            {
                return std::unique_ptr<ShaderBuildContext>();
            }

            // 非空即绕过固定源目录，直接把给定文本当网格阶段源——⑤⑥⑦ 用它注入
            // 被篡改的拓扑行，验证装配期校验真的会拒绝。
            FixedStageInputs fixed_stage;
            fixed_stage.fragment_source = fixed.source.c_str();
            fixed_stage.fragment_interface_hash = fixed.interface_hash;
            fixed_stage.fragment_options_digest = fixed.options_digest;
            fixed_stage.mesh_source = mesh_source_override;

            if (out_fixed)
                *out_fixed = fixed;

            // 不编译 SPV：本用例只读网格阶段 GLSL 文本。
            return std::unique_ptr<ShaderBuildContext>(
                AssembleMaterialProgram(profile, request, def, out_capture,
                                        fixed_stage));
        };

        // ── ① 无 profile（无约束）：local_size_x 必须等于 CPU dispatch 常量 ──
        const auto plain = build(nullptr, definition, nullptr);
        if (!plain)
        {
            result.diagnostics.emplace_back(
                "CharQuad material build failed without device profile");
        }
        else
        {
            const ShaderCreateInfo *mesh =
                plain->GetStageShader(ShaderStage::Mesh);
            if (!mesh)
            {
                result.diagnostics.emplace_back(
                    "CharQuad material emitted no mesh stage");
            }
            else
            {
                const std::string &glsl = mesh->GetFinalGLSL();
                if (glsl.find("local_size_x = ")
                        == std::string::npos)
                {
                    result.diagnostics.emplace_back(
                        "CharQuad mesh stage emits no local_size_x");
                }
                else if (glsl.find(
                        "local_size_x = "
                        + std::to_string(
                            static_cast<unsigned>(
                                TEXT_CHARQUAD_MAX_INVOCATIONS))
                        + ")") == std::string::npos)
                {
                    result.diagnostics.emplace_back(
                        "CharQuad mesh stage local_size_x != "
                        "TEXT_CHARQUAD_MAX_INVOCATIONS（CPU dispatch 组数会算错）");
                }
            }
        }

        // ── ② 材质声明更小的线程数 ⇒ 必须构建失败，不得静默钳制 ──
        MaterialDefinition lowered = definition;
        lowered.mesh_shader_max_invocations =
            TEXT_CHARQUAD_MAX_INVOCATIONS / 2u;
        if (build(nullptr, lowered, nullptr))
        {
            result.diagnostics.emplace_back(
                "CharQuad 接受 mesh_shader_max_invocations < "
                "TEXT_CHARQUAD_MAX_INVOCATIONS（CPU 按常量派发会漏字）");
        }

        // ── ③ 设备能力上限低于契约 ⇒ 必须构建失败 ──
        contract::PhysicalDeviceProfileLite narrow{};
        narrow.api_version = contract::MakeVkVersion(1, 4);
        narrow.limits.max_mesh_work_group_size_x =
            TEXT_CHARQUAD_MAX_INVOCATIONS / 2u;
        if (build(&narrow, definition, nullptr))
        {
            result.diagnostics.emplace_back(
                "CharQuad 在设备上限 < TEXT_CHARQUAD_MAX_INVOCATIONS "
                "时仍生成了更小的 group size");
        }

        // ── ④ 设备上限高于契约 ⇒ 仍锁在契约值（不退让也不放大）──
        contract::PhysicalDeviceProfileLite wide{};
        wide.api_version = contract::MakeVkVersion(1, 4);
        wide.limits.max_mesh_work_group_size_x =
            TEXT_CHARQUAD_MAX_INVOCATIONS * 2u;
        wide.limits.max_mesh_output_vertices = 1024u;
        wide.limits.max_mesh_output_primitives = 512u;
        const auto wide_ctx = build(&wide, definition, nullptr);
        if (!wide_ctx)
        {
            result.diagnostics.emplace_back(
                "CharQuad 在宽裕设备能力下构建失败");
        }
        else
        {
            const ShaderCreateInfo *mesh =
                wide_ctx->GetStageShader(ShaderStage::Mesh);
            if (!mesh
             || mesh->GetFinalGLSL().find(
                    "local_size_x = "
                    + std::to_string(static_cast<unsigned>(
                        TEXT_CHARQUAD_MAX_INVOCATIONS))
                    + ")") == std::string::npos)
            {
                result.diagnostics.emplace_back(
                    "CharQuad 的 group size 随设备能力放大（契约必须锁死）");
            }
        }

        // ── ⑤⑥⑦ 固定源被改写 ⇒ 必须构建失败，不得照着文本编译 ──
        //
        // 6.2 之后 topology 三行是**外部数据**：形状键只哈希「模式 + invocation 数」，
        // 不哈希这三行，所以手改文件里的数字不会让键失配——文件照样被找到、照样编译。
        // 以下三条把「数据被改动」变成硬失败；否则缺陷会以「每组处理不足 / 输出容量
        // 不够」的形式静默漏画，正是本用例登记的那类历史问题，只是从代码搬到了数据。
        const ShaderCreateInfo *plain_mesh =
            plain ? plain->GetStageShader(ShaderStage::Mesh) : nullptr;
        const MeshModeCapacity charquad_capacity = ResolveMeshModeTopology(
            MeshShaderMode::CharQuad, TEXT_CHARQUAD_MAX_INVOCATIONS);

        if (!plain_mesh)
        {
            result.diagnostics.emplace_back(
                "CharQuad mesh stage unavailable: topology tampering not exercised");
        }
        else
        {
            const std::string baseline = plain_mesh->GetFinalGLSL();

            struct TopologyTamper
            {
                const char *label;
                std::string from;
                std::string to;
            };

            const TopologyTamper tampers[3] =
            {
                {
                    "local_size_x",
                    "local_size_x = "
                        + std::to_string(TEXT_CHARQUAD_MAX_INVOCATIONS),
                    "local_size_x = "
                        + std::to_string(TEXT_CHARQUAD_MAX_INVOCATIONS / 2u)
                },
                {
                    "max_vertices",
                    "max_vertices = "
                        + std::to_string(charquad_capacity.max_vertices),
                    "max_vertices = "
                        + std::to_string(charquad_capacity.max_vertices / 2u)
                },
                {
                    "max_primitives",
                    "max_primitives = "
                        + std::to_string(charquad_capacity.max_primitives),
                    "max_primitives = "
                        + std::to_string(charquad_capacity.max_primitives / 2u)
                },
            };

            for (const TopologyTamper &tamper : tampers)
            {
                std::string tampered = baseline;
                const size_t at = tampered.find(tamper.from);
                if (at == std::string::npos)
                {
                    result.diagnostics.emplace_back(
                        std::string("CharQuad 固定源里找不到可篡改的 ")
                        + tamper.label + " 声明");
                    continue;
                }
                tampered.replace(at, tamper.from.size(), tamper.to);

                if (build(nullptr, definition, tampered.c_str()))
                {
                    result.diagnostics.emplace_back(
                        std::string("CharQuad 接受了被改写的 ")
                        + tamper.label
                        + "（固定源拓扑与 CPU 契约分叉却不报错）");
                }
            }
        }

        // ── ⑧ 反射取证：驱动真正读到的那份 SPIR-V 里的执行模式 ──
        //
        // ①④ 只证明文本里有那串数字，⑤⑥⑦ 只证明装配期把文本解析回了数值并比对。
        // 这一条钉住第三段：编译产物里的 LocalSize / OutputVertices /
        // OutputPrimitivesEXT。三者任一分叉，CPU 按常量派发就与 GPU 实际执行规模不符。
        // 执行模式枚举按 SPIR-V 规范写死（LocalSize=17 / OutputVertices=26 /
        // OutputPrimitivesEXT=5270），与 `PM.compute-release-cache` 写死 112 同理：
        // 若两侧一起漂，本门才会失败。
        if (plain_mesh && !plain_mesh->GetFinalGLSL().empty())
        {
            // 网格主着色器 include `generated/common_options.glsl`（阶段间 varying
            // 开关，两 stage 共用一份）。裸 `CompileShader` 只有库根，解析不到这份
            // 生成文件；必须先把 options 补进片段的 options 根，再走生产的双根编译。
            FixedFragmentSource toggle_fixed;
            MaterialProgramBuildCapture toggle_capture{};
            std::unique_ptr<ShaderBuildContext> toggle_ctx =
                build(nullptr, definition, nullptr, &toggle_fixed, &toggle_capture);

            hgl::AnsiString compile_log;
            const uint32_t *spv_words = nullptr;
            uint32_t spv_size = 0u;
            if (!toggle_ctx)
            {
                compile_log = "material build failed (no options root)";
            }
            else if (!hgl::graph::mtl::WriteInterStageVaryingOptionsSource(
                         toggle_fixed.options_root,
                         toggle_capture.interstage_varying_options))
            {
                compile_log =
                    hgl::graph::mtl::GetLastDomainShaderBuildBackendError();
            }
            else if (!CompileFixedMeshSource(*toggle_ctx, toggle_fixed,
                                             compile_log))
            {
                // compile_log 已由后端写入
            }
            else
            {
                const ShaderCreateInfo *compiled =
                    toggle_ctx->GetStageShader(ShaderStage::Mesh);
                if (compiled && compiled->HasSPVData())
                {
                    spv_words = compiled->GetSPVData();
                    spv_size = compiled->GetSPVSize();
                }
            }

            if (!spv_words || spv_size == 0u)
            {
                result.diagnostics.emplace_back(
                    "CharQuad mesh stage failed to compile: "
                    + std::string(compile_log.c_str() ? compile_log.c_str() : ""));
            }
            else
            {
                hgl::ValueArray<uint8_t> storage;
                ShaderFlatView view{};
                if (!ReflectSPVData(
                        spv_words, spv_size,
                        static_cast<uint32_t>(VK_SHADER_STAGE_MESH_BIT_EXT),
                        "main", storage, view))
                {
                    result.diagnostics.emplace_back(
                        "CharQuad mesh stage SPIR-V reflection failed");
                }
                else
                {
                    struct ReflectedMode
                    {
                        const char *label;
                        uint32_t    expected;
                    };

                    // 17 = LocalSize / 26 = OutputVertices / 5270 = OutputPrimitivesEXT。
                    // 写死枚举与 `PM.compute-release-cache` 写死 112 同理：两侧一起漂
                    // 才会漏过本门。
                    const ReflectedMode modes[3] =
                    {
                        { "LocalSize", TEXT_CHARQUAD_MAX_INVOCATIONS },
                        { "OutputVertices", charquad_capacity.max_vertices },
                        { "OutputPrimitivesEXT", charquad_capacity.max_primitives },
                    };

                    for (uint32_t m = 0; m < 3u; ++m)
                    {
                        uint32_t actual = 0;
                        // 数组顺序与 modes[] 一致：0=LocalSize 走 331/LocalSizeId
                        // 回退，1/2 是字面量 `OpExecutionMode`。
                        const bool found = (m == 0u)
                            ? FindSPVLocalSizeX(view, actual)
                            : FindSPVExecutionModeLiteral(
                                  view, (m == 1u) ? 26u : 5270u, 0, actual);

                        if (!found)
                            result.diagnostics.emplace_back(
                                std::string("CharQuad mesh SPIR-V has no ")
                                + modes[m].label + " execution mode");
                        else if (actual != modes[m].expected)
                            result.diagnostics.emplace_back(
                                std::string("CharQuad mesh SPIR-V ")
                                + modes[m].label + " = " + std::to_string(actual)
                                + " != " + std::to_string(modes[m].expected)
                                + "（CPU 派发契约与实际执行规模分叉）");
                    }
                }
                // SPV 归 toggle_ctx 所有，此处不得释放。
            }
        }

        result.passed = result.diagnostics.empty();
        return result;
    }

    /// `DrawTriangle` 的真实形状：`VertexColor` 域 + 2 分量整数位置。
    ///
    /// 失败现场（用户运行 DrawTriangle 示例时）：
    /// ```
    /// [VertexNodeConfigResolver.h:30] 位置映射与顶点位置输入不匹配：
    ///   position_mapping=Passthrough3D 需要 Position 分量数 3，
    ///   而 Stage 1 输入模块提供 2 个分量（VertexColor）
    /// ```
    /// 根因是 `vertex_node_config` 有四层来源（request 覆盖 > recipe >
    /// definition > 默认），而 ABI 对账读的是 `definition` 里最弱的那一层。
    /// 阶段 4.4 用一行回写修掉了它（`plan.vertex_definition.vertex_node_config
    /// = plan.vertex_node_config;`）——本用例就是那行的看守：`VertexColor`
    /// 的 definition 是 `Passthrough3D`（3 分量），而 DrawTriangle 的 recipe
    /// 是 `Make2DNodeConfigOrtho`（2 分量），只有回写生效时两者才一致。
    ///
    /// 同时守第二件事：`VK_FORMAT_R16G16_SINT` 必须选到 4B packed int16x2 的
    /// `s1_position_vec2i.glsl`，不是 8B 浮点 `s1_position_vec2.glsl`——两者
    /// 都能编译通过，选错了整批顶点静默错位。
    static GateResult RunDrawTriangleVertexColor2DCase()
    {
        GateResult result;
        result.name = "L0.draw-triangle-vertex-color-2d";

        MaterialDefinition definition{};
        if (!TryGetMaterialDefinitionByID("VertexColor", definition))
        {
            result.diagnostics.emplace_back("missing definition: VertexColor");
            result.passed = false;
            return result;
        }

        // DrawTriangle 的几何：像素坐标是 int16x2（`VF_V2I16`），不是 float。
        GeometryVertexFormat geometry;
        if (!geometry.Add(VertexSemantic::Position, VF_V2I16, 0, 0)
         || !geometry.Add(VertexSemantic::Color, VF_V4F, 0, 0))
        {
            result.diagnostics.emplace_back("cannot build DrawTriangle geometry");
            result.passed = false;
            return result;
        }

        // 顶点解码 / 位置映射 / 投影全部落在 `generated/mesh_options.glsl` 里，
        // 网格主着色器文本对同形状是逐字节相同的。断言必须看 options 文本，
        // 否则「形状对了但顶点格式选错」会静默通过。
        hgl::AnsiString mesh_options_text;

        const auto build = [&geometry, &mesh_options_text](
                               const MaterialDefinition &def,
                               const VertexShaderNodeConfig &node_cfg,
                               const GeometryVertexFormat *gvf)
            -> std::unique_ptr<ShaderBuildContext>
        {
            mesh_options_text.Clear();

            MaterialDefinitionBuildRequest request{};
            request.recipe.mtl_def_id = def.definition_id;
            request.recipe.vertex_node_config = node_cfg;
            request.recipe.render_state_overrides.has_alpha_test = false;
            request.shader_program_purpose = ShaderProgramPurpose::ForwardColor;
            request.geometry_vertex_format = gvf;
            // 只到 GLSL 文本为止：本用例断言的是发射文本，不需要 SPV。
            request.defer_finalize = true;

            RenderTemplateValidationDiagnostic template_diagnostic{};
            const SceneRenderTemplateProfile scene_profile =
                MakeForwardUnlitProfile();
            if (!ResolveSceneRenderTemplateRequest(
                    RenderTemplateID::ForwardUnlit,
                    hgl::graph::ShaderStage::Fragment,
                    scene_profile, request.render_template_request,
                    template_diagnostic)
             || !AppendMaterialRenderTemplateRoots(
                    def, request.render_template_request))
                return std::unique_ptr<ShaderBuildContext>();

            FixedFragmentSource fixed;
            if (!ResolveFixedFragmentSource(def, request, fixed))
                return std::unique_ptr<ShaderBuildContext>();
            FixedStageInputs fixed_stage;
            fixed_stage.fragment_source = fixed.source.c_str();
            fixed_stage.fragment_interface_hash = fixed.interface_hash;
            fixed_stage.fragment_options_digest = fixed.options_digest;

            MaterialProgramBuildCapture capture{};
            std::unique_ptr<ShaderBuildContext> ctx(
                AssembleMaterialProgram(nullptr, request, def, &capture, fixed_stage));
            mesh_options_text = capture.mesh_options;
            return ctx;
        };

        const VertexShaderNodeConfig node_2d = Make2DNodeConfigOrtho(false);

        // ── ① recipe 的 2D 节点配置 + 2 分量位置 ⇒ 必须构建成功 ──
        // 回写若失效，ABI 会读 definition 的 Passthrough3D（3 分量）与 s1 的
        // 2 分量对不上，这里直接返回 null。
        const auto ctx = build(definition, node_2d, &geometry);
        if (!ctx)
        {
            result.diagnostics.emplace_back(
                "DrawTriangle 形状构建失败（recipe 的 2D 节点配置未回写到 ABI 求解用的定义）");
        }
        else
        {
            const ShaderCreateInfo *mesh = ctx->GetStageShader(ShaderStage::Mesh);
            if (!mesh)
            {
                result.diagnostics.emplace_back(
                    "DrawTriangle 形状没有网格阶段");
            }
            else
            {
                // 断言看 options 文本：顶点解码与映射都在那里，主着色器已不含这些。
                const std::string glsl = mesh_options_text.c_str()
                    ? std::string(mesh_options_text.c_str())
                    : std::string();

                // ── ② 位置解码模块必须是 packed int16x2 那一份 ──
                if (glsl.find("#include \"vertex/s1_position_vec2i.glsl\"")
                        == std::string::npos)
                {
                    result.diagnostics.emplace_back(
                        "R16G16_SINT 没有选到 vertex/s1_position_vec2i.glsl"
                        "（4B packed int16x2 与 8B float vec2 是两个 ABI）");
                }
                if (glsl.find("#include \"vertex/s1_position_vec2.glsl\"")
                        != std::string::npos
                 || glsl.find("#include \"vertex/s1_position_vec3.glsl\"")
                        != std::string::npos)
                {
                    result.diagnostics.emplace_back(
                        "位置解码模块指向浮点 vec2/vec3（与 R16G16_SINT 不匹配）");
                }

                // ── ③ 映射必须取 recipe 的 PixelToLocal，而不是 definition 的 Passthrough3D ──
                if (glsl.find("#include \"vertex/s2_pixel_to_local.glsl\"")
                        == std::string::npos)
                {
                    result.diagnostics.emplace_back(
                        "网格阶段没有使用 recipe 的 s2_pixel_to_local 映射");
                }
                if (glsl.find("#include \"vertex/s2_passthrough3d.glsl\"")
                        != std::string::npos)
                {
                    result.diagnostics.emplace_back(
                        "网格阶段用了 definition 的 Passthrough3D 映射（recipe 覆盖丢失）");
                }

                // ── ④ 该形状必须在形状 master 目录里有一份，且内容就是这份 GLSL ──
                // ShaderGen 删除后没有生成器兜底：目录里缺这一份，该域就没有
                // mesh 阶段可编译。按内容比对（而不是按形状键硬编码文件名），
                // 重新捕获时只需替换文件，门禁仍然有效。
                //
                // 形状主着色器只在 `mesh/shape/` 下（`mesh/fixed/` 已删除）。
                bool matched = false;
                {
                    // 生产文本就是形状 master 本身（options 摘要已迁出文本），
                    // 逐字节比对，不做任何剥离。
                    const std::string body = mesh->GetFinalGLSL();

                    const std::filesystem::path dirs[1] =
                    {
                        RepoRootPath("ShaderLibrary/mesh/shape")
                    };
                    try
                    {
                        for (const std::filesystem::path &fixed_dir : dirs)
                        {
                            if (matched
                             || !std::filesystem::exists(fixed_dir))
                                continue;
                            for (const auto &entry :
                                 std::filesystem::directory_iterator(fixed_dir))
                            {
                                if (!entry.is_regular_file()
                                 || entry.path().extension() != ".mesh")
                                    continue;
                                if (ReadFileText(entry.path().string()) == body)
                                {
                                    matched = true;
                                    break;
                                }
                            }
                        }
                    }
                    catch (const std::filesystem::filesystem_error &)
                    {
                        result.diagnostics.emplace_back(
                            "cannot enumerate ShaderLibrary/mesh/shape");
                    }
                }
                if (!matched)
                {
                    result.diagnostics.emplace_back(
                        "ShaderLibrary/mesh/shape 没有与该形状一致的 .mesh"
                        "（删除 ShaderGen 后该域将无 mesh 阶段可用）");
                }
            }
        }

        // ── ⑤ AutoInstance 形状：VF_V2F + Make2DNodeConfigNDC(true) ──
        GeometryVertexFormat autoinstance_geometry;
        if (!autoinstance_geometry.Add(VertexSemantic::Position, VF_V2F, 0, 0)
         || !autoinstance_geometry.Add(VertexSemantic::Color, VF_V4F, 0, 0))
        {
            result.diagnostics.emplace_back("cannot build AutoInstance geometry");
        }
        else
        {
            const VertexShaderNodeConfig autoinstance_node = Make2DNodeConfigNDC(true);
            const auto autoinstance_ctx = build(definition, autoinstance_node, &autoinstance_geometry);
            if (!autoinstance_ctx)
            {
                result.diagnostics.emplace_back("AutoInstance 形状构建失败");
            }
            else
            {
                const ShaderCreateInfo *mesh = autoinstance_ctx->GetStageShader(ShaderStage::Mesh);
                if (!mesh)
                    result.diagnostics.emplace_back("AutoInstance 没有网格阶段");
                else
                {
                    const std::string glsl = mesh_options_text.c_str()
                        ? std::string(mesh_options_text.c_str())
                        : std::string();
                    if (glsl.find("#include \"vertex/s1_position_vec2.glsl\"") == std::string::npos)
                        result.diagnostics.emplace_back("AutoInstance 没有选到 s1_position_vec2.glsl");
                    if (glsl.find("#include \"vertex/s2_ndc_lift.glsl\"") == std::string::npos)
                        result.diagnostics.emplace_back("AutoInstance 没有选到 s2_ndc_lift.glsl");
                    if (glsl.find("#include \"vertex/s3_l2w_only.glsl\"") == std::string::npos)
                        result.diagnostics.emplace_back("AutoInstance 没有选到 s3_l2w_only.glsl");
                }
            }
        }

        // ── ⑥ 2 分量几何 + definition 默认的 3D 节点配置 ⇒ 必须构建失败 ──
        // 这是「没有回写时会发生什么」的等价现场：两个所有者不匹配必须硬失败，
        // 不得产出一份编不过（或能编过但顶点错位）的 GLSL。
        if (build(definition, definition.vertex_node_config, &geometry))
        {
            result.diagnostics.emplace_back(
                "Passthrough3D + 2 分量位置几何被接受（分量数对账失效）");
        }

        // ── ⑦ 3 分量几何 + 2D 节点配置 ⇒ 同样必须失败 ──
        // 反方向：证明对账不是「只认 recipe」的单向放行。
        GeometryVertexFormat geometry_3d;
        if (!geometry_3d.Add(VertexSemantic::Position, VF_V3F, 0, 0)
         || !geometry_3d.Add(VertexSemantic::Color, VF_V4F, 0, 0))
        {
            result.diagnostics.emplace_back("cannot build 3D control geometry");
        }
        else if (build(definition, node_2d, &geometry_3d))
        {
            result.diagnostics.emplace_back(
                "PixelToLocal（2 分量）+ 3 分量位置几何被接受（分量数对账失效）");
        }

        result.passed = result.diagnostics.empty();
        return result;
    }

    static GateResult RunShaderSemanticRegistryCase()
    {
        GateResult result;
        result.name = "L0.shader-semantic-registry";


        ShaderSemanticRegistryValidationResult validation{};
        if (!ValidateInterStageSemanticRegistry(validation))
        {
            result.diagnostics.emplace_back(
                "semantic registry validation failed: error="
                + std::to_string(static_cast<hgl::uint32>(validation.error))
                + " first=" + std::to_string(validation.first_index)
                + " second=" + std::to_string(validation.second_index));
        }

        if (GetInterStageSemanticInfoCount()
                != static_cast<hgl::uint32>(InterStageSemantic::RANGE_SIZE))
        {
            result.diagnostics.emplace_back("semantic registry coverage mismatch");
        }

        const InterStageSemanticInfo *data_index =
            GetInterStageSemanticInfo(InterStageSemantic::DataIndexID);
        const InterStageSemanticInfo *color =
            GetInterStageSemanticInfo(InterStageSemantic::Color);
        if (!data_index
         || data_index->stable_location != 0
         || data_index->interpolation != InterStageInterpolation::Flat
         || !color
         || color->stable_location != 5)
        {
            result.diagnostics.emplace_back(
                "inter-stage stable ABI metadata mismatch");
        }

        // 生成器已物理删除：原来这里拿「生成器产出的 lit/color 顶点文本」当
        // varying ABI 的判据。现在稳定 ABI 的唯一真源就是上面这张注册表，
        // 而「库里的主着色器是否照着它发射」由 MV.fixed-mesh-varying-invariants
        // 检查三份拓扑 master —— 两边分工，避免一处失败两处报。

        result.passed = result.diagnostics.empty();
        return result;
    }


    static GateResult RunComposedStageInterfaceCase()
    {
        GateResult result;
        result.name = "P.composed-stage-interface";

        ShaderStageBuildContext vertex{};
        vertex.stage = ShaderStage::Mesh;
        vertex.outputs.Add({0, ShaderStageValueType::Vec3, 0, 0});
        vertex.outputs.Add({0, ShaderStageValueType::Vec2, 1, 0});
        ShaderStageBuildContext fragment{};
        fragment.stage = ShaderStage::Fragment;
        fragment.inputs.Add({0, ShaderStageValueType::Vec3, 0, 0});
        fragment.inputs.Add({0, ShaderStageValueType::Vec2, 1, 0});
        if (!HasCompatibleStageInterface(vertex, fragment))
            result.diagnostics.emplace_back("matching composed stage interfaces rejected");

        fragment.inputs[1].value_type = ShaderStageValueType::Vec4;
        if (HasCompatibleStageInterface(vertex, fragment))
            result.diagnostics.emplace_back("incompatible composed stage interfaces accepted");

        result.passed = result.diagnostics.empty();
        return result;
    }

    static GateResult RunResolvedStageCacheIdentityCase()
    {
        GateResult result;
        result.name = "Q.resolved-stage-cache-identity";

        const GeometryVertexFormat rg16_geometry{{
            VertexSemantic::Position, VF_V2HF
        }};
        const GeometryVertexFormat rg32_geometry{{
            VertexSemantic::Position, VF_V2F
        }};

        ShaderStageBuildContext stage{};
        stage.stage = ShaderStage::Mesh;
        stage.compile_inputs_hash = 0x13579bdf2468ace0ull;
        const ShaderStageKey stage_key = stage.BuildKey();
        const ShaderStageKey equivalent_stage_key = stage.BuildKey();
        if (!(stage_key == equivalent_stage_key))
            result.diagnostics.emplace_back("equivalent provider stages must share cache identity");

        if (rg16_geometry.GetVertexInputHash() == rg32_geometry.GetVertexInputHash())
            result.diagnostics.emplace_back("raw Geometry formats must remain distinct");

        ShaderLinkSpec rg16_link{};
        rg16_link.mesh_stage = stage_key;
        rg16_link.fragment_stage.stage = ShaderStage::Fragment;
        // 顶点输入格式差异只能通过 mesh stage 的 interface_hash 进键（VS 已废弃，
        // link 层不再单独存一份 vertex_input_hash）。
        rg16_link.mesh_stage.interface_hash ^=
            rg16_geometry.GetVertexInputHash();
        ShaderLinkSpec rg32_link = rg16_link;
        rg32_link.mesh_stage.interface_hash ^=
            rg32_geometry.GetVertexInputHash();
        if (rg16_link.BuildKey() == rg32_link.BuildKey())
            result.diagnostics.emplace_back(
                "equivalent shader stages must retain distinct program input identity");

        ShaderArtifactStore store(RepoRootOSPath("build"), ShaderCacheMode::BuildIfMissing);
        const uint32_t payload[] = {0x07230203u, 1u, 2u, 3u};
        constexpr hgl::uint64 stage_source_digest = 0x534F55524345ull;
        if (!store.SaveStageSPV(
                stage_key, payload, sizeof(payload), stage_source_digest))
        {
            result.diagnostics.emplace_back("stage SPV cache save failed");
        }
        else
        {
            hgl::ValueArray<hgl::uint8> loaded;
            hgl::uint64 loaded_source_digest = 0;
            if (!store.LoadStageSPV(
                    stage_key, loaded, &loaded_source_digest)
             || loaded.GetCount() != static_cast<int>(sizeof(payload))
             || std::memcmp(loaded.GetData(), payload, sizeof(payload)) != 0
             || loaded_source_digest != stage_source_digest)
            {
                result.diagnostics.emplace_back(
                    "stage SPV/source digest cache round-trip failed");
            }

            ShaderStageKey wrong_key = stage_key;
            wrong_key.compile_inputs_hash ^= 1ull;
            if (store.LoadStageSPV(wrong_key, loaded))
                result.diagnostics.emplace_back("cache must reject a different stage key");
        }

        ShaderArtifactStore read_only_store(
            RepoRootOSPath("build"), ShaderCacheMode::ReadOnly);
        if (read_only_store.SaveStageSPV(stage_key, payload, sizeof(payload)))
            result.diagnostics.emplace_back("read-only cache must reject writes");
        const uint32_t invalid_spv[] = {0u, 1u, 2u, 3u};
        if (store.SaveStageSPV(
                stage_key, invalid_spv, sizeof(invalid_spv)))
            result.diagnostics.emplace_back(
                "invalid SPV payload must be rejected");

        ShaderLinkSpec program_link{};
        program_link.mesh_stage = stage_key;
        program_link.fragment_stage.stage = ShaderStage::Fragment;
        program_link.fragment_stage.definition_hash = 0x2201u;
        program_link.fragment_stage.compile_inputs_hash = 0x2202u;
        program_link.fragment_stage.interface_hash = 0x2203u;
        program_link.fragment_stage.compiler_hash =
            stage_key.compiler_hash != 0
                ? stage_key.compiler_hash : 0x2304u;
        program_link.mesh_stage.compiler_hash =
            program_link.fragment_stage.compiler_hash;
        program_link.link_contract_hash = 0x2303u;

        ShaderProgramArtifactMetadata metadata{};
        metadata.program_key_digest =
            program_link.BuildKey().GetDigest();
        metadata.generated_source_digest = 0x2406u;

        const uint32_t fragment_payload[] =
            {0x07230203u, 7u, 8u, 9u};
        if (!store.SaveStageSPV(
                program_link.mesh_stage,
                payload,
                sizeof(payload))
         || !store.SaveStageSPV(
                program_link.fragment_stage,
                fragment_payload,
                sizeof(fragment_payload))
         || !store.SaveProgramMetadata(program_link, metadata))
        {
            result.diagnostics.emplace_back(
                "complete program artifact save failed");
        }
        else
        {
            hgl::ValueArray<hgl::uint8> cached_vertex;
            hgl::ValueArray<hgl::uint8> cached_fragment;
            if (!store.HasProgramMetadata(program_link)
             || !store.LoadProgramArtifacts(
                    program_link,
                    metadata,
                    cached_vertex,
                    cached_fragment)
             || cached_vertex.GetCount()
                    != static_cast<int>(sizeof(payload))
             || cached_fragment.GetCount()
                    != static_cast<int>(sizeof(fragment_payload))
             || std::memcmp(
                    cached_vertex.GetData(),
                    payload,
                    sizeof(payload)) != 0
             || std::memcmp(
                    cached_fragment.GetData(),
                    fragment_payload,
                    sizeof(fragment_payload)) != 0)
            {
                result.diagnostics.emplace_back(
                    "complete program artifact load failed");
            }

            // 命中判定的唯一维度是 program 键摘要：把它改掉必须拒。
            ShaderProgramArtifactMetadata changed_metadata = metadata;
            changed_metadata.program_key_digest ^= 1u;
            if (store.LoadProgramArtifacts(
                    program_link,
                    changed_metadata,
                    cached_vertex,
                    cached_fragment))
            {
                result.diagnostics.emplace_back(
                    "metadata mismatch must reject stage artifacts");
            }

            // 第 3 步：generated_source_digest 是 emit 之后的文本产物，
            // 不参与命中判定——否则命中之前就必须先跑完 GLSL 生成，正是
            // 解耦要消除的耦合。它单独变化仍应命中，由命中后校验
            // （VerifyGeneratedSourceDigest）负责发现"声明键欠近似"。
            ShaderProgramArtifactMetadata text_only_changed = metadata;
            text_only_changed.generated_source_digest ^= 1u;
            if (!store.LoadProgramArtifacts(
                    program_link,
                    text_only_changed,
                    cached_vertex,
                    cached_fragment))
            {
                result.diagnostics.emplace_back(
                    "generated_source_digest must not gate the cache hit");
            }

            // 命中后校验的语义：同一份文本 + 同一摘要必须通过；
            // 摘要被改动必须被拒（这正是"声明键欠近似"的兜底）。
            {
                const char mesh_glsl[] = "void main(){}";
                const char fragment_glsl[] = "void main(){}";
                const size_t mesh_len = sizeof(mesh_glsl) - 1;
                const size_t fragment_len = sizeof(fragment_glsl) - 1;
                const hgl::uint64 digest = ComputeGeneratedSourceDigest(
                    mesh_glsl, mesh_len, fragment_glsl, fragment_len);

                if (digest == 0
                 || !MatchesGeneratedSourceDigest(
                        mesh_glsl, mesh_len, fragment_glsl, fragment_len, digest)
                 || MatchesGeneratedSourceDigest(
                        mesh_glsl, mesh_len, fragment_glsl, fragment_len,
                        digest ^ 1u)
                 || MatchesGeneratedSourceDigest(
                        mesh_glsl, mesh_len, fragment_glsl, fragment_len, 0))
                {
                    result.diagnostics.emplace_back(
                        "post-hit generated_source_digest verification is broken");
                }
            }

            if (!read_only_store.LoadProgramArtifacts(
                    program_link,
                    metadata,
                    cached_vertex,
                    cached_fragment)
             || read_only_store.SaveProgramMetadata(
                    program_link, metadata))
            {
                result.diagnostics.emplace_back(
                    "read-only complete program artifact behavior failed");
            }

            hgl::filesystem::Path metadata_path(
                RepoRootOSPath("build"));
            metadata_path /=
                hgl::ToOSString(hgl::graph::mtl::ShaderArtifactCacheDirectory);
            metadata_path /=
                hgl::ToOSString(hgl::graph::mtl::ShaderArtifactProgramDirectory);
            metadata_path /= hgl::ToOSString(
                (program_link.BuildKey().ToString()
                    + hgl::AnsiString(hgl::graph::mtl::ShaderArtifactMetadataExtension)).c_str());
            hgl::int64 metadata_file_size = 0;
            void *metadata_file =
                hgl::filesystem::LoadFileToMemory(
                    metadata_path.ToOSString(),
                    metadata_file_size);
            if (!metadata_file || metadata_file_size <= 0)
            {
                result.diagnostics.emplace_back(
                    "failed to load metadata corruption fixture");
            }
            else
            {
                auto *metadata_bytes =
                    static_cast<hgl::uint8 *>(metadata_file);
                metadata_bytes[metadata_file_size - 1] ^= 0xffu;
                if (hgl::filesystem::SaveMemoryToFile(
                        metadata_path.ToOSString(),
                        metadata_bytes,
                        metadata_file_size)
                        != metadata_file_size)
                {
                    result.diagnostics.emplace_back(
                        "failed to corrupt program metadata");
                }
                delete[] metadata_bytes;

                ShaderProgramArtifactMetadata corrupted{};
                if (store.LoadProgramMetadata(
                        program_link, corrupted))
                {
                    result.diagnostics.emplace_back(
                        "corrupt program metadata must be rejected");
                }
                if (!store.SaveProgramMetadata(
                        program_link, metadata))
                {
                    result.diagnostics.emplace_back(
                        "failed to restore program metadata fixture");
                }
            }
        }

        ShaderProgramArtifactMetadata invalid_metadata = metadata;
        invalid_metadata.program_key_digest = 0;
        if (store.SaveProgramMetadata(
                program_link, invalid_metadata))
            result.diagnostics.emplace_back(
                "invalid program metadata must reject writes");

        ShaderLinkSpec stage_only_link = program_link;
        stage_only_link.link_contract_hash ^= 0x1000u;
        ShaderProgramArtifactMetadata stage_only_metadata = metadata;
        stage_only_metadata.program_key_digest =
            stage_only_link.BuildKey().GetDigest();
        hgl::ValueArray<hgl::uint8> stage_only_vertex;
        hgl::ValueArray<hgl::uint8> stage_only_fragment;
        if (store.LoadProgramArtifacts(
                stage_only_link,
                stage_only_metadata,
                stage_only_vertex,
                stage_only_fragment))
        {
            result.diagnostics.emplace_back(
                "stage-only artifacts must not count as a program hit");
        }

        ShaderLinkSpec metadata_only_link = program_link;
        metadata_only_link.fragment_stage.definition_hash ^= 0x2000u;
        ShaderProgramArtifactMetadata metadata_only = metadata;
        metadata_only.program_key_digest =
            metadata_only_link.BuildKey().GetDigest();
        if (!store.SaveProgramMetadata(
                metadata_only_link, metadata_only))
        {
            result.diagnostics.emplace_back(
                "metadata-only fixture save failed");
        }
        else if (store.LoadProgramArtifacts(
                    metadata_only_link,
                    metadata_only,
                    stage_only_vertex,
                    stage_only_fragment))
        {
            result.diagnostics.emplace_back(
                "metadata without all stages must not count as a program hit");
        }

        ShaderArtifactStore shadow_store(
            RepoRootOSPath("build/cache-isolation"),
            ShaderCacheMode::BuildIfMissing);
        const uint32_t shadow_payload[] =
            {0x07230203u, 4u, 5u, 6u};
        const uint32_t shadow_fragment_payload[] =
            {0x07230203u, 10u, 11u, 12u};
        if (!shadow_store.SaveStageSPV(
                program_link.mesh_stage,
                shadow_payload,
                sizeof(shadow_payload))
         || !shadow_store.SaveStageSPV(
                program_link.fragment_stage,
                shadow_fragment_payload,
                sizeof(shadow_fragment_payload))
         || !shadow_store.SaveProgramMetadata(
                program_link, metadata))
        {
            result.diagnostics.emplace_back(
                "isolated cache save failed");
        }
        else
        {
            hgl::ValueArray<hgl::uint8> legacy_loaded;
            hgl::ValueArray<hgl::uint8> shadow_loaded;
            if (!store.LoadStageSPV(stage_key, legacy_loaded)
             || legacy_loaded.GetCount() != static_cast<int>(sizeof(payload))
             || std::memcmp(legacy_loaded.GetData(), payload, sizeof(payload)) != 0)
            {
                result.diagnostics.emplace_back(
                    "isolated cache must not replace the primary artifact");
            }

            if (!shadow_store.LoadStageSPV(
                    program_link.mesh_stage, shadow_loaded)
             || shadow_loaded.GetCount() != static_cast<int>(sizeof(shadow_payload))
             || std::memcmp(
                    shadow_loaded.GetData(), shadow_payload, sizeof(shadow_payload)) != 0)
            {
                result.diagnostics.emplace_back(
                    "isolated cache must load its own artifact");
            }

            hgl::ValueArray<hgl::uint8> shadow_fragment_loaded;
            if (!shadow_store.LoadProgramArtifacts(
                    program_link,
                    metadata,
                    shadow_loaded,
                    shadow_fragment_loaded)
             || shadow_fragment_loaded.GetCount()
                    != static_cast<int>(
                        sizeof(shadow_fragment_payload))
             || std::memcmp(
                    shadow_fragment_loaded.GetData(),
                    shadow_fragment_payload,
                    sizeof(shadow_fragment_payload)) != 0)
            {
                result.diagnostics.emplace_back(
                    "program artifacts must remain root-isolated");
            }

            ShaderArtifactStore contract_store(
                RepoRootOSPath("build/cache-miss"),
                ShaderCacheMode::ReadOnly);
            if (contract_store.LoadProgramArtifacts(
                    program_link,
                    metadata,
                    shadow_loaded,
                    shadow_fragment_loaded))
            {
                result.diagnostics.emplace_back(
                    "independent cache root must not hit other programs");
            }
        }

        result.passed = result.diagnostics.empty();
        return result;
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

    static GateResult RunAuthoritativeMaterialCacheIdentityCase()
    {
        GateResult result;
        result.name = "Q0.authoritative-material-cache-identity";

        MaterialDefinition lit{};
        MaterialDefinition vertex_color{};
        if (!TryGetMaterialDefinitionByID("Lit", lit)
         || !TryGetMaterialDefinitionByID("VertexColor", vertex_color))
        {
            result.diagnostics.emplace_back(
                "cache identity material definitions unavailable");
            result.passed = false;
            return result;
        }

        const GeometryVertexFormat &lit_geometry_a = kCacheIdentityLitGeometryA;
        const GeometryVertexFormat &lit_geometry_b = kCacheIdentityLitGeometryB;
        const GeometryVertexFormat &color_geometry = kCacheIdentityColorGeometry;

        // 失败原因必须逐次记录：`GetLastMaterialProgramAssemblyError()` 只反映
        // **最后一次**构建，后面的构建一旦成功就会把它清空，而空串的 `c_str()`
        // 返回 nullptr——诊断里直接取它会把错误路径变成崩溃。
        std::string build_error;
        const auto build = [&build_error](
            const contract::PhysicalDeviceProfileLite *profile,
            const MaterialDefinition &definition,
            const GeometryVertexFormat &geometry,
            const RenderTemplateID template_id,
            const SceneRenderTemplateProfile &scene_profile)
        {
            MaterialDefinitionBuildRequest request{};
            request.recipe.mtl_def_id = definition.definition_id;
            request.geometry_vertex_format = &geometry;
            request.defer_finalize = true;
            RenderTemplateValidationDiagnostic template_diagnostic{};
            if (!ResolveSceneRenderTemplateRequest(
                    template_id, hgl::graph::ShaderStage::Fragment,
                    scene_profile, request.render_template_request,
                    template_diagnostic)
             || !AppendMaterialRenderTemplateRoots(
                    definition, request.render_template_request))
            {
                build_error = "render template request resolution failed";
                return std::unique_ptr<ShaderBuildContext>();
            }
            // 片段源走领域固定源：本用例算的是缓存身份（链接规格 + 各种摘要），
            // 与片段怎么写无关。
            FixedFragmentSource fixed;
            if (!ResolveFixedFragmentSource(definition, request, fixed))
            {
                build_error = "fixed fragment source resolution failed";
                return std::unique_ptr<ShaderBuildContext>();
            }
            FixedStageInputs fixed_stage;
            fixed_stage.fragment_source = fixed.source.c_str();
            fixed_stage.fragment_interface_hash = fixed.interface_hash;
            fixed_stage.fragment_options_digest = fixed.options_digest;
            std::unique_ptr<ShaderBuildContext> context(
                AssembleMaterialProgram(
                    profile, request, definition, nullptr, fixed_stage));
            if (!context || !context->HasProgramLink())
            {
                const hgl::AnsiString &err = GetLastMaterialProgramAssemblyError();
                build_error = err.IsEmpty()
                    ? "(no diagnostic)" : err.c_str();
            }
            return context;
        };
        const auto lit_a = build(
            nullptr, lit, lit_geometry_a,
            RenderTemplateID::ForwardLitShadowedAO,
            MakeIdentityForwardLitProfile());
        const auto lit_b = build(
            nullptr, lit, lit_geometry_b,
            RenderTemplateID::ForwardLitShadowedAO,
            MakeIdentityForwardLitProfile());
        const auto color = build(
            nullptr, vertex_color, color_geometry,
            RenderTemplateID::ForwardUnlit,
            MakeForwardUnlitProfile());
        contract::PhysicalDeviceProfileLite profile{};
        profile.api_version = contract::MakeVkVersion(1, 4);
        profile.limits.max_uniform_buffer_range = 65536;
        profile.limits.max_storage_buffer_range = 1ull << 30;
        const auto lit_targeted = build(
            &profile, lit, lit_geometry_a,
            RenderTemplateID::ForwardLitShadowedAO,
            MakeIdentityForwardLitProfile());

        if (!lit_a || !lit_b || !color || !lit_targeted
         || !lit_a->HasProgramLink()
         || !lit_b->HasProgramLink()
         || !color->HasProgramLink()
         || !lit_targeted->HasProgramLink()
         || !lit_a->HasProgramArtifactMetadata()
         || !lit_b->HasProgramArtifactMetadata()
         || !color->HasProgramArtifactMetadata()
         || !lit_targeted->HasProgramArtifactMetadata())
        {
            result.diagnostics.emplace_back(
                std::string("material builds must produce ProgramLink (last error: ")
                + (build_error.empty() ? "(no diagnostic)" : build_error)
                + ")");
        }
        else
        {
            const ShaderLinkSpec &a = lit_a->GetProgramLink();
            const ShaderLinkSpec &b = lit_b->GetProgramLink();
            const ShaderLinkSpec &c = color->GetProgramLink();
            const ShaderLinkSpec &targeted =
                lit_targeted->GetProgramLink();
            if (lit_a->GetProgramArtifactMetadata().
                    program_key_digest
                    != a.BuildKey().GetDigest())
            {
                result.diagnostics.emplace_back(
                    "BuildContext program metadata does not match ProgramLink");
            }

            ShaderArtifactStore hot_store(
                RepoRootOSPath("build/cache-hot"),
                ShaderCacheMode::BuildIfMissing);
            const uint32_t cached_vertex_spv[] =
                {ShaderArtifactSPVMagic, 0x3101u, 0x3102u, 0x3103u};
            const uint32_t cached_fragment_spv[] =
                {ShaderArtifactSPVMagic, 0x3201u, 0x3202u, 0x3203u};
            const ShaderCreateInfo *lit_mesh =
                lit_a->GetStageShader(ShaderStage::Mesh);
            const ShaderCreateInfo *lit_fragment =
                lit_a->GetStageShader(ShaderStage::Fragment);
            if (!hot_store.SaveStageSPV(
                    a.mesh_stage,
                    cached_vertex_spv,
                    sizeof(cached_vertex_spv),
                    ComputeGeneratedStageSourceDigest(
                        lit_mesh->GetFinalGLSL().data(),
                        lit_mesh->GetFinalGLSL().size()))
             || !hot_store.SaveStageSPV(
                    a.fragment_stage,
                    cached_fragment_spv,
                    sizeof(cached_fragment_spv),
                    ComputeGeneratedStageSourceDigest(
                        lit_fragment->GetFinalGLSL().data(),
                        lit_fragment->GetFinalGLSL().size()))
             || !hot_store.SaveProgramMetadata(
                    a,
                    lit_a->GetProgramArtifactMetadata()))
            {
                result.diagnostics.emplace_back(
                    "hot program artifact fixture save failed");
            }
            else
            {
                lit_a->SetArtifactStore(&hot_store);
                if (!FinalizeShaderBuildContext(lit_a.get())
                 || !lit_a->UsedCachedMeshStage()
                 || !lit_a->UsedCachedFragmentStage()
                 || !lit_a->GetStageShader(ShaderStage::Mesh)
                 || !lit_a->GetStageShader(ShaderStage::Fragment)
                 || lit_a->GetStageShader(
                        ShaderStage::Mesh)->GetSPVSize()
                        != sizeof(cached_vertex_spv)
                 || lit_a->GetStageShader(
                        ShaderStage::Fragment)->GetSPVSize()
                        != sizeof(cached_fragment_spv)
                 || std::memcmp(
                        lit_a->GetStageShader(
                            ShaderStage::Mesh)->GetSPVData(),
                        cached_vertex_spv,
                        sizeof(cached_vertex_spv)) != 0
                 || std::memcmp(
                        lit_a->GetStageShader(
                            ShaderStage::Fragment)->GetSPVData(),
                        cached_fragment_spv,
                        sizeof(cached_fragment_spv)) != 0)
                {
                    result.diagnostics.emplace_back(
                        "hot program hit must populate BuildContext SPV");
                }
            }

            ShaderArtifactStore read_only_miss_store(
                RepoRootOSPath("build/cache-read-only-miss"),
                ShaderCacheMode::ReadOnly);
            lit_b->SetArtifactStore(&read_only_miss_store);
            // read-only miss：任一 stage artifact 缺失都应失败、
            // 不应产生 SPV 数据（stage 对象存在但 SPV 为空）
            if (read_only_miss_store.HasProgramMetadata(
                    lit_b->GetProgramLink())
             || FinalizeShaderBuildContext(lit_b.get())
             || (lit_b->GetStageShader(ShaderStage::Mesh)
                 && lit_b->GetStageShader(
                        ShaderStage::Mesh)->GetSPVSize() != 0)
             || (lit_b->GetStageShader(ShaderStage::Fragment)
                 && lit_b->GetStageShader(
                        ShaderStage::Fragment)->GetSPVSize() != 0))
            {
                result.diagnostics.emplace_back(
                    "read-only program miss must not compile or write");
            }

            if (a.mesh_stage.definition_hash == 0
             || a.mesh_stage.compile_inputs_hash == 0
             || a.mesh_stage.interface_hash == 0
             || a.mesh_stage.compiler_hash == 0
             || a.fragment_stage.definition_hash == 0
             || a.fragment_stage.compile_inputs_hash == 0
             || a.fragment_stage.interface_hash == 0
             || a.fragment_stage.compiler_hash == 0
             || a.link_contract_hash == 0)
            {
                result.diagnostics.emplace_back(
                    "authoritative stage/program keys contain zero identity");
            }

            if (a.fragment_stage.GetDigest()
                    == c.fragment_stage.GetDigest()
             || a.BuildKey() == c.BuildKey())
            {
                result.diagnostics.emplace_back(
                    "different materials must not share authoritative keys");
            }

            if (a.mesh_stage.GetDigest()
                    == b.mesh_stage.GetDigest()
             || a.BuildKey() == b.BuildKey())
            {
                result.diagnostics.emplace_back(
                    "different geometry formats must not share authoritative keys");
            }

            // 编译目标哈希只由「目标 Vulkan 版本 + 目标 SPIR-V 版本 + 代码生成代数」
            // 决定。物理设备量（vendor / device / api_version / limits / features）
            // **不得**再进任何 SPV 缓存键 —— SPIR-V 是平台中立字节码，设备量只影响
            // glslang 的校验门槛，不改变发射出的指令；一旦进键，发行版就得按显卡型号
            // 各烧一份产物，"只打包一套 SPV"这条产品要求直接不成立。
            //
            // 唯一真正进 GLSL 文本的设备量是 mesh invocation 上限，它走的是
            // ClampMeshInvocationsByDevice → definition_hash / interface_hash，
            // 与本断言关心的一般性设备身份无关。
            if (a.mesh_stage.compiler_hash
                    != targeted.mesh_stage.compiler_hash
             || a.fragment_stage.compiler_hash
                    != targeted.fragment_stage.compiler_hash
             || !(a.BuildKey() == targeted.BuildKey()))
            {
                result.diagnostics.emplace_back(
                    "physical device profile must NOT change authoritative keys");
            }

            const char *const shared_definition_ids[] =
            {
                "HumanSkinDefinition",
                "WoodDefinition",
                "StoneDefinition",
                "MetalDefinition"
            };
            std::vector<std::unique_ptr<ShaderBuildContext>>
                shared_builds;
            for (int i = 0; i < 4; ++i)
            {
                MaterialDefinition definition = lit;
                definition.definition_id = shared_definition_ids[i];
                definition.definition_name = shared_definition_ids[i];
                shared_builds.push_back(
                    build(
                        nullptr, definition, lit_geometry_a,
                        RenderTemplateID::ForwardLitShadowedAO,
                        MakeIdentityForwardLitProfile()));
            }

            if (shared_builds.size() != 4
             || !shared_builds[0]
             || !shared_builds[1]
             || !shared_builds[2]
             || !shared_builds[3])
            {
                result.diagnostics.emplace_back(
                    "cross-Definition ProgramKey fixtures failed");
            }
            else
            {
                const ShaderProgramKey shared_key =
                    shared_builds[0]->GetProgramLink().BuildKey();
                for (int i = 1; i < 4; ++i)
                {
                    if (!(shared_builds[i]->
                            GetProgramLink().BuildKey() == shared_key))
                    {
                        result.diagnostics.emplace_back(
                            "equivalent Definitions must share ProgramKey identity");
                        break;
                    }
                }
            }

            const uint64_t context_a =
                HashMaterialProgramBuildContext(
                    PrimitiveType::Triangles,
                    &lit_geometry_a,
                    nullptr);
            const uint64_t context_b =
                HashMaterialProgramBuildContext(
                    PrimitiveType::Triangles,
                    &lit_geometry_b,
                    nullptr);
            const uint64_t context_targeted =
                HashMaterialProgramBuildContext(
                    PrimitiveType::Triangles,
                    &lit_geometry_a,
                    &profile);
            if (context_a == 0
             || context_a == context_b
             || context_a != context_targeted)
            {
                result.diagnostics.emplace_back(
                    "program build context must include geometry but not physical device");
            }

            // ambient provider 是**前向着色**的 program 身份维度（环境层选模块，
            // 不同模块 ⇒ 不同 fragment 源码）；depth/shadow 不消费该槽，必须不受影响。
            const uint64_t context_ambient =
                HashMaterialProgramBuildContext(
                    PrimitiveType::Triangles,
                    &lit_geometry_a,
                    nullptr,
                    ShaderProgramPurpose::ForwardColor,
                    "indirect_ibl");
            const uint64_t context_ambient_empty =
                HashMaterialProgramBuildContext(
                    PrimitiveType::Triangles,
                    &lit_geometry_a,
                    nullptr,
                    ShaderProgramPurpose::ForwardColor,
                    "");
            const uint64_t context_ambient_depth =
                HashMaterialProgramBuildContext(
                    PrimitiveType::Triangles,
                    &lit_geometry_a,
                    nullptr,
                    ShaderProgramPurpose::DepthOnly,
                    "indirect_ibl");
            const uint64_t context_depth =
                HashMaterialProgramBuildContext(
                    PrimitiveType::Triangles,
                    &lit_geometry_a,
                    nullptr,
                    ShaderProgramPurpose::DepthOnly,
                    nullptr);
            if (context_ambient == context_a
             || context_ambient_empty != context_a
             || context_ambient_depth != context_depth)
            {
                result.diagnostics.emplace_back(
                    "program build context must include forward ambient provider only");
            }
        }

        result.passed = result.diagnostics.empty();
        return result;
    }

    static GateResult RunCanonicalShaderContractCase()
    {
        GateResult result;
        result.name = "Q1.canonical-shader-contract";

        // Q1 只保留 OutputContract 序列化恒等/校验段——ShaderInterfaceContract
        // 体系（interface 序列化恒等/location 冲突段）已随生产删除一并移除。

        OutputContract output_a{};
        output_a.purpose = ShaderProgramPurpose::ForwardColor;
        output_a.attachments.Add(
            {
                StableID("output.color"),
                ShaderStageValueType::Vec4,
                0,
                1,
                0
            });
        output_a.attachments.Add(
            {
                StableID("output.material_id"),
                ShaderStageValueType::UInt,
                1,
                1,
                0
            });
        OutputContract output_b = output_a;
        output_b.attachments.Clear();
        output_b.attachments.Add(output_a.attachments[1]);
        output_b.attachments.Add(output_a.attachments[0]);
        if (GetOutputContractHash(output_a) == 0
         || GetOutputContractHash(output_a)
                != GetOutputContractHash(output_b))
        {
            result.diagnostics.emplace_back(
                "output serialization must ignore attachment order");
        }

        OutputContract depth_output{};
        depth_output.purpose = ShaderProgramPurpose::DepthOnly;
        depth_output.depth_only = true;
        if (!ValidateOutputContract(depth_output))
            result.diagnostics.emplace_back(
                "depth-only output contract must allow no attachments");

        OutputContract unsupported_output = depth_output;
        unsupported_output.purpose =
            static_cast<ShaderProgramPurpose>(3);
        if (ValidateOutputContract(unsupported_output))
            result.diagnostics.emplace_back(
                "purpose validation must accept exactly three values");

        result.passed = result.diagnostics.empty();
        return result;
    }

    static GateResult RunMaterializationSharedInstanceCase()
    {
        GateResult result;
        result.name = "D1.materialization-shared-instance-separation";

        MaterialRecipe recipe;
        recipe.recipe_name = "shared-instance-regression";

        RecipeTextureBinding texture{};
        texture.texture_name = "base_color";
        texture.resource_id = "asset/base-color";
        texture.required = true;
        recipe.textures.emplace_back(texture);

        GlobalSSBOBinding asset{
            GlobalSSBOType::PBRSurface,
            41,
            3};
        recipe.material_ssbo_binding = asset;

        const auto *initial_binding =
            recipe.material_ssbo_binding.IsValid()
                ? &recipe.material_ssbo_binding : nullptr;
        if (!initial_binding
         || initial_binding->data_index != 3
         || !initial_binding->IsValid())
        {
            result.diagnostics.emplace_back("shared recipe binding resolve failed");
            result.passed = false;
            return result;
        }

        // Instance separation: changing the per-instance data_index must not
        // leak into the shared recipe identity (HashMaterialRecipe), while
        // the direct recipe binding must still carry the new data_index.
        recipe.material_ssbo_binding.data_index = 9;
        const auto *changed_binding =
            recipe.material_ssbo_binding.IsValid()
                ? &recipe.material_ssbo_binding : nullptr;
        if (!changed_binding
         || changed_binding->data_index != 9)
        {
            result.diagnostics.emplace_back(
                "instance data_index was not preserved in the direct recipe binding");
            result.passed = false;
            return result;
        }

        recipe.material_ssbo_binding.data_index = 3;
        const uint64_t first_recipe_hash = HashMaterialRecipe(recipe);
        recipe.material_ssbo_binding.data_index = 9;
        const uint64_t second_recipe_hash = HashMaterialRecipe(recipe);
        if (first_recipe_hash != second_recipe_hash)
            result.diagnostics.emplace_back(
            "instance data_index must not change shared recipe identity");

        recipe.material_ssbo_binding.ssbo_id = 42;
        const uint64_t changed_buffer_hash = HashMaterialRecipe(recipe);
        if (second_recipe_hash == changed_buffer_hash)
            result.diagnostics.emplace_back(
            "SSBO buffer identity must change shared recipe identity");

        result.passed = result.diagnostics.empty();
        return result;
    }

    static GateResult RunMaterialRegistryCoverageCase()
    {
        GateResult result;
        result.name = "E.material-registry-coverage";

        struct ExpectedEntry
        {
            const char *definition_id;
        };

        static const ExpectedEntry expected[] =
        {
            { "builtin/pure_color" },
            { "builtin/text_gpu" },
            { "builtin/text_gpu_bitmap" },
            { "builtin/checkerboard_2d" },
            { "builtin/checkerboard_3d" },
            { "builtin/error_checkerboard_3d" }
        };

        for (const auto &entry : expected)
        {
            MaterialDefinition definition{};
            if (!TryGetMaterialDefinitionByID(entry.definition_id, definition))
            {
                result.diagnostics.emplace_back(std::string("Missing material definition: ") + entry.definition_id);
                continue;
            }

            if (definition.definition_name.empty())
                result.diagnostics.emplace_back(std::string("Empty material definition name: ") + entry.definition_id);
        }

        result.passed = result.diagnostics.empty();
        return result;
    }

    /// 回退材质必须"任何几何都能画"（数据驱动：遍历整张回退规则表 + 标记行不可得时的
    /// 降级材质，新增种类自动纳入）：
    ///   ① 定义 ID 必须能在材质库里解析（打字错 = 兜底路径整条失效）；
    ///   ② 顶点需求只允许 GeometryAttribute/Position（不得依赖 UV/法线/颜色等可选属性）；
    ///      `Resource` 源需求（要材质数据行）不在本条约束内 —— 回退路径由
    ///      AttachFallbackMarkerColor 提供该行；
    ///   ③ provider_policy 必须是 GeometryOnly（不得靠 code module 反推属性）。
    static GateResult RunMaterialFallbackDefinitionCase()
    {
        GateResult result;
        result.name = "S.material-fallback-definitions-exist";

        const auto check_definition = [&result](const char *label, const char *definition_id)
        {
            MaterialDefinition definition{};
            if (!TryGetMaterialDefinitionByID(definition_id, definition))
            {
                result.diagnostics.emplace_back(std::string("fallback definition for ") + label
                    + " is not registered: " + definition_id);
                return;
            }

            if (definition.vertex_provider_policy != MaterialVertexProviderPolicy::GeometryOnly)
                result.diagnostics.emplace_back(std::string("fallback definition for ") + label
                    + " must use GeometryOnly provider policy: " + definition_id);

            for (uint32_t i = 0; i < definition.vertex_semantic_requirements.GetCount(); ++i)
            {
                const ShaderCodeModuleSemanticRequirement &requirement =
                    definition.vertex_semantic_requirements[i];

                // 只看**几何输入**（GeometryAttribute）：ProducedSemantic 是着色器内部
                // 产出（例如 WorldPosition），不依赖顶点数据，任何几何都满足。
                if (requirement.source == ShaderCodeModuleCapabilitySource::GeometryAttribute
                 && requirement.semantic != ShaderCodeModuleSemantic::Position)
                    result.diagnostics.emplace_back(std::string("fallback definition for ") + label
                        + " must not require vertex attributes beyond Position: " + definition_id);
            }
        };

        for (uint32_t index = 0; index < MATERIAL_ERROR_KIND_COUNT; ++index)
        {
            const MaterialErrorKind kind = static_cast<MaterialErrorKind>(index);
            const FallbackMaterialRule &rule = GetFallbackMaterialRule(kind);

            check_definition((std::string("kind '") + rule.marker_name + "'").c_str(),
                             rule.definition_id);
        }

        // 标记行不可得时的降级材质：与规则表同样的约束（它是最后一道兜底）。
        check_definition("markerless degrade",
                         kMarkerlessFallbackMaterialDefinitionID);

        result.passed = result.diagnostics.empty();
        return result;
    }

    static GateResult RunMaterialDefinitionIdentityCase()
    {
        GateResult result;
        result.name = "R1.material-definition-identity";

        MaterialDefinition pure_color{};
        if (!TryGetMaterialDefinitionByID("builtin/pure_color", pure_color))
        {
            result.diagnostics.emplace_back(
                "PureColor canonical definition is missing");
        }
        else
        {
            if (pure_color.definition_id != "builtin/pure_color")
            {
                result.diagnostics.emplace_back(
                    "PureColor canonical definition identity changed");
            }
        }

        MaterialDefinition text{};
        if (!TryGetMaterialDefinitionByID("builtin/text_gpu", text))
        {
            result.diagnostics.emplace_back("Text canonical definition is missing");
        }
        else if (text.definition_id != "builtin/text_gpu")
        {
            result.diagnostics.emplace_back(
                "Text canonical definition identity changed");
        }

        MaterialRecipe canonical_recipe{};
        canonical_recipe.mtl_def_id = "builtin/text_gpu";
        NormalizeRecipe(canonical_recipe);

        MaterialDefinition ordinary{};
        if (!TryGetMaterialDefinitionByID("Lit", ordinary)
         || ordinary.definition_id != "Lit")
        {
            result.diagnostics.emplace_back(
                "ordinary material lookup must resolve the TOML definition");
        }

        result.passed = result.diagnostics.empty();
        return result;
    }

    static GateResult RunUnifiedMaterialBaselineCase()
    {
        GateResult result;
        result.name = "T.unified-material-baseline";

        MaterialDefinition pure_color{};
        if (!TryGetMaterialDefinitionByID("builtin/pure_color", pure_color))
        {
            result.diagnostics.emplace_back("canonical PureColor must exist");
        }
        else if (pure_color.material_private_data != GlobalSSBOType::EmissiveSurface
               || pure_color.vertex_semantic_requirements.GetCount() != 1)
            result.diagnostics.emplace_back("canonical PureColor contract is not semantic-only");

        result.passed = result.diagnostics.empty();
        return result;
    }

    static GateResult RunMaterialOutputContractCase()
    {
        GateResult result;
        result.name = "V1.material-output-contract";

        MaterialDefinition lit{};
        if (!TryGetMaterialDefinitionByID("Lit", lit))
            {
                result.diagnostics.emplace_back(
                    "Lit definition unavailable for output contract");
            }
            else
            {
                const GeometryVertexFormat geometry{
                    {VertexSemantic::Position, VF_V3F},
                    {VertexSemantic::TexCoord, VF_V2F},
                    {VertexSemantic::Normal, VF_V3F}
                };
                const auto build = [&](
                    const ShaderProgramPurpose purpose,
                    const bool alpha_test,
                    const bool dither,
                    std::string *out_options = nullptr)
                {
                    MaterialDefinition selected = lit;
                    MaterialDefinitionBuildRequest request{};
                    request.recipe.mtl_def_id = selected.definition_id;
                    request.recipe.render_state_overrides.has_alpha_test = true;
                    request.recipe.render_state_overrides.alpha_test = alpha_test;
                    request.recipe.render_state_overrides.has_dither = true;
                    request.recipe.render_state_overrides.dither = dither;
                    request.recipe.render_state_overrides.has_pipeline_config = true;
                    request.geometry_vertex_format = &geometry;
                    request.defer_finalize = true;
                    request.shader_program_purpose = purpose;
                    const bool depth_purpose =
                        purpose == ShaderProgramPurpose::DepthOnly
                     || purpose == ShaderProgramPurpose::ShadowDepth;
                    const bool masked =
                        alpha_test || dither;
                    const RenderTemplateID template_id = depth_purpose
                        ? (masked ? RenderTemplateID::ShadowCasterMasked
                                  : RenderTemplateID::ShadowCasterOpaque)
                        : RenderTemplateID::ForwardLitShadowedAO;
                    const SceneRenderTemplateProfile profile = depth_purpose
                        ? MakeShadowCasterProfile()
                        : MakeIdentityForwardLitProfile();
                    RenderTemplateValidationDiagnostic template_diagnostic{};
                    if (!ResolveSceneRenderTemplateRequest(
                            template_id, hgl::graph::ShaderStage::Fragment,
                            profile, request.render_template_request,
                            template_diagnostic))
                    {
                        result.diagnostics.emplace_back(std::string("ResolveSceneRenderTemplateRequest failed for ") + GetRenderTemplateName(template_id) + " err=" + std::to_string(int(template_diagnostic.error)));
                        return std::unique_ptr<ShaderBuildContext>();
                    }
                    if (!AppendMaterialRenderTemplateRoots(
                            selected, request.render_template_request))
                    {
                        result.diagnostics.emplace_back("AppendMaterialRenderTemplateRoots failed");
                        return std::unique_ptr<ShaderBuildContext>();
                    }
                    // 本用例断言的是输出契约 / program link 形状，片段文本不参与。
                    // 片段源照样按领域固定源填进请求——不填会被
                    // MaterialProgramAssembler 直接拒收（片段拼装链已删除）。
                    FixedFragmentSource fixed;
                    if (!ResolveFixedFragmentSource(selected, request, fixed))
                    {
                        result.diagnostics.emplace_back(
                            std::string("ResolveFixedFragmentSource failed for ")
                            + GetRenderTemplateName(template_id));
                        return std::unique_ptr<ShaderBuildContext>();
                    }
                    FixedStageInputs fixed_stage;
                    fixed_stage.fragment_source = fixed.source.c_str();
                    fixed_stage.fragment_interface_hash = fixed.interface_hash;
                    fixed_stage.fragment_options_digest = fixed.options_digest;
                    if (out_options)
                    {
                        // 覆盖语义（alpha test / cutoff / dither）**不在**固定主
                        // 着色器文本里，而全部落在 generated/frag_options.glsl 的
                        // #define 上。要让本用例仍能断言它，就必须走生产同一条
                        // 发射路径取值（手抄宏表会随发射器漂移）。
                        hgl::AnsiString options_text;
                        if (!EmitDomainOptionsForDefinition(
                                selected, request, fixed.domain, purpose,
                                options_text))
                        {
                            result.diagnostics.emplace_back(
                                std::string("options emission failed for ")
                                + GetRenderTemplateName(template_id));
                        }
                        else if (options_text.Length() > 0)
                        {
                            out_options->assign(
                                options_text.c_str(),
                                static_cast<size_t>(options_text.Length()));
                        }
                    }
                    auto ctx = std::unique_ptr<ShaderBuildContext>(
                        AssembleMaterialProgram(
                            nullptr, request, selected, nullptr, fixed_stage));
                    if (!ctx)
                    {
                        const hgl::AnsiString &err = GetLastMaterialProgramAssemblyError();
                        result.diagnostics.emplace_back(std::string("AssembleMaterialProgram returned null: ") + (err.IsEmpty() ? "unknown error" : err.c_str()));
                    }
                    else if (!ctx->HasProgramLink())
                    {
                        result.diagnostics.emplace_back(std::string("HasProgramLink false for ") + GetRenderTemplateName(template_id));
                    }
                    return ctx;
                };

                std::string masked_depth_options;
                std::string dither_shadow_options;
                const auto opaque = build(
                    ShaderProgramPurpose::ForwardColor,
                    false,
                    false);
                const auto transparent = build(
                    ShaderProgramPurpose::ForwardColor,
                    false,
                    false);
                const auto depth = build(
                    ShaderProgramPurpose::DepthOnly,
                    false,
                    false);
                const auto shadow = build(
                    ShaderProgramPurpose::ShadowDepth,
                    false,
                    false);
                const auto masked_depth = build(
                    ShaderProgramPurpose::DepthOnly,
                    true,
                    false,
                    &masked_depth_options);
                const auto dither_shadow = build(
                    ShaderProgramPurpose::ShadowDepth,
                    false,
                    true,
                    &dither_shadow_options);

                if (!opaque || !opaque->HasProgramLink()) result.diagnostics.emplace_back("opaque failed");
                if (!transparent || !transparent->HasProgramLink()) result.diagnostics.emplace_back("transparent failed");
                if (!depth || !depth->HasProgramLink()) result.diagnostics.emplace_back("depth failed");
                if (!shadow || !shadow->HasProgramLink()) result.diagnostics.emplace_back("shadow failed");
                if (!masked_depth || !masked_depth->HasProgramLink()) result.diagnostics.emplace_back("masked_depth failed");
                if (!dither_shadow || !dither_shadow->HasProgramLink()) result.diagnostics.emplace_back("dither_shadow failed");

                if (!opaque || !transparent || !depth || !shadow
                 || !masked_depth || !dither_shadow
                 || !opaque->HasProgramLink()
                 || !transparent->HasProgramLink()
                 || !depth->HasProgramLink()
                 || !shadow->HasProgramLink()
                 || !masked_depth->HasProgramLink()
                 || !dither_shadow->HasProgramLink())
                {
                    result.diagnostics.emplace_back(
                        "production output contract builds failed");
                }
                else
                {
                    // 安全取 stage GLSL：depth/shadow 变体本无 Fragment stage，
                    // GetStageShader 可能返回悬挂指针——先 has_fragment() 守卫 + 空指针检查，
                    // 并用值拷贝避免悬挂引用。
                    const auto safe_fs = [](const ShaderBuildContext *ctx) -> std::string
                    {
                        if (!ctx || !ctx->has_fragment())
                            return {};
                        const auto *stage = ctx->GetStageShader(ShaderStage::Fragment);
                        return stage ? stage->GetFinalGLSL() : std::string{};
                    };
                    const auto safe_ms = [](const ShaderBuildContext *ctx) -> std::string
                    {
                        if (!ctx || !ctx->has_mesh())
                            return {};
                        const auto *stage = ctx->GetStageShader(ShaderStage::Mesh);
                        return stage ? stage->GetFinalGLSL() : std::string{};
                    };

                    const std::string opaque_fs = safe_fs(opaque.get());
                    const std::string depth_fs = safe_fs(depth.get());
                    const std::string depth_ms = safe_ms(depth.get());
                    const std::string masked_depth_fs = safe_fs(masked_depth.get());
                    const std::string masked_depth_ms = safe_ms(masked_depth.get());
                    const std::string dither_shadow_fs = safe_fs(dither_shadow.get());
                    // 结构化断言：opaque 必有 Fragment（强检查）；
                    // depth/masked/dither 变体仅当 has_fragment() 时检查内容，
                    // 否则跳过对应子项（避免解引用悬挂指针 + 假阳性）。
                    const auto has = [](const ShaderBuildContext *ctx, ShaderStage s)
                    { return ctx && (ctx->GetShaderStage() & uint32_t(s)); };
                    const auto lacks = [](std::string_view src, const char *needle)
                    { return src.find(needle) == std::string::npos; };
                    const auto contains = [](std::string_view src, const char *needle)
                    { return src.find(needle) != std::string::npos; };

                    bool mismatch = false;
                    // opaque：ForwardColor，必有 Fragment
                    if (lacks(opaque_fs, "layout(location=0) out vec4 outColor;")
                     || lacks(opaque_fs, "WriteMaterialOutput(")
                     || contains(opaque_fs, "ULRE_OUTPUT_CONTRACT"))
                        mismatch = true;
                    // depth：DepthOnly，期望无 Fragment 输出相关符号（若有 Fragment 才查）
                    if (has(depth.get(), ShaderStage::Fragment))
                    {
                        if (contains(depth_fs, "outColor")
                         || contains(depth_fs, "WriteMaterialOutput(")
                         || contains(depth_fs, "ULRE_OUTPUT_CONTRACT")
                         || contains(depth_fs, "EvalAlpha("))
                            mismatch = true;
                    }
                    // depth：DepthOnly 的 mesh 源不应含 fragDataIndexID（depth 无需 data index）
                    if (has(depth.get(), ShaderStage::Mesh)
                     && contains(depth_ms, "fragDataIndexID"))
                        mismatch = true;
                    // masked_depth：alpha test，固定主着色器必须真的走覆盖判定
                    // 通路（EvalAlpha + HGLApplyAlpha），且不得引入光照合成。
                    if (has(masked_depth.get(), ShaderStage::Fragment)
                     && (lacks(masked_depth_fs, "EvalAlpha(")
                      || lacks(masked_depth_fs, "HGLApplyAlpha(")
                      || contains(masked_depth_fs, "EvalLighting(")))
                        mismatch = true;
                    // 覆盖宏不在片段文本里（固定源无条件 include
                    // generated/frag_options.glsl），因此改断 options 文本：
                    // alpha test 必须真的发射 HGL_ALPHA_TEST + cutoff。
                    if (masked_depth_options.find("#define HGL_ALPHA_TEST 1")
                            == std::string::npos
                     || masked_depth_options.find("#define HGL_ALPHA_CUTOFF ")
                            == std::string::npos)
                        mismatch = true;
                    // masked_depth：mesh 源不应含 fragWorldPos（depth 无需世界坐标输出）
                    if (has(masked_depth.get(), ShaderStage::Mesh)
                     && contains(masked_depth_ms, "fragWorldPos"))
                        mismatch = true;
                    // dither_shadow：dither 与 alpha test 共用同一份 masked 主着色器，
                    // 区别只在 options —— 必须是 HGL_ALPHA_DITHER（而非 ALPHA_TEST）。
                    if (has(dither_shadow.get(), ShaderStage::Fragment)
                     && (lacks(dither_shadow_fs, "HGLApplyAlpha(")
                      || contains(dither_shadow_fs, "EvalLighting(")))
                        mismatch = true;
                    if (dither_shadow_options.find("#define HGL_ALPHA_DITHER 1")
                            == std::string::npos
                     || dither_shadow_options.find("#define HGL_ALPHA_TEST ")
                            != std::string::npos)
                        mismatch = true;

                    if (mismatch)
                    {
                        result.diagnostics.emplace_back(
                            "production fragment output generation mismatch");
                    }

                    const auto has_material_resource =
                        [](const ShaderBuildContext &spec)
                    {
                        // 运行时数据行需求由编译期直判信号承载（2026-10-11 S1）。
                        return spec.RequiresRecipeRuntimeRows();
                    };
                    // sky 已 BDA 化（不在任何 shader 的资源声明里）⇒ 原「depth 不得携带
                    // sky 资源」的检查已结构性成立，随 SkyInfo 语义退场一并删除。
                    if (has_material_resource(*depth)
                     || !has_material_resource(*masked_depth))
                    {
                        result.diagnostics.emplace_back(
                            "depth coverage resource pruning mismatch");
                    }

                    if (!(opaque->GetProgramLink().BuildKey()
                            == transparent->GetProgramLink().BuildKey())
                     || depth->GetProgramLink().BuildKey()
                            == shadow->GetProgramLink().BuildKey()
                     || opaque->GetProgramLink().BuildKey()
                            == depth->GetProgramLink().BuildKey()
                     || depth->GetProgramLink().BuildKey()
                            == masked_depth->GetProgramLink().BuildKey())
                    {
                        result.diagnostics.emplace_back(
                            "output purpose ProgramKey identity mismatch");
                    }
                }
            }

        result.passed = result.diagnostics.empty();
        return result;
    }

    static GateResult RunPrimitiveVariantPurposeCase()
    {
        GateResult result;
        result.name = "V2.primitive-variant-purpose";

        MaterialRecipe surface_recipe{};
        surface_recipe.recipe_name = "Surface";
        MaterialRecipe depth_recipe{};
        depth_recipe.recipe_name = "Depth";
        MaterialRecipe shadow_recipe{};
        shadow_recipe.recipe_name = "Shadow";
        const PrimitiveVariant variants[] =
        {
            {
                &surface_recipe,
                PrimitiveVariantPurpose::Surface,
                0
            },
            {
                &depth_recipe,
                PrimitiveVariantPurpose::DepthOnly,
                0
            },
            {
                &shadow_recipe,
                PrimitiveVariantPurpose::ShadowCaster,
                0
            }
        };
        const PrimitiveAsset asset(
            nullptr,
            variants,
            static_cast<uint32_t>(std::size(variants)));

        const PrimitiveVariant *surface =
            asset.FindVariantByPurpose(
                PrimitiveVariantPurpose::Surface, 2);
        const PrimitiveVariant *depth =
            asset.FindVariantByPurpose(
                PrimitiveVariantPurpose::DepthOnly, 0);
        const PrimitiveVariant *shadow =
            asset.FindVariantByPurpose(
                PrimitiveVariantPurpose::ShadowCaster, 0);
        const PrimitiveVariant *fallback =
            asset.FindVariantByPurpose(
                PrimitiveVariantPurpose::Picking, 2);
        if (!surface
         || surface->material_recipe != &surface_recipe
         || !depth
         || depth->material_recipe != &depth_recipe
         || !shadow
         || shadow->material_recipe != &shadow_recipe
         || !fallback
         || fallback->material_recipe != &surface_recipe)
        {
            result.diagnostics.emplace_back(
                "primitive variants were not resolved by purpose");
        }

        result.passed = result.diagnostics.empty();
        return result;
    }

    static GateResult RunUnifiedForwardSkeletonCase()
    {
        GateResult result;
        result.name = "V3.unified-forward-skeleton";

        struct ExpectedSource
        {
            const char *definition_id;
            const char *source_module;
        };
        static const ExpectedSource expected_sources[] =
        {
            {"VertexPaletteColor", "vertex_color_source"},
            {"VertexColor", "vertex_color_source"},
            {"VertexLuminance", "luminance_source"},
            {"UnlitTexture", "texture_source"},
            {"DebugNormalColor", "debug_normal_source"},
            {"Lit", "pbr_surface_source"},
            {"builtin/pure_color", "unlit_source"},
            {"builtin/text_gpu", "text_source_gpu"}
        };

        for (const ExpectedSource &expected : expected_sources)
        {
            MaterialDefinition definition{};
            if (!TryGetMaterialDefinitionByID(
                    expected.definition_id, definition)
             || !definition.material_source_module_name
             || std::strcmp(
                    definition.material_source_module_name,
                    expected.source_module) != 0)
            {
                result.diagnostics.emplace_back(
                    std::string("definition did not use unified skeleton: ")
                    + expected.definition_id);
            }
        }

        MaterialDefinition lit{};
        MaterialDefinition unlit{};
        if (!TryGetMaterialDefinitionByID("Lit", lit)
         || !TryGetMaterialDefinitionByID("UnlitTexture", unlit))
        {
            result.diagnostics.emplace_back(
                "unified skeleton build definitions unavailable");
        }
        else
        {
            const GeometryVertexFormat lit_geometry{
                {VertexSemantic::Position, VF_V3F},
                {VertexSemantic::TexCoord, VF_V2F},
                {VertexSemantic::Normal, VF_V3F}
            };
            const GeometryVertexFormat unlit_geometry{
                {VertexSemantic::Position, VF_V2F},
                {VertexSemantic::TexCoord, VF_V2F}
            };
            const auto build = [](
                const MaterialDefinition &definition,
                const GeometryVertexFormat &geometry,
                const RenderTemplateID template_id,
                const SceneRenderTemplateProfile &scene_profile)
            {
                MaterialDefinitionBuildRequest request{};
                request.recipe.mtl_def_id = definition.definition_id;
                request.geometry_vertex_format = &geometry;
                request.defer_finalize = true;
                RenderTemplateValidationDiagnostic template_diagnostic{};
                if (!ResolveSceneRenderTemplateRequest(
                        template_id, hgl::graph::ShaderStage::Fragment,
                        scene_profile, request.render_template_request,
                        template_diagnostic)
                 || !AppendMaterialRenderTemplateRoots(
                        definition, request.render_template_request))
                    return std::unique_ptr<ShaderBuildContext>();
                // 本用例断言的是统一骨架的 program link 形状，片段文本不参与。
                // 片段源照样按领域固定源填进请求——不填会被
                // MaterialProgramAssembler 直接拒收（片段拼装链已删除）。
                FixedFragmentSource fixed;
                if (!ResolveFixedFragmentSource(definition, request, fixed))
                    return std::unique_ptr<ShaderBuildContext>();
                FixedStageInputs fixed_stage;
                fixed_stage.fragment_source = fixed.source.c_str();
                fixed_stage.fragment_interface_hash = fixed.interface_hash;
                fixed_stage.fragment_options_digest = fixed.options_digest;
                return std::unique_ptr<ShaderBuildContext>(
                    AssembleMaterialProgram(
                        nullptr, request, definition, nullptr, fixed_stage));
            };
            const auto lit_build = build(
                lit, lit_geometry, RenderTemplateID::ForwardLitShadowedAO,
                MakeIdentityForwardLitProfile());
            const auto unlit_build = build(
                unlit, unlit_geometry, RenderTemplateID::ForwardUnlit,
                MakeForwardUnlitProfile());
            if (!lit_build || !unlit_build)
            {
                result.diagnostics.emplace_back(
                    "unified forward skeleton build failed");
            }
            else
            {
                const std::string &lit_fs =
                    lit_build->GetStageShader(
                        ShaderStage::Fragment)->GetFinalGLSL();
                const std::string &unlit_fs =
                    unlit_build->GetStageShader(
                        ShaderStage::Fragment)->GetFinalGLSL();
                if (lit_fs.find(
                        "#include \"lighting/forward_pbr.glsl\"")
                        == std::string::npos
                 || lit_fs.find(
                        "#include \"compositor/forward_lighting.glsl\"")
                        == std::string::npos
                 || lit_fs.find(
                        "EvalSurface(si, materialDataIndex)")
                        == std::string::npos
                 || lit_fs.find("EvalPassEnvironment(")
                        == std::string::npos)
                {
                    result.diagnostics.emplace_back(
                        "unified Lit module schedule mismatch");
                }
                // 「有无场景光照」在新架构里不再是宏（旧的
                // `#define HGL_USE_SCENE_LIGHTING 0/1` 属于已删除的生成器 define
                // 文档），而是**结构性的**：Lit 走 forward_pbr + forward_lighting
                // 合成器，无光照领域走 flat_lighting 且完全不含光照模块。
                if (unlit_fs.find(
                        "#include \"compositor/flat_lighting.glsl\"")
                        == std::string::npos
                 || unlit_fs.find(
                        "#include \"common/surface_interface.glsl\"")
                        == std::string::npos
                 || unlit_fs.find(
                        "EvalSurface(si, materialDataIndex)")
                        == std::string::npos
                 || unlit_fs.find("EvalPassEnvironment(")
                        == std::string::npos
                 || unlit_fs.find(
                        "#include \"compositor/forward_lighting.glsl\"")
                        != std::string::npos
                 || unlit_fs.find(
                        "#include \"lighting/forward_pbr.glsl\"")
                        != std::string::npos)
                {
                    result.diagnostics.emplace_back(
                        "unified Unlit module schedule mismatch");
                }
                // AO 槽必须真的被消费：provider 的 #include 排在 compositor 之前
                // （否则 GetAmbientOcclusion 未声明即调用）。此前该槽只 include
                // 不调用，属死契约。
                // 注：调用点在 compositor/forward_lighting.glsl 文件内部，
                // 不会出现在组装后的模板正文里，故此处不查调用点文本。
                // 「是否调用」已不再由宏表达（HGL_USE_AMBIENT_OCCLUSION_PROVIDER
                // 已删除，forward_lighting 无条件接线），故这里只钉 include 顺序。
                const size_t ao_include =
                    lit_fs.find("#include \"ao/identity.glsl\"");
                const size_t output_include =
                    lit_fs.find("#include \"compositor/forward_lighting.glsl\"");
                if (ao_include == std::string::npos
                 || output_include == std::string::npos
                 || ao_include > output_include)
                {
                    result.diagnostics.emplace_back(
                        "ambient occlusion provider slot is not wired");
                }
                // LightingInput.ambientColor 已删：全树零读取点的死字段，
                // 残留会让"天空色算两遍"复活。
                if (lit_fs.find("ambientColor") != std::string::npos
                 || unlit_fs.find("ambientColor") != std::string::npos)
                {
                    result.diagnostics.emplace_back(
                        "dead LightingInput.ambientColor reappeared");
                }
            }
        }

        result.passed = result.diagnostics.empty();
        return result;
    }

    static GateResult RunUnifiedPureColorFragmentCase()
    {
        GateResult result;
        result.name = "V.unified-purecolor-fragment";

        MaterialDefinition pure_color{};
        if (!TryGetMaterialDefinitionByID("builtin/pure_color", pure_color))
        {
            result.diagnostics.emplace_back("unified PureColor definition is unavailable");
            result.passed = false;
            return result;
        }

        if (!pure_color.material_source_module_name
         || std::strcmp(
                pure_color.material_source_module_name,
                "unlit_source") != 0)
            result.diagnostics.emplace_back("PureColor must use one FS module");

        // 片段阶段由领域主着色器物理固定：PureColor 走 UnlitSolid，那份
        // `.frag` 必须真的 include unlit 的 surface provider 与 flat 光照
        // compositor。原先这里组装一遍模板再查文本，组装器已删除；直接查真源
        // 更硬——它查的就是运行时会被编译的那份字节。
        const std::string unlit_fs = ReadFileText(
            RepoRootPath("ShaderLibrary/domain/unlit_solid/unlit_solid.frag"));
        if (unlit_fs.empty())
        {
            result.diagnostics.emplace_back(
                "unlit_solid.frag is unavailable");
        }
        else
        {
            if (unlit_fs.find("#include \"material/unlit_source.glsl\"")
                == std::string::npos)
                result.diagnostics.emplace_back(
                    "unlit_solid.frag does not use material/unlit_source.glsl");
            if (unlit_fs.find("#include \"compositor/flat_lighting.glsl\"")
                == std::string::npos)
                result.diagnostics.emplace_back(
                    "unlit_solid.frag does not use compositor/flat_lighting.glsl");
        }

        result.passed = result.diagnostics.empty();
        return result;
    }

    static GateResult RunUnifiedMaterialContractCase()
    {
        GateResult result;
        result.name = "U.unified-material-contract";

        if (std::strcmp("builtin/pure_color", "builtin/pure_color") != 0)
            result.diagnostics.emplace_back("canonical PureColor ID changed unexpectedly");

        MaterialDefinition pure_color{};
        if (!TryGetMaterialDefinitionByID("builtin/pure_color", pure_color))
        {
            result.diagnostics.emplace_back("canonical PureColor definition missing during contract phase");
        }
        else
        {
            if (pure_color.material_private_data != GlobalSSBOType::EmissiveSurface
             || pure_color.vertex_semantic_requirements.GetCount() != 1)
                result.diagnostics.emplace_back("PureColor contract is not canonical");
        }

        result.passed = result.diagnostics.empty();
        return result;
    }

    static GateResult RunTransformGraphModelCase()
    {
        GateResult result;
        result.name = "W.transform-graph-model";

        const VertexShaderNodeConfig flat = VertexNodeConfigResolver::FlatXY();
        const VertexShaderNodeConfig wall = VertexNodeConfigResolver::WallXY();
        const VertexShaderNodeConfig world = VertexNodeConfigResolver::World3D();
        VertexShaderNodeConfig world_vec2 = world;
        world_vec2.input = VertexInputMode::Vec2Position;
        world_vec2.position_mapping = PositionMappingMode::LiftXY_XY0;

        if (VertexNodeConfigResolver::GetHash(flat) == VertexNodeConfigResolver::GetHash(wall)
         || VertexNodeConfigResolver::GetHash(flat) == VertexNodeConfigResolver::GetHash(world))
            result.diagnostics.emplace_back(
                "transform graph variants must have distinct structural identity");

        if (!VertexNodeConfigResolver::IsScreenLike(flat)
         || !VertexNodeConfigResolver::IsScreenLike(wall)
         || VertexNodeConfigResolver::IsScreenLike(world)
         || VertexNodeConfigResolver::IsScreenLike(world_vec2))
            result.diagnostics.emplace_back(
                "screen-space classification must include projection, not only Vec2 input");

        if (flat.input != VertexInputMode::Vec2Position
         || flat.position_mapping != PositionMappingMode::NDCLift
         || flat.projection != ProjectionMode::LocalToWorldOnly)
            result.diagnostics.emplace_back("FlatXY graph conversion mismatch");

        if (wall.position_mapping != PositionMappingMode::LiftXY_X0Y)
            result.diagnostics.emplace_back("WallXY graph conversion mismatch");

        // Round-trip is identity now that VertexNodeConfigResolver is a static utility
        if (VertexNodeConfigResolver::GetHash(world) != VertexNodeConfigResolver::GetHash(world))
            result.diagnostics.emplace_back("transform graph hash self-consistency failure");

        result.passed = result.diagnostics.empty();
        return result;
    }

    static GateResult RunTransformGraphCompositionCase()
    {
        GateResult result;
        result.name = "X.transform-graph-composition";

        // 生成器删除后，「这个图会拼出哪些 Stage2/Stage3 模块」不再靠生成一段
        // GLSL 去观察，而是直接问 `VertexNodeConfigResolver` 要模块路径，并核对
        // **该模块文件真的在库里**。少一个文件、改名、删枚举分支都会在这里硬失败，
        // 而原断言只证明「生成器当次拼对了字符串」。
        const auto require_module = [&](const char *module, const char *context)
        {
            if (!module || !module[0])
            {
                result.diagnostics.emplace_back(
                    std::string(context) + ": 图没有映射到任何模块");
                return;
            }
            const std::string path =
                RepoRootPath((std::string("ShaderLibrary/") + module).c_str());
            std::error_code ec;
            if (!std::filesystem::is_regular_file(path, ec))
                result.diagnostics.emplace_back(
                    std::string(context) + ": 模块文件不存在 " + module);
        };

        for (uint32_t i = uint32_t(PositionMappingMode::BEGIN_RANGE);
             i <= uint32_t(PositionMappingMode::END_RANGE); ++i)
        {
            const auto mapping = PositionMappingMode(i);
            VertexShaderNodeConfig cfg = MakeDefault3DNodeConfig();
            cfg.position_mapping = mapping;
            require_module(
                VertexNodeConfigResolver::GetMappingModulePath(cfg),
                GetPositionMappingModeName(mapping));
            if (GetPositionMappingComponentCount(mapping) == 0)
                result.diagnostics.emplace_back(
                    std::string(GetPositionMappingModeName(mapping))
                    + ": 位置分量数不能为 0");
        }

        for (uint32_t i = uint32_t(ProjectionMode::BEGIN_RANGE);
             i <= uint32_t(ProjectionMode::END_RANGE); ++i)
        {
            const auto projection = ProjectionMode(i);
            VertexShaderNodeConfig cfg = MakeDefault3DNodeConfig();
            cfg.projection = projection;
            require_module(
                VertexNodeConfigResolver::GetStage3ModulePath(cfg),
                GetProjectionModeName(projection));
        }

        // 相机朝向是 Stage3 的另一条分叉（不看 projection），必须单独覆盖。
        {
            VertexShaderNodeConfig facing = MakeDefault3DNodeConfig();
            facing.orientation = OrientationMode::CameraFacingAxisY;
            facing.scale = ScaleMode::World;
            require_module(
                VertexNodeConfigResolver::GetStage3ModulePath(facing),
                "CameraFacingAxisY/World");
            facing.scale = ScaleMode::FixedPixelSize;
            require_module(
                VertexNodeConfigResolver::GetStage3ModulePath(facing),
                "CameraFacingAxisY/FixedPixelSize");
        }

        // 图不同 ⇒ 选中的模块必须不同（否则「图」就是个摆设）。
        {
            const std::string flat_module = VertexNodeConfigResolver::GetMappingModulePath(
                VertexNodeConfigResolver::FlatXY());
            const std::string wall_module = VertexNodeConfigResolver::GetMappingModulePath(
                VertexNodeConfigResolver::WallXY());
            const std::string world_module = VertexNodeConfigResolver::GetMappingModulePath(
                VertexNodeConfigResolver::World3D());
            if (flat_module == wall_module || flat_module == world_module
             || wall_module == world_module)
                result.diagnostics.emplace_back(
                    "transform graph variants must map to distinct Stage2 modules");
        }

        ShaderStageBuildContext flat_stage{};
        flat_stage.stage = ShaderStage::Mesh;
        flat_stage.compile_inputs_hash =
            VertexNodeConfigResolver::GetHash(VertexNodeConfigResolver::FlatXY());
        const ShaderStageKey flat_key = flat_stage.BuildKey();

        ShaderStageBuildContext wall_stage{};
        wall_stage.stage = ShaderStage::Mesh;
        wall_stage.compile_inputs_hash =
            VertexNodeConfigResolver::GetHash(VertexNodeConfigResolver::WallXY());
        const ShaderStageKey wall_key = wall_stage.BuildKey();
        if (flat_key == wall_key)
            result.diagnostics.emplace_back(
                "transform graph variants must produce distinct stage identity");

        result.passed = result.diagnostics.empty();
        return result;
    }

    static GateResult RunGlobalSSBOBindingKeyCase()
    {
        GateResult result;
        result.name = "Y1a.material-ssbo-binding-key";

        MaterialRecipe recipe{};
        recipe.material_ssbo_binding = {
            GlobalSSBOType::EmissiveSurface,
            11,
            7};
        if (!recipe.material_ssbo_binding.IsValid())
        {
            result.diagnostics.emplace_back(
                "material SSBO binding rejected a valid binding");
        }

        recipe.material_ssbo_binding = {
            GlobalSSBOType::EmissiveSurface,
            44,
            9};
        if (!recipe.material_ssbo_binding.IsValid())
        {
            result.diagnostics.emplace_back(
                "material SSBO binding replacement is invalid");
        }

        MaterialRecipe missing_row_id_recipe{};
        missing_row_id_recipe.material_ssbo_binding = {
            GlobalSSBOType::EmissiveSurface,
            55,
            uint32_t(-1)};
        if (missing_row_id_recipe.material_ssbo_binding.IsValid())
        {
            result.diagnostics.emplace_back(
                "material SSBO binding accepted a missing active row ID");
        }

        const auto *material_data =
            recipe.material_ssbo_binding.IsValid()
                ? &recipe.material_ssbo_binding : nullptr;
        if (!material_data || material_data->ssbo_id != 44
         || material_data->data_index != 9
         || material_data->ssbo_type != GlobalSSBOType::EmissiveSurface)
        {
            result.diagnostics.emplace_back(
                "material SSBO binding replacement did not preserve its identity");
        }

        if (!material_data
         || material_data->ssbo_type != GlobalSSBOType::EmissiveSurface)
        {
            result.diagnostics.emplace_back(
                "material SSBO binding type did not remain explicit");
        }

        MaterialRecipe no_binding_recipe{};
        if (ResolveRecipeSSBOType(
                recipe,
                GlobalSSBOType::PBRSurface)
                != GlobalSSBOType::EmissiveSurface
         || ResolveRecipeSSBOType(
                no_binding_recipe,
                GlobalSSBOType::PBRSurface)
                != GlobalSSBOType::PBRSurface
         || ResolveRecipeSSBOType(
                recipe,
                GlobalSSBOType::EmissiveSurface)
                != GlobalSSBOType::EmissiveSurface)
        {
            result.diagnostics.emplace_back(
                "material authoring must inherit recipe SSBO type unless an explicit material override is supplied");
        }

        if (recipe.material_ssbo_binding.data_index == uint32_t(-1))
        {
            result.diagnostics.emplace_back(
                "valid material SSBO binding lost its active row ID");
        }

        result.passed = result.diagnostics.empty();
        return result;
    }

    static GateResult RunResolvedMaterialRenderStateCase()
    {
        GateResult result;
        result.name = "Y1.resolved-material-render-state";

        MaterialDefinition definition{};
        definition.default_render_state.alpha_test = true;
        definition.default_render_state.alpha_cutoff = 0.35f;
        definition.default_render_state.pipeline_config =
            MakeSolid3DConfig();
        definition.default_render_state.pipeline_config.depth_write = false;
        definition.default_render_state.pipeline_config.cull_mode = VK_CULL_MODE_NONE;

        MaterialRecipe defaults{};
        const ResolvedMaterialRenderState default_state =
            ResolveMaterialRenderState(definition, defaults);
        if (default_state.pipeline_config.cull_mode != VK_CULL_MODE_NONE
         || !default_state.alpha_test
         || default_state.alpha_cutoff != 0.35f
         || default_state.pipeline_config.depth_write)
        {
            result.diagnostics.emplace_back(
                "definition render defaults were not preserved by resolution");
        }

        MaterialRecipe overrides{};
        overrides.render_state_overrides.has_alpha_test = true;
        overrides.render_state_overrides.alpha_test = false;
        overrides.render_state_overrides.has_alpha_cutoff = true;
        overrides.render_state_overrides.alpha_cutoff = 0.8f;
        overrides.render_state_overrides.has_dither = true;
        overrides.render_state_overrides.dither = true;
        overrides.render_state_overrides.has_pipeline_config = true;
        overrides.render_state_overrides.pipeline_config =
            MakeAlpha3DConfig();
        overrides.render_state_overrides.pipeline_config.cull_mode = VK_CULL_MODE_BACK_BIT;
        const ResolvedMaterialRenderState resolved =
            ResolveMaterialRenderState(definition, overrides);
        if (resolved.pipeline_config.cull_mode != VK_CULL_MODE_BACK_BIT
         || resolved.alpha_test
         || resolved.alpha_cutoff != 0.8f
         || !resolved.dither
         || !resolved.pipeline_config.alpha_blend)
        {
            result.diagnostics.emplace_back(
                "recipe render-state overrides were not applied atomically");
        }

        if (HashResolvedMaterialRenderState(default_state)
            == HashResolvedMaterialRenderState(resolved))
        {
            result.diagnostics.emplace_back(
                "resolved render-state differences must affect pipeline identity");
        }

        // S4b/D4：归一化取用口只剩 TryGet… —— 未归一化 recipe 必须被**拒绝**
        // （旧口会静默拷结构体默认值 ⇒ 错值），归一化后与 ResolveMaterialRenderState(definition, recipe) 等价。
        MaterialRecipe unnormalized{};
        ResolvedMaterialRenderState from_recipe{};
        if (TryGetNormalizedRecipeRenderState(unnormalized, from_recipe))
        {
            result.diagnostics.emplace_back(
                "un-normalized recipe must be rejected by TryGetNormalizedRecipeRenderState");
        }

        MaterialRecipe normalized_recipe{};
        normalized_recipe.mtl_def_id = "builtin/pure_color";
        NormalizeRecipe(normalized_recipe);
        if (!TryGetNormalizedRecipeRenderState(normalized_recipe, from_recipe))
        {
            result.diagnostics.emplace_back(
                "normalized recipe must be readable by TryGetNormalizedRecipeRenderState");
        }
        else
        {
            MaterialDefinition pure_color{};
            if (!TryGetMaterialDefinitionByID("builtin/pure_color", pure_color))
            {
                result.diagnostics.emplace_back(
                    "builtin/pure_color definition is missing");
            }
            else
            {
                const ResolvedMaterialRenderState expected =
                    ResolveMaterialRenderState(pure_color, normalized_recipe);
                if (from_recipe.alpha_test != expected.alpha_test
                 || from_recipe.alpha_cutoff != expected.alpha_cutoff
                 || from_recipe.dither != expected.dither
                 || HashResolvedMaterialRenderState(from_recipe)
                        != HashResolvedMaterialRenderState(expected))
                {
                    result.diagnostics.emplace_back(
                        "normalized-recipe accessor must equal ResolveMaterialRenderState(definition, recipe)");
                }
            }
        }

        result.passed = result.diagnostics.empty();
        return result;
    }

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

    static bool HasLegacySchemaToken(const std::string &text) noexcept
    {
        static const char *tokens[] = {
            "schema = 0", "schema=0",
            "schema = 1", "schema=1",
            "schema = 2", "schema=2",
            "schema = 3", "schema=3",
            "schema \t=\t 2", "schema \t=\t 1"
        };
        for (const char *token : tokens)
        {
            if (text.find(token) != std::string::npos)
                return true;
        }
        return false;
    }

    static GateResult RunMaterialLibraryContractCase()
    {
        GateResult result;
        result.name = "M.material-library-contract";

        const std::string root = GetShaderLibraryPath("ShaderLibrary");
        const std::filesystem::path material_dir =
            std::filesystem::path(root) / "material";

        if (!std::filesystem::exists(material_dir)
         || !std::filesystem::is_directory(material_dir))
        {
            result.diagnostics.emplace_back(
                "missing required ShaderLibrary/material directory: "
                + material_dir.string());
            result.passed = false;
            return result;
        }

        std::vector<std::string> material_files;
        try
        {
            for (const auto &entry : std::filesystem::recursive_directory_iterator(material_dir))
            {
                if (!entry.is_regular_file())
                    continue;
                if (entry.path().extension() == ".toml")
                    material_files.push_back(entry.path().string());
            }
        }
        catch (const std::filesystem::filesystem_error &)
        {
            result.diagnostics.emplace_back(
                "failed to inspect ShaderLibrary/material tree");
            result.passed = false;
            return result;
        }

        if (material_files.empty())
        {
            result.diagnostics.emplace_back(
                "ShaderLibrary/material contains no .mi.toml files");
            result.passed = false;
            return result;
        }

        for (const std::string &path : material_files)
        {
            const std::string text = ReadFileText(path);
            if (text.empty())
            {
                result.diagnostics.emplace_back(
                    "empty material file: " + path);
                continue;
            }

            // 阶段四：材质资产统一为材质实例格式 `*.mi.toml`。
            // 这里刻意不再「跳过不认识的后缀」——目录里出现别的 .toml 就是违规，
            // 否则旧 `*.material.toml` 会被静默忽略（加载器也不会读它）。
            constexpr const char kMaterialInstanceSuffix[] = ".mi.toml";
            constexpr std::size_t kMaterialInstanceSuffixLength =
                sizeof(kMaterialInstanceSuffix) - 1;
            if (path.size() < kMaterialInstanceSuffixLength
             || path.compare(path.size() - kMaterialInstanceSuffixLength,
                             kMaterialInstanceSuffixLength,
                             kMaterialInstanceSuffix) != 0)
            {
                result.diagnostics.emplace_back(
                    "material asset must use the .mi.toml suffix: " + path);
                continue;
            }

            if (HasLegacySchemaToken(text))
            {
                result.diagnostics.emplace_back(
                    "legacy schema is forbidden in material file: " + path);
            }

            if (text.find("schema = 4") == std::string::npos
             && text.find("schema=4") == std::string::npos)
            {
                result.diagnostics.emplace_back(
                    "material file must use schema 4 only: " + path);
            }

            // 领域必须是显式声明，不允许靠缺省值蒙混。
            // 非模型表面材质（调试/UI/天空等）同样要写 domain = "Custom"，
            // 这样「已定性为非领域」与「漏填」在文本层就能区分开。
            if (text.find("domain =") == std::string::npos
             && text.find("domain=") == std::string::npos)
            {
                result.diagnostics.emplace_back(
                    "material file must declare an explicit domain key "
                    "(use \"Custom\" for engine-internal non-surface materials): "
                    + path);
            }

            if (ContainsAnyToken(text, {
                    "MTL_DEF_",
                    "MaterialDefinitionSourceKind",
                    "MaterialDefinitionBootstrapKind",
                    "embedded material",
                    "EmbeddedMaterial" }))
            {
                result.diagnostics.emplace_back(
                    "embedded material or legacy definition IDs are forbidden in: "
                    + path);
            }
        }

        for (const auto &root_dir : { std::filesystem::path("src"), std::filesystem::path("inc") })
        {
            if (!std::filesystem::exists(root_dir) || !std::filesystem::is_directory(root_dir))
                continue;

            try
            {
                for (const auto &entry : std::filesystem::recursive_directory_iterator(root_dir))
                {
                    if (!entry.is_regular_file())
                        continue;

                    const std::string extension = entry.path().extension().string();
                    if (extension != ".cpp" && extension != ".h" && extension != ".hpp"
                     && extension != ".inl" && extension != ".inc")
                        continue;

                    const std::string path = entry.path().string();
                    if (path.find("ShaderResourceSchemaRegressionGate.cpp") != std::string::npos)
                        continue;

                    const std::string text = ReadFileText(path);
                    if (text.empty())
                        continue;

                    if (ContainsAnyToken(text, {
                            "MTL_DEF_",
                            "MaterialDefinitionSourceKind",
                            "MaterialDefinitionBootstrapKind" }))
                    {
                        result.diagnostics.emplace_back(
                            "legacy embedded material bootstrapping markers are forbidden in: "
                            + path);
                    }
                }
            }
            catch (const std::filesystem::filesystem_error &)
            {
                result.diagnostics.emplace_back(
                    "failed to scan source tree for legacy material bootstrap markers");
            }
        }

        result.passed = result.diagnostics.empty();
        return result;
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

    /// C++ 表结构（GlobalAddresses.h）与 GLSL 侧 GlobalAddressesRef 必须逐字段同序同型。
    /// 为什么必须门禁：C++ 加了字段而 GLSL 漏加时，材质 shader 编译失败会**静默回退默认材质**
    /// ——画面照常有内容、其余门全绿、0 校验层消息，只有渲染契约门（ATS D3）才抓得到。
    static GateResult RunGlobalAddressesStructParityCase()
    {
        GateResult result;
        result.name = "S.global-addresses-struct-parity";

        const std::string cpp_text =
            ReadFileText(RepoRootPath("inc/hgl/graph/ubo/GlobalAddresses.h"));
        const std::string glsl_text =
            ReadFileText(RepoRootPath("ShaderLibrary/ubo/global_addresses.glsl"));

        if (cpp_text.empty() || glsl_text.empty())
        {
            result.diagnostics.emplace_back(
                "无法读取 GlobalAddresses.h / ubo/global_addresses.glsl（结构对表的两端）");
            result.passed = result.diagnostics.empty();
            return result;
        }

        const std::vector<std::string> cpp_fields =
            ExtractU64Fields(cpp_text, "GlobalAddresses");
        const std::vector<std::string> glsl_fields =
            ExtractU64Fields(glsl_text, "GlobalAddressesRef");

        if (cpp_fields.empty() || glsl_fields.empty())
        {
            result.diagnostics.emplace_back(
                "GlobalAddresses 字段提取为空（结构标记被改写？）C++="
                + std::to_string(cpp_fields.size())
                + " GLSL=" + std::to_string(glsl_fields.size()));
            result.passed = result.diagnostics.empty();
            return result;
        }

        if (cpp_fields != glsl_fields)
        {
            std::string detail = "GlobalAddresses 字段漂移（C++ "
                + std::to_string(cpp_fields.size()) + " 项 / GLSL "
                + std::to_string(glsl_fields.size()) + " 项）：";
            const size_t count = cpp_fields.size() > glsl_fields.size()
                               ? cpp_fields.size() : glsl_fields.size();
            for (size_t i = 0; i < count; ++i)
            {
                const std::string lhs =
                    i < cpp_fields.size() ? cpp_fields[i] : std::string("<缺>");
                const std::string rhs =
                    i < glsl_fields.size() ? glsl_fields[i] : std::string("<缺>");
                if (lhs != rhs)
                {
                    detail += "第 " + std::to_string(i) + " 项 C++=" + lhs
                            + " GLSL=" + rhs;
                    break;
                }
            }
            detail += "；漏加字段会让材质 shader 静默回退默认材质";
            result.diagnostics.emplace_back(detail);
        }

        result.passed = result.diagnostics.empty();
        return result;
    }

    /// C++ 世界地址表（WorldAddresses.h）与 GLSL 侧 WorldAddressesRef 必须逐字段同序同型。
    /// 与 S.global-addresses-struct-parity 同理：漏加字段 = shader 静默回退默认材质 / 解引用错地址。
    static GateResult RunWorldAddressesStructParityCase()
    {
        GateResult result;
        result.name = "W.world-addresses-struct-parity";

        const std::string cpp_text =
            ReadFileText(RepoRootPath("inc/hgl/graph/ubo/WorldAddresses.h"));
        const std::string glsl_text =
            ReadFileText(RepoRootPath("ShaderLibrary/ubo/world_addresses.glsl"));

        if (cpp_text.empty() || glsl_text.empty())
        {
            result.diagnostics.emplace_back(
                "无法读取 WorldAddresses.h / ubo/world_addresses.glsl（结构对表的两端）");
            result.passed = result.diagnostics.empty();
            return result;
        }

        const std::vector<std::string> cpp_fields =
            ExtractU64Fields(cpp_text, "WorldAddresses");
        const std::vector<std::string> glsl_fields =
            ExtractU64Fields(glsl_text, "WorldAddressesRef");

        if (cpp_fields.empty() || glsl_fields.empty())
        {
            result.diagnostics.emplace_back(
                "WorldAddresses 字段提取为空（结构标记被改写？）C++="
                + std::to_string(cpp_fields.size())
                + " GLSL=" + std::to_string(glsl_fields.size()));
            result.passed = result.diagnostics.empty();
            return result;
        }

        if (cpp_fields != glsl_fields)
        {
            std::string detail = "WorldAddresses 字段漂移（C++ "
                + std::to_string(cpp_fields.size()) + " 项 / GLSL "
                + std::to_string(glsl_fields.size()) + " 项）：";
            const size_t count = cpp_fields.size() > glsl_fields.size()
                               ? cpp_fields.size() : glsl_fields.size();
            for (size_t i = 0; i < count; ++i)
            {
                const std::string lhs =
                    i < cpp_fields.size() ? cpp_fields[i] : std::string("<缺>");
                const std::string rhs =
                    i < glsl_fields.size() ? glsl_fields[i] : std::string("<缺>");
                if (lhs != rhs)
                {
                    detail += "第 " + std::to_string(i) + " 项 C++=" + lhs
                            + " GLSL=" + rhs;
                    break;
                }
            }
            detail += "；漏加字段会让世界私有地址（相机行/渲染项）解引用错地址";
            result.diagnostics.emplace_back(detail);
        }

        result.passed = result.diagnostics.empty();
        return result;
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

    /// 机制层契约（A6-b）：**批信息行** + **批的 dispatch 档**两端同序同型同值。
    /// · `BatchInfoRow`（16 B：row_base / capacity / topology / group_size）是 gather 逐批读的
    ///   切片描述 —— 漂移 = 按错偏移读切片（静默错位/错画）；
    /// · 档号（`kBatchTopologyTriangles/Lines` ↔ GLSL `#define TOPOLOGY_*`）**两处各写一遍**
    ///   ⇒ 漂移 = 线的 X 公式选错档（按顶点数而非段数）⇒ 整批线段少画/错画。
    /// 两条都是 S./E. 同族的**静默**错位坑（像素判据只在特定角度才看得见），必须门禁。
    static GateResult RunBatchInfoStructParityCase()
    {
        GateResult result;
        result.name = "B.batch-info-struct-parity";

        const std::string cpp_text =
            ReadFileText(RepoRootPath("inc/hgl/ecs/support/GPUCullSystem.h"));
        const std::string glsl_text =
            ReadFileText(RepoRootPath("ShaderLibrary/ecs/entity_gpu_cull.glsl"));

        if (cpp_text.empty() || glsl_text.empty())
        {
            result.diagnostics.emplace_back(
                "无法读取 GPUCullSystem.h / entity_gpu_cull.glsl（结构对表的两端）");
            result.passed = result.diagnostics.empty();
            return result;
        }

        // ── ① 行形状：逐字段同序同型（类型/长度已归一） ──
        const std::vector<StructFieldEntry> cpp_fields =
            ExtractStructFieldsLineWise(cpp_text, "BatchInfoRow");
        const std::vector<StructFieldEntry> glsl_fields =
            ExtractStructFieldsLineWise(glsl_text, "BatchInfoRow");

        if (cpp_fields.empty() || glsl_fields.empty())
        {
            result.diagnostics.emplace_back(
                "BatchInfoRow 字段提取为空（块头被改写？）C++="
                + std::to_string(cpp_fields.size())
                + " GLSL=" + std::to_string(glsl_fields.size()));
            result.passed = result.diagnostics.empty();
            return result;
        }

        if (cpp_fields.size() != glsl_fields.size())
        {
            result.diagnostics.emplace_back(
                "BatchInfoRow 字段数漂移：C++ " + std::to_string(cpp_fields.size())
                + " 项 / GLSL " + std::to_string(glsl_fields.size())
                + " 项；漂移 = gather 按错偏移读切片（静默错位/错画）");
        }
        else
        {
            for (size_t i = 0; i < cpp_fields.size(); ++i)
            {
                if (cpp_fields[i].type == glsl_fields[i].type
                 && cpp_fields[i].name == glsl_fields[i].name
                 && cpp_fields[i].dim  == glsl_fields[i].dim)
                    continue;

                result.diagnostics.emplace_back(
                    "BatchInfoRow 第 " + std::to_string(i) + " 字段漂移：C++="
                    + cpp_fields[i].type + " " + cpp_fields[i].name
                    + " / GLSL=" + glsl_fields[i].type + " " + glsl_fields[i].name
                    + "；漂移 = gather 按错偏移读切片（静默错位/错画）");
                break;
            }
        }

        // ── ② 派发档 / 每 pass 块常量：C++ 常量 ↔ GLSL #define 数值一致（两处各写
        //    一遍的漂移面；格 = 整数常量，故与档号同表） ──
        struct TierPair
        {
            const char *cpp_token;
            const char *glsl_token;
        };

        const TierPair tiers[] =
        {
            { "kBatchTopologyTriangles", "#define TOPOLOGY_TRIANGLES" },
            { "kBatchTopologyLines",     "#define TOPOLOGY_LINES"     },
            // T4c2：批行/计数按 pass 分块 ⇒ 遍数两端各写一遍（漂移 = 阴影块永不被写或被越界写）
            // ⚠ C++ 端令牌必须带类型+声明符：`kArenaPasses` 的**首次出现**是
            //   GetBatchLayout 的越界判断（同一行无数字）⇒ 只用标识符会提取失败误报。
            { "static constexpr uint32_t kArenaPasses", "#define ULRE_ARENA_PASSES" },
        };

        for (const TierPair &tier : tiers)
        {
            const int cpp_value  = ExtractFirstIntAfter(cpp_text,  tier.cpp_token);
            const int glsl_value = ExtractFirstIntAfter(glsl_text, tier.glsl_token);

            if (cpp_value < 0 || glsl_value < 0)
            {
                result.diagnostics.emplace_back(
                    std::string("派发档找不到数值：C++ ") + tier.cpp_token
                    + " / GLSL " + tier.glsl_token
                    + "（常量改名？两端各写一遍，改名必须同步）");
                continue;
            }

            if (cpp_value != glsl_value)
            {
                result.diagnostics.emplace_back(
                    std::string("派发档数值漂移：") + tier.cpp_token + "="
                    + std::to_string(cpp_value) + " vs " + tier.glsl_token + "="
                    + std::to_string(glsl_value)
                    + "；档号错 = 线/网格的 X 公式选错（整批少画/错画）");
            }
        }

        result.passed = result.diagnostics.empty();
        return result;
    }

    /// C++ EntityGPU（inc/hgl/ecs/core/EntityGPU.h）与 GLSL 侧 EntityGPUData
    /// （ShaderLibrary/ecs/entity_gpu.glsl）必须逐字段同序同型同长度（§7.4 / §8.3-③）。
    /// 为什么必须门禁：这是 128 B 行的 GPU 镜像 —— 漂移 = 剔除着色器按错步长解引用
    /// 实体行（W.world-addresses-struct-parity 同族的静默错位坑）。
    /// 两端唯一容许的形状差 = GLSL 行尾**显式**填充 `_pad`（scalar layout 下 GLSL
    /// 不会隐式补 alignas 的 40 B，不写死则数组步长 = 88 而非 128）：比对时要求
    /// C++ 字段列表是 GLSL 字段列表的前缀，余量全部由 `_pad*` 承担，且 GLSL 侧
    /// 总存储尺寸必须 == 128（sizeof(EntityGPU) 的头文件断言同值，此处对拍）。
    static GateResult RunEntityGPUStructParityCase()
    {
        GateResult result;
        result.name = "E.entity-gpu-struct-parity";

        const std::string cpp_text =
            ReadFileText(RepoRootPath("inc/hgl/ecs/core/EntityGPU.h"));
        const std::string glsl_text =
            ReadFileText(RepoRootPath("ShaderLibrary/ecs/entity_gpu.glsl"));

        if (cpp_text.empty() || glsl_text.empty())
        {
            result.diagnostics.emplace_back(
                "无法读取 EntityGPU.h / entity_gpu.glsl（结构对表的两端）");
            result.passed = result.diagnostics.empty();
            return result;
        }

        const std::vector<StructFieldEntry> cpp_fields =
            ExtractStructFieldsLineWise(cpp_text, "EntityGPU");
        const std::vector<StructFieldEntry> glsl_fields =
            ExtractStructFieldsLineWise(glsl_text, "EntityGPUData");

        const auto fmt = [](const std::vector<StructFieldEntry> &fields) -> std::string
        {
            std::string out;
            for (const StructFieldEntry &f : fields)
            {
                if (!out.empty()) out += ", ";
                out += f.type + " " + f.name;
                if (!f.dim.empty()) out += "[" + f.dim + "]";
            }
            return out;
        };

        if (cpp_fields.empty() || glsl_fields.empty())
        {
            result.diagnostics.emplace_back(
                std::string("EntityGPU 字段提取为空（结构标记被改写？）取块器 = 逐行式，")
                + "C++= " + fmt(cpp_fields) + " | GLSL= " + fmt(glsl_fields));
            result.passed = result.diagnostics.empty();
            return result;
        }

        // C++ 字段必须是 GLSL 字段的前缀（同序同型同长度逐字段比对）
        const size_t common = cpp_fields.size() < glsl_fields.size()
                            ? cpp_fields.size() : glsl_fields.size();
        for (size_t i = 0; i < common; ++i)
        {
            if (cpp_fields[i].type != glsl_fields[i].type
             || cpp_fields[i].name != glsl_fields[i].name
             || cpp_fields[i].dim  != glsl_fields[i].dim)
            {
                result.diagnostics.emplace_back(
                    "EntityGPU 字段漂移：第 " + std::to_string(i) + " 项 C++="
                    + cpp_fields[i].type + " " + cpp_fields[i].name
                    + (cpp_fields[i].dim.empty() ? "" : "[" + cpp_fields[i].dim + "]")
                    + " GLSL=" + glsl_fields[i].type + " " + glsl_fields[i].name
                    + (glsl_fields[i].dim.empty() ? "" : "[" + glsl_fields[i].dim + "]")
                    + "；两端必须同序同型同长度");
                result.passed = false;
                return result;
            }
        }
        if (cpp_fields.size() != glsl_fields.size())
        {
            // 余量必须全是显式填充 `_pad*`，否则 = 一端多出真实字段
            const std::vector<StructFieldEntry> &extra
                = cpp_fields.size() > glsl_fields.size() ? cpp_fields : glsl_fields;
            for (size_t i = common; i < extra.size(); ++i)
            {
                if (extra[i].name.rfind("_pad", 0) != 0)
                {
                    result.diagnostics.emplace_back(
                        "EntityGPU 字段数不一致且余量不是显式填充：多余项 "
                        + extra[i].type + " " + extra[i].name
                        + "（C++= " + fmt(cpp_fields) + " | GLSL= " + fmt(glsl_fields)
                        + "）；GLSL 只容许行尾 _pad* 承担 alignas(64) 的 40 B 填充");
                    result.passed = false;
                    return result;
                }
            }
        }

        // GLSL 总存储尺寸必须恰为 128 B（数组步长契约：88 载荷 + 显式 _pad）
        std::string glsl_stripped = glsl_text;
        StripCommentTokens(glsl_stripped);
        const auto glsl_defines = CollectDefineConstants(glsl_stripped);
        size_t glsl_bytes = 0;
        for (const StructFieldEntry &f : glsl_fields)
        {
            size_t bytes = 0;
            if (!StructFieldStorageSize(f, glsl_defines, bytes))
            {
                result.diagnostics.emplace_back(
                    "EntityGPU GLSL 字段 " + f.name + " 的数组长度不是可求值常数："
                    + f.dim);
                result.passed = false;
                return result;
            }
            glsl_bytes += bytes;
        }
        if (glsl_bytes != 128)
        {
            result.diagnostics.emplace_back(
                "EntityGPUData 的 GLSL 总存储 = " + std::to_string(glsl_bytes)
                + " B，必须 == 128 B（sizeof(EntityGPU)；scalar layout 无隐式填充，"
                "差值必须由行尾 _pad 显式写死）");
        }

        result.passed = result.diagnostics.empty();
        return result;
    }

    /// C++ `LightRow`（inc/hgl/graph/ubo/LightRow.h）与 GLSL 侧 `LightRowData`
    /// （ShaderLibrary/ubo/scene_ubo.glsl）必须逐字段同序同型同长度（T6b）。
    /// 为什么必须门禁：这是光源的 64 B GPU 行 —— 漂移 = 光照按错偏移取值（方向/颜色/强度
    /// 静默变垃圾，画面偏暗或光从侧面来，而没有任何报错）。
    /// 手法与 E.entity-gpu-struct-parity 同（逐行取块器），差别两条：
    ///   ① 字段数必须**相等**（LightRow 的填充两端都显式写 `_pad*`，没有 alignas 余量）；
    ///   ② GLSL 总存储必须 == 64 B（数组步长契约；C++ 侧另有 sizeof 静态断言把守）。
    static GateResult RunLightRowStructParityCase()
    {
        GateResult result;
        result.name = "W.light-row-struct-parity";

        const std::string cpp_text =
            ReadFileText(RepoRootPath("inc/hgl/graph/ubo/LightRow.h"));
        const std::string glsl_text =
            ReadFileText(RepoRootPath("ShaderLibrary/ubo/light_row.glsl"));

        if (cpp_text.empty() || glsl_text.empty())
        {
            result.diagnostics.emplace_back(
                "无法读取 LightRow.h / ubo/light_row.glsl（结构对表的两端）");
            result.passed = result.diagnostics.empty();
            return result;
        }

        const std::vector<StructFieldEntry> cpp_fields =
            ExtractStructFieldsLineWise(cpp_text, "LightRow");
        const std::vector<StructFieldEntry> glsl_fields =
            ExtractStructFieldsLineWise(glsl_text, "LightRowData");

        const auto fmt = [](const std::vector<StructFieldEntry> &fields) -> std::string
        {
            std::string out;
            for (const StructFieldEntry &f : fields)
            {
                if (!out.empty()) out += ", ";
                out += f.type + " " + f.name;
                if (!f.dim.empty()) out += "[" + f.dim + "]";
            }
            return out;
        };

        if (cpp_fields.empty() || glsl_fields.empty())
        {
            result.diagnostics.emplace_back(
                std::string("LightRow 字段提取为空（结构标记被改写？）取块器 = 逐行式，")
                + "C++= " + fmt(cpp_fields) + " | GLSL= " + fmt(glsl_fields));
            result.passed = result.diagnostics.empty();
            return result;
        }

        if (cpp_fields.size() != glsl_fields.size())
        {
            result.diagnostics.emplace_back(
                "LightRow 字段数不一致（C++ " + std::to_string(cpp_fields.size())
                + " 项 / GLSL " + std::to_string(glsl_fields.size())
                + " 项）：两端逐字段同序同型，填充也必须两端显式写 `_pad*`"
                "（C++= " + fmt(cpp_fields) + " | GLSL= " + fmt(glsl_fields) + "）");
            result.passed = false;
            return result;
        }

        for (size_t i = 0; i < cpp_fields.size(); ++i)
        {
            if (cpp_fields[i].type != glsl_fields[i].type
             || cpp_fields[i].name != glsl_fields[i].name
             || cpp_fields[i].dim  != glsl_fields[i].dim)
            {
                result.diagnostics.emplace_back(
                    "LightRow 字段漂移：第 " + std::to_string(i) + " 项 C++="
                    + cpp_fields[i].type + " " + cpp_fields[i].name
                    + (cpp_fields[i].dim.empty() ? "" : "[" + cpp_fields[i].dim + "]")
                    + " GLSL=" + glsl_fields[i].type + " " + glsl_fields[i].name
                    + (glsl_fields[i].dim.empty() ? "" : "[" + glsl_fields[i].dim + "]")
                    + "；两端必须同序同型同长度（改一侧必改另一侧）");
                result.passed = false;
                return result;
            }
        }

        // GLSL 总存储必须恰为 64 B（行步长契约；不写行尾 `_pad1[2]` 会得 48 B）
        std::string glsl_stripped = glsl_text;
        StripCommentTokens(glsl_stripped);
        const auto glsl_defines = CollectDefineConstants(glsl_stripped);
        size_t glsl_bytes = 0;
        for (const StructFieldEntry &f : glsl_fields)
        {
            size_t bytes = 0;
            if (!StructFieldStorageSize(f, glsl_defines, bytes))
            {
                result.diagnostics.emplace_back(
                    "LightRow GLSL 字段 " + f.name + " 的数组长度不是可求值常数：" + f.dim);
                result.passed = false;
                return result;
            }
            glsl_bytes += bytes;
        }
        if (glsl_bytes != 64)
        {
            result.diagnostics.emplace_back(
                "LightRowData 的 GLSL 总存储 = " + std::to_string(glsl_bytes)
                + " B，必须 == 64 B（sizeof(LightRow)；scalar layout 无隐式填充，"
                  "差值必须由行尾 `_pad` 显式写死）");
        }

        result.passed = result.diagnostics.empty();
        return result;
    }

    /// X 列表（`HGL_MESH_DRAW_PARAMS_FIELD_LIST`，ShaderBufferSources.h）前 9 个头部
    /// 字段与剔除着色器里的**头部镜像**（`MeshDrawParamsHeadData`，entity_gpu_cull.glsl）
    /// 必须逐字段同序同型（E.entity-gpu-struct-parity 同族手法；§7.6 取块器复用）。
    /// 为什么必须门禁（T3f-2 形态 B）：镜像是 gather 里 `groupCountX` 的**唯一读源**
    /// ——X 列表头部漂移（改名/调序/插字段）而镜像没跟 ⇒ CS 按错偏移读 is_indexed/
    /// index_count/vertex_count ⇒ 组数算错 = 静默画错/画漏，实机与现有门都抓不到。
    /// 镜像只承诺头部前 9 项（尾部 11×uint64 地址不进镜像 ⇒ 不做全表前缀断言），
    /// 但**步长必须 ==104**（池行 sizeof 契约；`_pad` 承担余量，与 entity_gpu 同纪律）。
    static GateResult RunMeshDrawParamsHeadParityCase()
    {
        GateResult result;
        result.name = "E.mesh-draw-params-head-parity";

        const std::string cpu_text =
            ReadFileText(RepoRootPath("inc/hgl/graph/ShaderBufferSources.h"));
        const std::string glsl_text =
            ReadFileText(RepoRootPath("ShaderLibrary/ecs/entity_gpu_cull.glsl"));

        if (cpu_text.empty() || glsl_text.empty())
        {
            result.diagnostics.emplace_back(
                "无法读取 ShaderBufferSources.h / entity_gpu_cull.glsl（头部对表的两端）");
            result.passed = result.diagnostics.empty();
            return result;
        }

        // 端 1：X 列表前 9 项 `M(name, "glsl_type", cpu_type)`（宏定义体按行取）。
        const size_t xl = cpu_text.find("#define HGL_MESH_DRAW_PARAMS_FIELD_LIST(M)");
        if (xl == std::string::npos)
        {
            result.diagnostics.emplace_back(
                "X 列表宏 `HGL_MESH_DRAW_PARAMS_FIELD_LIST` 未找到（改名?）");
            result.passed = false;
            return result;
        }

        std::vector<StructFieldEntry> x_fields;
        {
            std::istringstream stream(cpu_text.substr(xl));
            std::string line;
            while (std::getline(stream, line))
            {
                // X 列表体每行 = `M(name, "type", cpu_type) \`；首个不以 M( 开头的
                // 非续行即列表结束（`#undef` 前）
                const size_t m = line.find("M(");
                if (m == std::string::npos)
                {
                    if (!x_fields.empty())
                        break;
                    continue;
                }

                // name
                const size_t n0 = m + 2;
                const size_t n1 = line.find(',', n0);
                // "glsl_type"
                const size_t q0 = line.find('"', n1);
                const size_t q1 = line.find('"', q0 + 1);
                if (n1 == std::string::npos || q0 == std::string::npos
                 || q1 == std::string::npos)
                {
                    result.diagnostics.emplace_back(
                        "X 列表行解析失败: " + line);
                    break;
                }

                StructFieldEntry f;
                f.name = line.substr(n0, n1 - n0);
                // 去名字前后空白
                const size_t ns = f.name.find_first_not_of(" \t\r");
                const size_t ne = f.name.find_last_not_of(" \t\r");
                f.name = (ns == std::string::npos) ? "" : f.name.substr(ns, ne - ns + 1);
                f.type = line.substr(q0 + 1, q1 - q0 - 1);   // GLSL 类型原文
                f.dim.clear();
                NormalizeStructField(f);
                x_fields.push_back(f);
            }
        }

        if (x_fields.size() < 9)
        {
            result.diagnostics.emplace_back(
                "X 列表头部取项异常（前 9 项要求，实取 " + std::to_string(x_fields.size()) + "）");
            result.passed = false;
            return result;
        }
        x_fields.resize(9);   // 镜像只承诺头部 9 项

        // 端 2：GLSL `MeshDrawParamsHeadData`（逐行取块器；_pad 是余量承担者）。
        const std::vector<StructFieldEntry> glsl_fields_all =
            ExtractStructFieldsLineWise(glsl_text, "MeshDrawParamsHeadData");

        std::vector<StructFieldEntry> glsl_head;
        for (const StructFieldEntry &f : glsl_fields_all)
        {
            if (f.name.rfind("_pad", 0) == 0)
                continue;
            glsl_head.push_back(f);
        }

        if (glsl_head.empty())
        {
            result.diagnostics.emplace_back(
                "MeshDrawParamsHeadData 字段提取为空（结构标记被改写？取块器 = 逐行式）");
            result.passed = false;
            return result;
        }

        // 逐字段同序同型（X 列表的 dim 恒空：头部 9 项全是标量）
        const size_t common = glsl_head.size() < x_fields.size()
                            ? glsl_head.size() : x_fields.size();
        for (size_t i = 0; i < common; ++i)
        {
            if (x_fields[i].type != glsl_head[i].type
             || x_fields[i].name != glsl_head[i].name)
            {
                result.diagnostics.emplace_back(
                    "MeshDrawParams 头部漂移：第 " + std::to_string(i) + " 项 X列表="
                    + x_fields[i].type + " " + x_fields[i].name + " / GLSL 镜像="
                    + glsl_head[i].type + " " + glsl_head[i].name
                    + "；gather 的 groupCountX 读源失效（画错组数=静默回归）");
                result.passed = false;
                return result;
            }
        }
        if (glsl_head.size() != x_fields.size())
        {
            result.diagnostics.emplace_back(
                "头部项数不一致（余量必须全由 _pad 承担）：X 前 9 / 镜像非 pad 项 "
                + std::to_string(glsl_head.size()));
            result.passed = false;
            return result;
        }

        // 步长契约：载荷 + _pad == 128（池行 sizeof；scalar layout 无隐式填充）
        std::string glsl_stripped = glsl_text;
        StripCommentTokens(glsl_stripped);
        const auto glsl_defines = CollectDefineConstants(glsl_stripped);

        size_t glsl_bytes = 0;
        for (const StructFieldEntry &f : glsl_fields_all)
        {
            size_t bytes = 0;
            if (!StructFieldStorageSize(f, glsl_defines, bytes))
            {
                result.diagnostics.emplace_back(
                    "MeshDrawParamsHeadData 字段 " + f.name + " 的长度不可求值：" + f.dim);
                result.passed = false;
                return result;
            }
            glsl_bytes += bytes;
        }

        // CPU 端对照：X 列表头部 9 项 = 4B×8 + float×1 = 36 B（sizeof 断言在头文件里，
        // 门里核对镜像总尺寸 == 池行 104 即可；36+68=104 是本用例的算术前提）
        if (glsl_bytes != 104)
        {
            result.diagnostics.emplace_back(
                "MeshDrawParamsHeadData 的 GLSL 总存储 = " + std::to_string(glsl_bytes)
                + " B，必须 == 104 B（池行 sizeof；差值必须由行尾 _pad 显式写死）");
        }

        result.passed = result.diagnostics.empty();
        return result;
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
        { "addr_color_palette",         false },
        { "addr_viewport",              false },
    };

    /// 「表归属」契约（§6 契约 ④）：`world_side=true` 检查世界表，`false` 检查全局表。
    ///
    /// 两条判据（都按**字段集合**比对，与顺序无关；顺序由 parity 用例各管）：
    /// ① 该侧（C++ 与 GLSL 两端）提取出的字段集合 == 分类表里登记的该侧集合
    ///    —— 多出来的是「未分类字段」（新字段忘了登记 / 放错表），少了的是「漏字段」；
    /// ② 该侧字段**不得**出现在另一侧 —— 放错表就是跨世界串数据：世界私有字段（相机行 /
    ///    实体行表 / 实体 4-ID 表 / L2W / 光源行 / 影子数据）一旦落进设备级表，就退化成
    ///    "所有世界共用一份"，多世界同帧互相覆写。
    static GateResult RunAddressOwnershipCase(const bool world_side)
    {
        GateResult result;
        result.name = world_side ? "W.world-addresses-field-ownership"
                                 : "S.global-addresses-field-ownership";

        const std::string world_cpp =
            ReadFileText(RepoRootPath("inc/hgl/graph/ubo/WorldAddresses.h"));
        const std::string global_cpp =
            ReadFileText(RepoRootPath("inc/hgl/graph/ubo/GlobalAddresses.h"));
        const std::string world_glsl =
            ReadFileText(RepoRootPath("ShaderLibrary/ubo/world_addresses.glsl"));
        const std::string global_glsl =
            ReadFileText(RepoRootPath("ShaderLibrary/ubo/global_addresses.glsl"));

        if (world_cpp.empty() || global_cpp.empty()
         || world_glsl.empty() || global_glsl.empty())
        {
            result.diagnostics.emplace_back(
                "无法读取 WorldAddresses.h / GlobalAddresses.h / "
                "ubo/world_addresses.glsl / ubo/global_addresses.glsl");
            result.passed = result.diagnostics.empty();
            return result;
        }

        const std::vector<std::string> world_fields_cpp =
            ExtractU64Fields(world_cpp, "WorldAddresses");
        const std::vector<std::string> world_fields_glsl =
            ExtractU64Fields(world_glsl, "WorldAddressesRef");
        const std::vector<std::string> global_fields_cpp =
            ExtractU64Fields(global_cpp, "GlobalAddresses");
        const std::vector<std::string> global_fields_glsl =
            ExtractU64Fields(global_glsl, "GlobalAddressesRef");

        const std::vector<std::string> &this_cpp  = world_side ? world_fields_cpp  : global_fields_cpp;
        const std::vector<std::string> &this_glsl = world_side ? world_fields_glsl : global_fields_glsl;
        const std::vector<std::string> &other_cpp = world_side ? global_fields_cpp : world_fields_cpp;
        const std::vector<std::string> &other_glsl= world_side ? global_fields_glsl: world_fields_glsl;
        const char *this_name  = world_side ? "WorldAddresses" : "GlobalAddresses";
        const char *other_name = world_side ? "GlobalAddresses" : "WorldAddresses";

        if (this_cpp.empty() || this_glsl.empty())
        {
            result.diagnostics.emplace_back(
                std::string(this_name) + " 字段提取为空（结构标记被改写？）C++="
                + std::to_string(this_cpp.size()) + " GLSL=" + std::to_string(this_glsl.size()));
            result.passed = result.diagnostics.empty();
            return result;
        }

        // 分类表里登记的本侧字段集合
        std::vector<std::string> expected;
        for (const AddressOwnershipEntry &entry : kAddressOwnershipTable)
        {
            if (entry.world_owned == world_side)
                expected.emplace_back(entry.field);
        }

        const auto contains = [](const std::vector<std::string> &list, const std::string &name)
        {
            for (const std::string &item : list)
                if (item == name)
                    return true;
            return false;
        };

        // ① 该侧字段集合必须与登记集合一致
        for (const std::string &field : this_cpp)
        {
            if (!contains(expected, field))
            {
                result.diagnostics.emplace_back(
                    std::string(this_name) + " 出现未分类字段 `" + field
                    + "`（C++ 侧）：请在 ShaderResourceSchemaRegressionGate 的 kAddressOwnershipTable"
                      " 里登记它属于世界私有还是设备级全局");
            }
        }
        for (const std::string &field : this_glsl)
        {
            if (!contains(expected, field))
            {
                result.diagnostics.emplace_back(
                    std::string(this_name) + " 出现未分类字段 `" + field
                    + "`（GLSL 侧）：同上，先在 kAddressOwnershipTable 登记");
            }
        }
        for (const std::string &field : expected)
        {
            if (!contains(this_cpp, field))
            {
                result.diagnostics.emplace_back(
                    std::string(this_name) + " 缺少登记字段 `" + field
                    + "`（C++ 侧）：字段被删/改名后请同步 kAddressOwnershipTable");
            }
            if (!contains(this_glsl, field))
            {
                result.diagnostics.emplace_back(
                    std::string(this_name) + " 缺少登记字段 `" + field + "`（GLSL 侧）");
            }
        }

        // ② 该侧字段不得出现在另一侧
        for (const std::string &field : other_cpp)
        {
            if (contains(expected, field))
            {
                result.diagnostics.emplace_back(
                    std::string("字段 `") + field + "` 出现在 " + other_name
                    + "（C++ 侧）却在分类表里登记为 " + this_name
                    + " —— 放错表：世界私有字段进设备级表会让多世界共用一份、同帧互相覆写");
            }
        }
        for (const std::string &field : other_glsl)
        {
            if (contains(expected, field))
            {
                result.diagnostics.emplace_back(
                    std::string("字段 `") + field + "` 出现在 " + other_name + "（GLSL 侧）却在分类表里登记为 "
                    + this_name + " —— 放错表");
            }
        }

        result.passed = result.diagnostics.empty();
        return result;
    }

    // 阶段三之三：材质定义有**两个**来源 —— 内嵌 builtin（C++ 代码）与资产目录
    // （阶段四起为 `.mi.toml`）。这里复刻运行时的注册顺序（先内嵌、后目录），
    // 因为后续比对要求 registry 拿到的是完整集合。
    static int RegisterBuiltinMaterialDefinitions(MaterialDefinitionFileRegistry &registry)
    {
        int count = 0;
        for (int i = 0; i < GetBuiltinMaterialDefinitionCount(); ++i)
        {
            const BuiltinMaterialDefinitionSource *source =
                GetBuiltinMaterialDefinitionSource(i);
            if (!source || !source->schema4_source)
                continue;
            if (registry.RegisterDefinitionSource(
                    source->source_label, source->schema4_source,
                    static_cast<int>(std::strlen(source->schema4_source))))
                ++count;
        }
        return count;
    }

    static GateResult RunMaterialDefinitionFileSchemaCase()
    {
        GateResult result;
        result.name = "S.material-definition-file-schema";

        const char material_file[] =
            "schema = 4\n"
            "id = \"LitFile\"\n"
            "name = \"LitFile\"\n"
            "provider_policy = \"AllowDerived\"\n"
            "[transform]\n"
            "source = \"Vec3Position\"\n"
            "mapping = \"Passthrough3D\"\n"
            "orientation = \"World\"\n"
            "scale = \"World\"\n"
            "projection = \"WorldCameraVP\"\n"
            "[fragment]\n"
            "material_source_module_name = \"pbr_surface_source\"\n"
            "ntb_module_name = \"ntb_tangent_vbo_normalmap\"\n"
            "[render_state]\n"
            "alpha_test = true\n"
            "alpha_cutoff = 0.25\n"
            "[vertex]\n"
            "requirements = [\"Position\", \"UV0\", \"Normal\"]\n"
            "varyings = [\"emit_world_pos\", \"emit_world_normal\", \"emit_uv0\"]\n"
            "[resources]\n"
            "";

        MaterialDefinitionFileData data;
        const auto parse = ParseMaterialDefinitionFile(
            material_file, static_cast<int>(std::strlen(material_file)), data);
        if (parse != MaterialDefinitionFileParseResult::OK)
        {
            result.diagnostics.emplace_back(
                std::string("material schema parse failed: ")
                + GetMaterialDefinitionFileParseResultName(parse));
        }
        else
        {
            const auto &definition = data.definition;
            if (definition.definition_id != "LitFile"
             || definition.vertex_provider_policy != MaterialVertexProviderPolicy::AllowDerived
             || definition.vertex_node_config.position_mapping != PositionMappingMode::Passthrough3D
             || definition.vertex_semantic_requirements.GetCount() != 3
             || definition.texture_configuration_max_count
                    != DefaultMaterialTextureConfigurationCapacity
             || !ResolveMaterialRenderState(
                    definition, MaterialRecipe{}).alpha_test)
            {
                result.diagnostics.emplace_back("material schema fields mismatch");
            }

            const GeometryVertexFormat allow_derived_geometry{
                {VertexSemantic::Position, VF_V3F},
                {VertexSemantic::TexCoord, VF_V2F},
                {VertexSemantic::Normal, VF_V3F}
            };
            MaterialDefinitionBuildRequest allow_derived_request{};
            allow_derived_request.geometry_vertex_format = &allow_derived_geometry;
            MaterialResolvedVertexABI allow_derived_abi{};
            if (!BuildResolvedMaterialVertexABI(
                    definition, allow_derived_request, allow_derived_abi)
             || allow_derived_abi.vertex_input_glsl.IsEmpty()
             || allow_derived_abi.position_format != VK_FORMAT_R32G32B32_SFLOAT)
            {
                result.diagnostics.emplace_back(
                    "AllowDerived material definition must build a SSBO vertex ABI");
            }

            // 打包 int16x2 位置（4B/顶点）必须映射到 vec2i 模块——这是三对闭环格式之一。
            // 该格式产出 `vec2 Position`，因此只能配 2D 映射的材质；`LitFile` 是
            // Passthrough3D（要 3 分量），直接配它会被生成期硬门拦下。
            MaterialDefinition packed_position_definition = definition;
            packed_position_definition.vertex_node_config.position_mapping =
                PositionMappingMode::PixelToLocal;

            const GeometryVertexFormat packed_position_geometry{
                {VertexSemantic::Position, VF_V2I16},
                {VertexSemantic::TexCoord, VF_V2F},
                {VertexSemantic::Normal, VF_V3F}
            };
            MaterialDefinitionBuildRequest packed_position_request{};
            packed_position_request.geometry_vertex_format = &packed_position_geometry;
            MaterialResolvedVertexABI packed_position_abi{};
            if (!BuildResolvedMaterialVertexABI(
                    packed_position_definition, packed_position_request, packed_position_abi)
             || packed_position_abi.position_format != VK_FORMAT_R16G16_SINT
             || !std::strstr(packed_position_abi.vertex_input_glsl.c_str(),
                             "s1_position_vec2i.glsl"))
            {
                result.diagnostics.emplace_back(
                    "packed int16x2 position must resolve to the vec2i module");
            }

            // 反向：分量数不匹配时必须硬失败（历史上这种组合会静默产出一份编不过的 GLSL，
            // glslang 再把错误指向 s2 模块，定位成本极高）。
            MaterialResolvedVertexABI mismatch_abi{};
            MaterialDefinitionBuildRequest mismatch_request{};
            mismatch_request.geometry_vertex_format = &packed_position_geometry;
            if (BuildResolvedMaterialVertexABI(
                    definition, mismatch_request, mismatch_abi))
            {
                result.diagnostics.emplace_back(
                    "vec2 position with Passthrough3D mapping must be rejected");
            }

            // 未闭环位置格式必须被拒绝，而不是挑一个"同义"模块凑合：
            // R16G16_SFLOAT(4B) 曾被当成 Vec2Position（读 8B），R32G32B32A32_SFLOAT(16B)
            // 曾被当成 Vec3Position（读 12B），两者都产出顶点全体错位的着色器。
            const VkFormat unsupported_position_formats[] = {
                VF_V2HF, VF_V2I, VF_V2U, VF_V2U16, VF_V4F
            };
            for (const VkFormat unsupported_format : unsupported_position_formats)
            {
                const GeometryVertexFormat unsupported_geometry{
                    {VertexSemantic::Position, unsupported_format},
                    {VertexSemantic::TexCoord, VF_V2F},
                    {VertexSemantic::Normal, VF_V3F}
                };
                MaterialDefinitionBuildRequest unsupported_request{};
                unsupported_request.geometry_vertex_format = &unsupported_geometry;
                MaterialResolvedVertexABI unsupported_abi{};
                if (BuildResolvedMaterialVertexABI(definition, unsupported_request, unsupported_abi))
                {
                    result.diagnostics.emplace_back(
                        std::string("unsupported vertex position format must be rejected: VkFormat=")
                        + std::to_string(static_cast<int>(unsupported_format)));
                }
            }
        }

        const char texture_layout_file[] =
            "schema = 4\n"
            "id = \"TextureLayoutFile\"\n"
            "name = \"TextureLayoutFile\"\n"
            "provider_policy = \"GeometryOnly\"\n"
            "[fragment]\n"
            "material_source_module_name = \"texture_source\"\n"
            "[vertex]\n"
            "requirements = [\"Position\", \"UV0\"]\n"
            "[resources]\n"
            "textures = [\n"
            "    { name = \"base_color\", sampler = \"Sampler2DArray\", required = true },\n"
            "    { name = \"detail_mask\", sampler = \"Sampler2D\", required = false }\n"
            "]\n"
            "[resources.texture_configurations]\n"
            "max_count = 37\n";
        MaterialDefinitionFileData texture_layout_data;
        if (ParseMaterialDefinitionFile(
                texture_layout_file,
                static_cast<int>(std::strlen(texture_layout_file)),
                texture_layout_data)
                != MaterialDefinitionFileParseResult::OK)
        {
            result.diagnostics.emplace_back(
                "schema 3 texture-reference layout file must parse");
        }
        else
        {
            const MaterialDefinition &definition =
                texture_layout_data.definition;
            MaterialTextureReferenceLayout layout{};
            if (!BuildMaterialTextureReferenceLayout(definition, layout)
             || definition.texture_declarations.size() != 2
             || definition.texture_configuration_max_count != 37
             || layout.reference_count != 2
             || layout.row_stride != 16
             || layout.max_configuration_count != 37
             || layout.layout_hash == 0
             || FindMaterialTextureDeclaration(
                    definition, "base_color") != 0
             || FindMaterialTextureDeclaration(
                    definition, "detail_mask") != 1
             || FindMaterialTextureDeclaration(
                    definition, "missing_texture") != -1
             || !IsMaterialTextureArraySampler(
                    definition.texture_declarations[0].sampler_type)
             || IsMaterialTextureArraySampler(
                    definition.texture_declarations[1].sampler_type)
)
            {
                result.diagnostics.emplace_back(
                    "texture-reference layout fields mismatch");
            }

            MaterialDefinition changed_capacity = definition;
            changed_capacity.texture_configuration_max_count = 91;
            MaterialTextureReferenceLayout changed_layout{};
            if (!BuildMaterialTextureReferenceLayout(
                    changed_capacity, changed_layout)
             || changed_layout.layout_hash != layout.layout_hash
             || changed_layout.row_stride != layout.row_stride
             || changed_layout.max_configuration_count != 91)
            {
                result.diagnostics.emplace_back(
                    "texture configuration capacity must not alter shader ABI");
            }

            const uint64_t pool_key =
                MaterialTextureReferencePool::MakePoolKey(
                    definition, layout);
            const uint64_t changed_capacity_pool_key =
                MaterialTextureReferencePool::MakePoolKey(
                    changed_capacity, changed_layout);
            MaterialDefinition different_definition = definition;
            different_definition.definition_id = "OtherTextureLayoutFile";
            const uint64_t different_definition_pool_key =
                MaterialTextureReferencePool::MakePoolKey(
                    different_definition, layout);
            MaterialTextureConfigurationAllocation empty_allocation{};
            if (pool_key == 0
             || changed_capacity_pool_key == 0
             || different_definition_pool_key == 0
             || pool_key == changed_capacity_pool_key
             || pool_key == different_definition_pool_key
             || empty_allocation.IsValid())
            {
                result.diagnostics.emplace_back(
                    "texture-reference pool identity or empty allocation is invalid");
            }

        }

        const char invalid_texture_configuration_file[] =
            "schema = 4\n"
            "id = \"InvalidTextureConfiguration\"\n"
            "name = \"InvalidTextureConfiguration\"\n"
            "provider_policy = \"GeometryOnly\"\n"
            "[fragment]\n"
            "material_source_module_name = \"texture_source\"\n"
            "[vertex]\n"
            "requirements = [\"Position\", \"UV0\"]\n"
            "[resources]\n"
            "textures = [{ name = \"base_color\", sampler = \"Sampler2D\", required = true }]\n"
            "[resources.texture_configurations]\n"
            "max_count = 0\n";
        MaterialDefinitionFileData invalid_texture_configuration_data;
        if (ParseMaterialDefinitionFile(
                invalid_texture_configuration_file,
                static_cast<int>(std::strlen(
                    invalid_texture_configuration_file)),
                invalid_texture_configuration_data)
                != MaterialDefinitionFileParseResult::InvalidValue)
        {
            result.diagnostics.emplace_back(
                "zero texture configuration capacity must be rejected");
        }

        const char duplicate_texture_name_file[] =
            "schema = 4\n"
            "id = \"DuplicateTextureName\"\n"
            "name = \"DuplicateTextureName\"\n"
            "provider_policy = \"GeometryOnly\"\n"
            "[fragment]\n"
            "material_source_module_name = \"texture_source\"\n"
            "[vertex]\n"
            "requirements = [\"Position\", \"UV0\"]\n"
            "[resources]\n"
            "textures = [\n"
            "    { name = \"base_color\", sampler = \"Sampler2D\", required = true },\n"
            "    { name = \"base_color\", sampler = \"Sampler2D\", required = false }\n"
            "]\n";
        MaterialDefinitionFileData duplicate_texture_name_data;
        if (ParseMaterialDefinitionFile(
                duplicate_texture_name_file,
                static_cast<int>(std::strlen(duplicate_texture_name_file)),
                duplicate_texture_name_data)
                != MaterialDefinitionFileParseResult::InvalidValue)
        {
            result.diagnostics.emplace_back(
                "duplicate material texture names must be rejected");
        }

        // S2（2026-10-11）：材质侧纹理采样选项（filter/wrap/swizzle/anisotropy/lod/compare_op）整删
        // —— 这些 authoring 键现在是**未知键**，必须被 ValidateTextureDeclarationKeys 拒绝。
        // （原先此处测的是"swizzle 值非法"，机制换了、意图保留：畸形纹理声明不得静默通过。）
        const char retired_sampling_key_file[] =
            "schema = 4\n"
            "id = \"RetiredSamplingKey\"\n"
            "name = \"RetiredSamplingKey\"\n"
            "provider_policy = \"GeometryOnly\"\n"
            "[fragment]\n"
            "material_source_module_name = \"texture_source\"\n"
            "[vertex]\n"
            "requirements = [\"Position\", \"UV0\"]\n"
            "[resources]\n"
            "textures = [{ name = \"base_color\", sampler = \"Sampler2D\", required = true, swizzle = \"BGR1\" }]\n";
        MaterialDefinitionFileData retired_sampling_key_data;
        if (ParseMaterialDefinitionFile(
                retired_sampling_key_file,
                static_cast<int>(std::strlen(retired_sampling_key_file)),
                retired_sampling_key_data)
                != MaterialDefinitionFileParseResult::InvalidValue)
        {
            result.diagnostics.emplace_back(
                "retired texture sampling keys must be rejected as unknown keys");
        }

        const char unknown_table_file[] =
            "schema = 4\n"
            "id = \"UnknownTable\"\n"
            "name = \"UnknownTable\"\n"
            "provider_policy = \"GeometryOnly\"\n"
            "[fragment]\n"
            "material_source_module_name = \"unlit_source\"\n"
            "[unknown]\n"
            "value = true\n"
            "[vertex]\n"
            "requirements = [\"Position\"]\n";
        MaterialDefinitionFileData unknown_table_data;
        if (ParseMaterialDefinitionFile(
                unknown_table_file,
                static_cast<int>(std::strlen(unknown_table_file)),
                unknown_table_data)
                != MaterialDefinitionFileParseResult::UnknownKey)
        {
            result.diagnostics.emplace_back(
                "unknown material table must be rejected");
        }

        const char invalid_file[] =
            "schema = 4\n"
            "id = \"Broken\"\n"
            "name = \"Broken\"\n"
            "source = \"file\"\n"
            "bootstrap = \"None\"\n"
            "provider_policy = \"GeometryOnly\"\n"
            "[fragment]\n"
            "material_source_module_name = \"unlit_source\"\n"
            "[vertex]\n"
            "requirements = [\"Position\"]\n";
        MaterialDefinitionFileData invalid_data;
        if (ParseMaterialDefinitionFile(
                invalid_file, static_cast<int>(std::strlen(invalid_file)), invalid_data)
                != MaterialDefinitionFileParseResult::UnknownKey)
        {
            result.diagnostics.emplace_back(
                "removed source/bootstrap metadata must be rejected");
        }

        // 阶段四：领域材质不再需要抄写 `name` / `provider_policy` / `[fragment]` /
        // `[vertex] requirements` —— 全部由领域契约给出。这条探针钉死
        // 「推导结果 == 契约」：抄漏不会报错，只会在运行期拿缺属性的几何喂管线。
        {
            const char derived_source[] =
                "schema = 4\n"
                "id = \"DerivedVertexColor\"\n"
                "domain = \"VertexColor\"\n"
                "[vertex]\n"
                "varyings = [\"emit_vertex_color\"]\n";

            MaterialDefinitionFileData derived_data;
            const auto derived_parse = ParseMaterialDefinitionFile(
                derived_source,
                static_cast<int>(std::strlen(derived_source)),
                derived_data);
            if (derived_parse != MaterialDefinitionFileParseResult::OK)
            {
                result.diagnostics.emplace_back(
                    std::string("domain-derived material parse failed: ")
                    + GetMaterialDefinitionFileParseResultName(derived_parse)
                    + " table=\"" + derived_data.parse_error.table
                    + "\" key=\"" + derived_data.parse_error.key
                    + "\" value=\"" + derived_data.parse_error.value
                    + "\" detail=\"" + derived_data.parse_error.detail + "\"");
            }
            else
            {
                const auto &derived = derived_data.definition;
                if (derived.definition_name != "DerivedVertexColor")
                {
                    result.diagnostics.emplace_back(
                        "omitted name must default to the definition id");
                }
                if (derived.vertex_provider_policy
                        != MaterialVertexProviderPolicy::GeometryOnly)
                {
                    result.diagnostics.emplace_back(
                        "omitted provider_policy must follow the domain contract");
                }
                if (!HasMaterialVertexSemanticRequirement(derived, VertexSemantic::Position)
                 || !HasMaterialVertexSemanticRequirement(derived, VertexSemantic::Color)
                 || derived.vertex_semantic_requirements.GetCount() != 2)
                {
                    result.diagnostics.emplace_back(
                        "omitted requirements must equal the domain contract mask");
                }
                if (!derived.material_source_module_name
                 || std::strcmp(derived.material_source_module_name,
                                "vertex_color_source") != 0)
                {
                    result.diagnostics.emplace_back(
                        "omitted [fragment] must be filled from the domain contract");
                }
            }

            // 反向探针：非领域材质没有契约可依，缺 `provider_policy` 必须带键名报错。
            const char custom_no_policy_source[] =
                "schema = 4\n"
                "id = \"CustomNoPolicy\"\n"
                "[fragment]\n"
                "material_source_module_name = \"pbr_surface_source\"\n"
                "[vertex]\n"
                "requirements = [\"Position\"]\n";

            MaterialDefinitionFileData custom_no_policy_data;
            const auto custom_no_policy_parse = ParseMaterialDefinitionFile(
                custom_no_policy_source,
                static_cast<int>(std::strlen(custom_no_policy_source)),
                custom_no_policy_data);
            if (custom_no_policy_parse == MaterialDefinitionFileParseResult::OK)
            {
                result.diagnostics.emplace_back(
                    "non-domain material without provider_policy must be rejected");
            }
            else if (custom_no_policy_data.parse_error.key != "provider_policy")
            {
                result.diagnostics.emplace_back(
                    "non-domain provider_policy rejection must report key "
                    "\"provider_policy\", got \""
                    + custom_no_policy_data.parse_error.key + "\"");
            }

            // 推导结果**不得**与 `varyings` 取交集：领域契约的语义位是顶点 ABI
            // （同领域必须同布局才能共享 layout/合批），`varyings` 只决定着色器
            // 发什么插值。DebugNormal 契约要求 Position+Normal，这份材质虽然不发
            // world normal，Normal 仍必须留在需求里 —— 否则它的顶点布局会与同领域
            // 其它材质分叉，领域契约校验会直接拒绝（这条探针就是那次拒绝的锚点）。
            const char trimmed_source[] =
                "schema = 4\n"
                "id = \"DerivedKeepAbi\"\n"
                "domain = \"DebugNormal\"\n"
                "[vertex]\n"
                "varyings = [\"emit_data_index_id\"]\n";

            MaterialDefinitionFileData trimmed_data;
            const auto trimmed_parse = ParseMaterialDefinitionFile(
                trimmed_source,
                static_cast<int>(std::strlen(trimmed_source)),
                trimmed_data);
            if (trimmed_parse != MaterialDefinitionFileParseResult::OK)
            {
                result.diagnostics.emplace_back(
                    std::string("domain-ABI material parse failed: ")
                    + GetMaterialDefinitionFileParseResultName(trimmed_parse)
                    + " table=\"" + trimmed_data.parse_error.table
                    + "\" key=\"" + trimmed_data.parse_error.key
                    + "\" value=\"" + trimmed_data.parse_error.value
                    + "\" detail=\"" + trimmed_data.parse_error.detail + "\"");
            }
            else
            {
                const auto &trimmed = trimmed_data.definition;
                if (trimmed.vertex_semantic_requirements.GetCount() != 2
                 || !HasMaterialVertexSemanticRequirement(
                        trimmed, VertexSemantic::Position)
                 || !HasMaterialVertexSemanticRequirement(
                        trimmed, VertexSemantic::Normal))
                {
                    result.diagnostics.emplace_back(
                        "derived requirements must keep the full domain ABI mask "
                        "regardless of the declared varyings");
                }
            }
        }

        // 反向探针：非领域材质没有契约可依，缺 `requirements` 必须带键名报错
        // （历史行为是静默 return false，写错只能靠猜）。
        const char custom_source[] =
            "schema = 4\n"
            "id = \"CustomNoRequirements\"\n"
            "provider_policy = \"GeometryOnly\"\n"
            "[fragment]\n"
            "material_source_module_name = \"pbr_surface_source\"\n"
            "[resources]\n";

        MaterialDefinitionFileData custom_data;
        const auto custom_parse = ParseMaterialDefinitionFile(
            custom_source,
            static_cast<int>(std::strlen(custom_source)),
            custom_data);
        if (custom_parse == MaterialDefinitionFileParseResult::OK)
        {
            result.diagnostics.emplace_back(
                "non-domain material without requirements must be rejected");
        }
        else if (custom_data.parse_error.key != "requirements")
        {
            result.diagnostics.emplace_back(
                "non-domain requirements rejection must report key "
                "\"requirements\", got \""
                + custom_data.parse_error.key + "\"");
        }

        MaterialDefinitionFileRegistry registry;
        const int builtin_count = RegisterBuiltinMaterialDefinitions(registry);
        if (builtin_count != 5)
        {
            result.diagnostics.emplace_back(
                "expected 5 embedded builtin material definitions, got "
                + std::to_string(builtin_count));
        }

        // 内嵌定义**不得**依赖资产目录：只喂内嵌源码的独立 registry 也必须能解析
        // 五个保底 ID。这是"保底物的可依赖性高于被兜底物"的机器化判据 ——
        // 谁把它们挪回 .mi.toml 资产目录，这条断言立刻变红。
        {
            MaterialDefinitionFileRegistry builtin_only;
            RegisterBuiltinMaterialDefinitions(builtin_only);
            static const char *fallback_ids[] = {
                "builtin/pure_color",
                "builtin/checkerboard_3d",
                "builtin/error_checkerboard_3d",
                "builtin/text_gpu",
                "builtin/text_gpu_bitmap"
            };
            for (const char *id : fallback_ids)
            {
                if (!builtin_only.FindByID(id))
                    result.diagnostics.emplace_back(
                        std::string("builtin definition must not depend on the asset "
                                    "directory: ") + id);
            }
        }

        int file_count = 0;
        int error_count = 0;
        if (!registry.LoadDirectory(hgl::ToOSString(GetShaderLibraryPath()),
                                    &file_count, &error_count)
         || file_count != 10
         || error_count != 0)
        {
            result.diagnostics.emplace_back("material file registry bulk load failed");
        }
        else
        {
            const char *expected_ids[] = {
                "Lit", "SkyMinimal", "SkyCube",
                "DebugNormalColor",
                "VertexColor", "UnlitTexture", "UnlitTexture3D",
                "VertexLuminance", "VertexPaletteColor",
                "builtin/pure_color",
                "builtin/checkerboard_2d",
                "builtin/checkerboard_3d",
                "builtin/error_checkerboard_3d",
                "builtin/text_gpu", "builtin/text_gpu_bitmap"
            };
            for (const char *id : expected_ids)
            {
                if (!registry.FindByID(id))
                    result.diagnostics.emplace_back(
                        std::string("missing bulk material definition: ") + id);
            }

        const char *bulk_ids[] = {
            "Lit", "SkyMinimal", "SkyCube",
            "DebugNormalColor", "VertexColor",
            "UnlitTexture",
            "VertexLuminance",
            "VertexPaletteColor",
            "builtin/pure_color",
            "builtin/checkerboard_2d",
            "builtin/checkerboard_3d",
            "builtin/error_checkerboard_3d",
            "builtin/text_gpu", "builtin/text_gpu_bitmap"
        };
        for (const char *id : bulk_ids)
        {
            const MaterialDefinition *file_definition = registry.FindByID(id);
            MaterialDefinition registry_definition{};
            if (!file_definition
             || !TryGetMaterialDefinitionByID(id, registry_definition))
            {
                result.diagnostics.emplace_back(
                    std::string("bulk file lookup failed: ") + id);
                continue;
            }

            if (HashResolvedMaterialRenderState(
                    ResolveMaterialRenderState(
                        *file_definition, MaterialRecipe{}))
                    != HashResolvedMaterialRenderState(
                        ResolveMaterialRenderState(
                            registry_definition, MaterialRecipe{}))
             || file_definition->vertex_provider_policy != registry_definition.vertex_provider_policy
             || file_definition->vertex_semantic_requirements.GetCount()
                    != registry_definition.vertex_semantic_requirements.GetCount()
             || file_definition->material_private_data
                    != registry_definition.material_private_data
             || file_definition->texture_declarations.size()
                    != registry_definition.texture_declarations.size()
             || file_definition->texture_configuration_max_count
                    != registry_definition.texture_configuration_max_count)
            {
                result.diagnostics.emplace_back(
                    std::string("bulk file definition contract mismatch: ") + id);
                continue;
            }

            for (int i = 0; i < file_definition->vertex_semantic_requirements.GetCount(); ++i)
            {
                if (!(file_definition->vertex_semantic_requirements[i]
                    == registry_definition.vertex_semantic_requirements[i]))
                {
                    result.diagnostics.emplace_back(
                        std::string("bulk semantic mismatch: ") + id);
                    break;
                }
            }

            if (VertexNodeConfigResolver::IsScreenLike(file_definition->vertex_node_config)
             && (file_definition->vertex_node_config.input != VertexInputMode::Vec2Position
              || file_definition->vertex_node_config.position_mapping != PositionMappingMode::NDCLift
              || file_definition->vertex_node_config.projection != ProjectionMode::LocalToWorldOnly))
            {
                result.diagnostics.emplace_back(
                    std::string("2D file node config mismatch: ") + id);
            }

            const ResolvedMaterialRenderState render_state =
                ResolveMaterialRenderState(
                    *file_definition, MaterialRecipe{});
            const bool is_text_material =
                std::strcmp(id, "builtin/text_gpu") == 0
             || std::strcmp(id, "builtin/text_gpu_bitmap") == 0;
            if (is_text_material)
            {
                if (!render_state.pipeline_config.alpha_blend)
                    result.diagnostics.emplace_back(
                        std::string("text material alpha blend mismatch: ")
                        + id);
                continue;
            }
            if (render_state.alpha_test || render_state.dither
             || render_state.pipeline_config.alpha_blend)
            {
                result.diagnostics.emplace_back(
                    std::string("opaque material render state mismatch: ")
                    + id);
            }
            MaterialCoverageContract coverage{};
            RenderTemplateRequest coverage_request{};
            RenderTemplateValidationDiagnostic coverage_diagnostic{};
            if (!ResolveSceneRenderTemplateRequest(
                    RenderTemplateID::ShadowCasterOpaque,
                    hgl::graph::ShaderStage::Fragment,
                    MakeShadowCasterProfile(),
                    coverage_request,
                    coverage_diagnostic)
             || !AppendMaterialRenderTemplateRoots(
                    *file_definition, coverage_request)
             || !BuildMaterialCoverageContract(
                    *file_definition, MaterialRecipe{}, coverage_request,
                    ShaderProgramPurpose::ShadowDepth, coverage)
             || coverage.requires_alpha_evaluation)
            {
                result.diagnostics.emplace_back(
                    std::string("opaque material shadow coverage mismatch: ")
                    + id);
            }
        }

        const char *text_ids[] = {
            "builtin/text_gpu", "builtin/text_gpu_bitmap"
        };
        for (const char *id : text_ids)
        {
            MaterialDefinition definition{};
            if (!TryGetMaterialDefinitionByID(id, definition)
             || !ResolveMaterialRenderState(
                    definition, MaterialRecipe{}).pipeline_config.alpha_blend)
            {
                result.diagnostics.emplace_back(
                    std::string("text material alpha blend state missing: ")
                    + id);
            }
        }

        }

        result.passed = result.diagnostics.empty();
        return result;
    }

    /// 解析失败细节回传（2026-10-12 用户口径）：材质配置写错必须能 LOG 到"表/键/值/行号"级别
    /// —— 发售口径是全预编译 SPV、届时不再有 shader 合成器，配置错误只能靠日志定位。
    /// 三个子项分别覆盖：枚举/名字值写错、键名写错、TOML 语法错。
    static GateResult RunMaterialDefinitionParseDetailCase()
    {
        GateResult result;
        result.name = "S.material-definition-parse-detail";

        // ① 采样器类型名写错：必须报到 键 + 值（本次口径的原始触发场景）
        const char bad_sampler_file[] =
            "schema = 4\n"
            "id = \"ParseDetailBadSampler\"\n"
            "name = \"ParseDetailBadSampler\"\n"
            "provider_policy = \"GeometryOnly\"\n"
            "[fragment]\n"
            "material_source_module_name = \"unlit_source\"\n"
            "[vertex]\n"
            "requirements = [\"Position\"]\n"
            "[resources]\n"
            "textures = [{ name = \"base_color\", sampler = \"SamplerBogus\", required = false }]\n";
        MaterialDefinitionFileData bad_sampler_data;
        if (ParseMaterialDefinitionFile(
                bad_sampler_file, static_cast<int>(std::strlen(bad_sampler_file)),
                bad_sampler_data) != MaterialDefinitionFileParseResult::InvalidValue)
        {
            result.diagnostics.emplace_back(
                "bogus sampler name must fail the material definition parse");
        }
        else if (bad_sampler_data.parse_error.key != "sampler"
              || bad_sampler_data.parse_error.value != "SamplerBogus")
        {
            result.diagnostics.emplace_back(std::string("bogus sampler name must report key+value, got key='")
                + bad_sampler_data.parse_error.key + "' value='"
                + bad_sampler_data.parse_error.value + "' (table='"
                + bad_sampler_data.parse_error.table + "')");
        }

        // ② 未知键：必须报到键名（键白名单是"作者笔误 / 已删字段残留"的第一道闸）
        const char unknown_key_file[] =
            "schema = 4\n"
            "id = \"ParseDetailUnknownKey\"\n"
            "name = \"ParseDetailUnknownKey\"\n"
            "provider_policy = \"GeometryOnly\"\n"
            "bogus_root_key = 1\n"
            "[fragment]\n"
            "material_source_module_name = \"unlit_source\"\n"
            "[vertex]\n"
            "requirements = [\"Position\"]\n";
        MaterialDefinitionFileData unknown_key_data;
        if (ParseMaterialDefinitionFile(
                unknown_key_file, static_cast<int>(std::strlen(unknown_key_file)),
                unknown_key_data) != MaterialDefinitionFileParseResult::UnknownKey)
        {
            result.diagnostics.emplace_back("unknown root key must be rejected as UnknownKey");
        }
        else if (unknown_key_data.parse_error.key != "bogus_root_key")
        {
            result.diagnostics.emplace_back(std::string("unknown root key must report its name, got '")
                + unknown_key_data.parse_error.key + "'");
        }

        // ③ TOML 语法错误：必须报出行号（作者最常见的失效模式之一）
        const char syntax_error_file[] = "schema = 4\nid = \"Unterminated\n";
        MaterialDefinitionFileData syntax_error_data;
        if (ParseMaterialDefinitionFile(
                syntax_error_file, static_cast<int>(std::strlen(syntax_error_file)),
                syntax_error_data) != MaterialDefinitionFileParseResult::InvalidValue)
        {
            result.diagnostics.emplace_back("broken TOML must fail the parse");
        }
        else if (syntax_error_data.parse_error.line <= 0)
        {
            result.diagnostics.emplace_back("broken TOML must report the offending line number");
        }

        result.passed = result.diagnostics.empty();
        return result;
    }

    // 表面领域契约（MaterialDomain.h）的门禁。
    //
    // 这一层是「有限领域」取代「顶点×材质×Pass 笛卡尔积」的唯一强制点，
    // 所以门禁必须同时守住两个方向：
    //   ① 契约表自身不能腐化（指向不存在的模块 / 空语义 / 重复槽名）；
    //   ② 校验器不能放行违规材质，也不能误杀合法材质。
    static GateResult RunMaterialDomainContractCase()
    {
        GateResult result;
        result.name = "D.material-domain-contract";

        if (MakeSkyConfig().cull_mode != VK_CULL_MODE_NONE)
            result.diagnostics.emplace_back(
                "Sky spheres are viewed from inside and must not cull either face");

        if (MakeSkyCubeSphereConfig().cull_mode != VK_CULL_MODE_FRONT_BIT)
            result.diagnostics.emplace_back(
                "SkyCubeSphere must use its explicit front-face culling config");

        const std::string color_identity = ReadFileText(
            RepoRootPath("ShaderLibrary/vertex/s1_color_default.glsl"));
        if (color_identity.find("vec4 GetVertexColor() { return vec4(1.0); }")
                == std::string::npos)
            result.diagnostics.emplace_back(
                "default vertex-color provider must return the white multiplicative identity");

        const std::string surface_interface = ReadFileText(
            RepoRootPath("ShaderLibrary/surface/material_surface.glsl"));
        if (surface_interface.find(
                "surface.baseColor = material.baseColor * si.vertexColor.rgb;")
                == std::string::npos
         || surface_interface.find(
                "surface.alpha = material.alpha * si.vertexColor.a;")
                == std::string::npos)
            result.diagnostics.emplace_back(
                "common surface contract must apply vertex color exactly once to RGB and alpha");

        const std::string vertex_color_source = ReadFileText(
            RepoRootPath("ShaderLibrary/material/vertex_color_source.glsl"));
        if (vertex_color_source.find("materialResult.baseColor = vec3(1.0);")
                == std::string::npos)
            result.diagnostics.emplace_back(
                "VertexColor domain must rely on the common vertex-color multiplier, not multiply it twice");

        // ── 片段侧 varying 声明：单一真源审计 ──
        //
        // 迁移前 13 份领域 `.frag` 各自手抄 `layout(in …)`：网格写出什么与片段读
        // 什么只能靠人对齐，而 checker_3d / error_checker_3d 还给 WorldNormal 挂了
        // 一个与接口无关的 `HGL_MATERIAL_HAS_VERTEX_NORMAL` 开关。现在声明只有一
        // 处——`common/interstage_varying.glsl` 的片段段，开关是
        // `generated/common_options.glsl` 的 `HGL_VARYING_*`，与网格侧同一份。
        // 契约因此变成三条可机检的事实：
        //   ① 领域 `.frag` 里不再出现任何阶段间 varying 的手写声明；
        //   ② 除纯空体占位外，每份 `.frag` 都经 `HGL_FRAGMENT_INPUT_VARYINGS`
        //      走统一文件（否则「统一」只是别人的统一）；
        //   ③ 统一文件把 WorldNormal 声明挂在 `HGL_VARYING_WORLD_NORMAL` 之下，
        //      且两个「有则消费」的领域仍在消费 `fragWorldNormal`。
        const std::string interstage_source = ReadFileText(
            RepoRootPath("ShaderLibrary/common/interstage_varying.glsl"));

        // 「这一行是阶段间输入声明」：`layout(location=…)` 起头且带 `in` 记号。
        // 输出附件用 `out`，因此两者不会混。
        const auto declares_interstage_input =
            [](const std::string &line) -> bool
            {
                return line.find("layout(location=") != std::string::npos
                    && ContainsWord(line, "in")
                    && !ContainsWord(line, "out");
            };

        // 唯一豁免：StandardOpaque 的 depth pass 空体占位。它刻意没有 varying、
        // 没有 include、没有采样输出，加任何东西都会平白改变 depth 管线。
        const char *const varying_free_fragments[] =
        {
            "ShaderLibrary/domain/standard_opaque/shadow_caster.frag",
        };

        const std::vector<std::pair<std::string, std::string>> domain_tree =
            LoadTextTree(RepoRootPath("ShaderLibrary/domain"));
        std::size_t audited_fragments = 0;
        for (const auto &entry : domain_tree)
        {
            const std::string &identity = entry.first;
            if (identity.find(".frag|") == std::string::npos)
                continue;

            // `LoadTextTree` 的 key 是 `lexically_relative` 结果，Windows 上带 `\`。
            // 判据与诊断信息统一成 `/`，免得同一个文件出现两种写法。
            std::string relative = identity.substr(0, identity.find('|'));
            for (char &ch : relative)
                if (ch == '\\')
                    ch = '/';
            const std::string path = "ShaderLibrary/domain/" + relative;

            bool exempt = false;
            for (const char *allowed : varying_free_fragments)
                exempt = exempt || path == allowed;
            if (exempt)
                continue;

            ++audited_fragments;
            const std::string &source = entry.second;

            for (std::size_t line_begin = 0; line_begin < source.size();)
            {
                const std::size_t line_end = source.find('\n', line_begin);
                const std::string line = source.substr(
                    line_begin,
                    line_end == std::string::npos
                        ? std::string::npos
                        : line_end - line_begin);
                if (declares_interstage_input(line))
                    result.diagnostics.emplace_back(
                        path
                        + " declares an inter-stage varying inline; the only "
                          "definition point is common/interstage_varying.glsl");

                if (line_end == std::string::npos)
                    break;
                line_begin = line_end + 1;
            }

            const bool routes_through_unified_file =
                source.find("#define HGL_FRAGMENT_INPUT_VARYINGS")
                    != std::string::npos
             && source.find("#include \"common/interstage_varying.glsl\"")
                    != std::string::npos;
            if (!routes_through_unified_file)
                result.diagnostics.emplace_back(
                    path
                    + " must define HGL_FRAGMENT_INPUT_VARYINGS and include "
                      "common/interstage_varying.glsl instead of declaring "
                      "inter-stage varyings itself");
        }
        if (audited_fragments < 12)
            result.diagnostics.emplace_back(
                "domain fragment sweep found only "
                + std::to_string(audited_fragments)
                + " master(s); the domain library is expected to be far larger");

        const InterStageSemanticInfo *normal_info =
            GetInterStageSemanticInfo(InterStageSemantic::WorldNormal);
        if (normal_info == nullptr || normal_info->shader_symbol == nullptr)
        {
            result.diagnostics.emplace_back(
                "WorldNormal semantics are not registered");
        }
        else
        {
            const std::string conditional = ExtractConditionalBlock(
                interstage_source, "HGL_VARYING_WORLD_NORMAL");
            if (conditional.find(std::string("vec3 ") + normal_info->shader_symbol)
                    == std::string::npos)
                result.diagnostics.emplace_back(
                    "common/interstage_varying.glsl must declare fragWorldNormal "
                    "under HGL_VARYING_WORLD_NORMAL");

            // 「有则消费」的两个领域：接口给了就必须用上，否则顶点法线会静默退化
            // 成面法线兜底。
            const char *const normal_consumers[] =
            {
                "ShaderLibrary/domain/checker_3d/checker_3d.frag",
                "ShaderLibrary/domain/error_checker_3d/error_checker_3d.frag",
            };
            for (const char *consumer_path : normal_consumers)
            {
                const std::string consumer_source =
                    ReadFileText(RepoRootPath(consumer_path));
                if (consumer_source.find(
                        "si.worldNormal = normalize(fragWorldNormal);")
                        == std::string::npos)
                    result.diagnostics.emplace_back(
                        std::string(consumer_path)
                        + " must consume the optional WorldNormal varying when present");
            }
        }

        DomainShaderTemplate sky_minimal_template;
        if (!ResolveDomainShaderTemplate(
                MaterialDomain::Sky,
                ShaderProgramPurpose::ForwardColor,
                false,
                sky_minimal_template)
         || std::strcmp(
                sky_minimal_template.relative_path.c_str(),
                "domain/sky/sky_minimal.frag") != 0)
            result.diagnostics.emplace_back(
                "SkyMinimal must resolve the fixed fragment that includes its no-texture surface");

        DomainShaderTemplate sky_cube_template;
        if (!ResolveDomainShaderTemplate(
                MaterialDomain::Sky,
                ShaderProgramPurpose::ForwardColor,
                false,
                sky_cube_template,
                "sky_cube_surface")
         || std::strcmp(
                sky_cube_template.relative_path.c_str(),
                "domain/sky/sky.frag") != 0)
            result.diagnostics.emplace_back(
                "SkyCube must retain its cubemap fixed fragment");

        DomainShaderTemplate unsupported_sky_template;
        if (ResolveDomainShaderTemplate(
                MaterialDomain::Sky,
                ShaderProgramPurpose::ForwardColor,
                false,
                unsupported_sky_template,
                "unknown_sky_surface"))
            result.diagnostics.emplace_back(
                "Sky must reject a surface provider without a fixed fragment master");

        // ── ① 契约表自检 + 引用模块存在性 ────────────────────────────────────
        ShaderCodeModuleRegistry registry;
        int module_file_count = 0;
        int module_error_count = 0;
        const bool registry_ok = registry.LoadDirectory(
            hgl::ToOSString(GetShaderLibraryPath()),
            &module_file_count, &module_error_count);
        if (!registry_ok)
            result.diagnostics.emplace_back("domain contract: module registry scan failed");
        else if (module_error_count != 0)
            result.diagnostics.emplace_back(
                "domain contract: module registry scan reported parse errors");

        for (int i = 0; i < MaterialDomainContractCount; ++i)
        {
            const MaterialDomainContract *contract = GetMaterialDomainContractAt(i);
            if (!contract)
            {
                result.diagnostics.emplace_back(
                    "domain contract table has a null entry at index "
                    + std::to_string(i));
                continue;
            }

            const std::string tag = std::string("domain '")
                + (contract->name ? contract->name : "<null>") + "'";

            if (!contract->name || !*contract->name)
                result.diagnostics.emplace_back(
                    "domain contract at index " + std::to_string(i) + " has no name");

            // 名字必须能被 ParseMaterialDomain 反解回同一个枚举值，
            // 否则 TOML 写什么名字都对不上。
            MaterialDomain roundtrip = MaterialDomain::Custom;
            if (contract->name
                && (!ParseMaterialDomain(contract->name, roundtrip)
                    || roundtrip != contract->domain))
            {
                result.diagnostics.emplace_back(
                    tag + " name does not round-trip through ParseMaterialDomain");
            }

            if (contract->required_vertex_semantic_mask == 0)
                result.diagnostics.emplace_back(tag + " requires no vertex semantic at all");

            // 无纹理领域（纯色/顶点色/亮度/棋盘格等）是**合法**的：
            // 它们结构上就不该有贴图。这里只拦「指针与计数不一致」的笔误，
            // 不要求每个领域都必须有槽。
            if ((contract->texture_slots == nullptr) != (contract->texture_slot_count == 0))
            {
                result.diagnostics.emplace_back(
                    tag + " texture slot table is inconsistent (pointer/count mismatch)");
                continue;
            }

            // no_material_source 与 reference_source 不能同时成立：
            // 前者说「本领域没有来源模块」，后者却给出了一个模块名。
            if (contract->no_material_source
                && contract->reference_material_source_module)
            {
                result.diagnostics.emplace_back(
                    tag + " sets no_material_source but still names a source module");
            }

            if (contract->texture_slot_count <= 0)
                continue;

            for (int s = 0; s < contract->texture_slot_count; ++s)
            {
                const MaterialDomainTextureSlot &slot = contract->texture_slots[s];
                if (!slot.name || !*slot.name)
                {
                    result.diagnostics.emplace_back(
                        tag + " has an unnamed texture slot at index "
                        + std::to_string(s));
                    continue;
                }
                for (int t = s + 1; t < contract->texture_slot_count; ++t)
                {
                    const MaterialDomainTextureSlot &other = contract->texture_slots[t];
                    if (other.name && std::strcmp(slot.name, other.name) == 0)
                        result.diagnostics.emplace_back(
                            tag + " declares duplicate texture slot '" + slot.name + "'");
                }
            }

            // 引用模块必须真实存在于注册表：契约指向空气时，
            // Phase 2 的宏头生成会产出无法编译的结果。
            if (registry_ok)
            {
                if (contract->reference_material_source_module
                    && !registry.FindByName(contract->reference_material_source_module))
                {
                    result.diagnostics.emplace_back(
                        tag + " references unknown material source module '"
                        + contract->reference_material_source_module + "'");
                }
                if (contract->reference_ntb_module
                    && !registry.FindByName(contract->reference_ntb_module))
                {
                    result.diagnostics.emplace_back(
                        tag + " references unknown ntb module '"
                        + contract->reference_ntb_module + "'");
                }
            }
        }

        // ── ② 合法材质必须通过 ────────────────────────────────────────────────
        const char good_file[] =
            "schema = 4\n"
            "id = \"DomainGoodStandardOpaque\"\n"
            "name = \"DomainGoodStandardOpaque\"\n"
            "provider_policy = \"GeometryOnly\"\n"
            "domain = \"StandardOpaque\"\n"
            "[fragment]\n"
            "material_source_module_name = \"pbr_surface_source\"\n"
            "ntb_module_name = \"ntb_tangent_vbo_normalmap\"\n"
            "[vertex]\n"
            "requirements = [\"Position\", \"UV0\", \"Normal\"]\n"
            "[resources]\n"
            "textures = [\n"
            "  { name = \"base_color\", sampler = \"Sampler2DArray\", required = false },\n"
            "  { name = \"roughness\", sampler = \"Sampler2DArray\", required = false },\n"
            "  { name = \"metallic\", sampler = \"Sampler2DArray\", required = false },\n"
            "  { name = \"occlusion\", sampler = \"Sampler2DArray\", required = false },\n"
            "  { name = \"opacity_mask\", sampler = \"Sampler2DArray\", required = false },\n"
            "  { name = \"normal\", sampler = \"Sampler2DArray\", required = false }\n"
            "]\n";
        MaterialDefinitionFileData good_data;
        const MaterialDefinitionFileParseResult good_result =
            ParseMaterialDefinitionFile(
                good_file, static_cast<int>(std::strlen(good_file)), good_data);
        if (good_result != MaterialDefinitionFileParseResult::OK)
        {
            result.diagnostics.emplace_back(
                std::string("a conforming StandardOpaque material must parse, got ")
                + GetMaterialDefinitionFileParseResultName(good_result)
                + " key='" + good_data.parse_error.key
                + "' detail='" + good_data.parse_error.detail + "'");
        }
        else if (good_data.definition.domain != MaterialDomain::StandardOpaque)
        {
            result.diagnostics.emplace_back(
                "parsed material lost its domain (expected StandardOpaque)");
        }

        // ── ③ 未声明 domain = Custom = 不校验（向后兼容）─────────────────────
        // 这个材质缺 base_color 槽、缺 Normal 语义，对 StandardOpaque 是违规，
        // 但 Custom 不做契约校验 ⇒ 必须通过。任何「无条件套契约」的改动都会
        // 在这里炸出来。
        const char custom_file[] =
            "schema = 4\n"
            "id = \"DomainCustomLoose\"\n"
            "name = \"DomainCustomLoose\"\n"
            "provider_policy = \"GeometryOnly\"\n"
            "[fragment]\n"
            "material_source_module_name = \"unlit_source\"\n"
            "[vertex]\n"
            "requirements = [\"Position\"]\n";
        MaterialDefinitionFileData custom_data;
        if (ParseMaterialDefinitionFile(
                custom_file, static_cast<int>(std::strlen(custom_file)), custom_data)
            != MaterialDefinitionFileParseResult::OK)
        {
            result.diagnostics.emplace_back(
                "a material without 'domain' must stay exempt from contract checks");
        }
        else if (custom_data.definition.domain != MaterialDomain::Custom)
        {
            result.diagnostics.emplace_back(
                "missing 'domain' must default to MaterialDomain::Custom");
        }

        // ── ④ 负向：逐条契约违规都必须被拒 ──────────────────────────────────
        struct DomainViolationProbe
        {
            const char *label;
            const char *content;
        };

        const DomainViolationProbe probes[] = {
            {
                "missing required vertex semantic (Normal)",
                "schema = 4\n"
                "id = \"DomainBadVertex\"\n"
                "name = \"DomainBadVertex\"\n"
                "provider_policy = \"GeometryOnly\"\n"
                "domain = \"StandardOpaque\"\n"
                "[fragment]\n"
                "material_source_module_name = \"pbr_surface_source\"\n"
                "[vertex]\n"
                "requirements = [\"Position\", \"UV0\"]\n"
                "[resources]\n"
                "textures = [\n"
                "  { name = \"base_color\", sampler = \"Sampler2DArray\", required = false },\n"
                "  { name = \"roughness\", sampler = \"Sampler2DArray\", required = false },\n"
                "  { name = \"metallic\", sampler = \"Sampler2DArray\", required = false },\n"
                "  { name = \"occlusion\", sampler = \"Sampler2DArray\", required = false },\n"
                "  { name = \"opacity_mask\", sampler = \"Sampler2DArray\", required = false },\n"
                "  { name = \"normal\", sampler = \"Sampler2DArray\", required = false }\n"
                "]\n"
            },
            {
                "texture slot outside the domain whitelist",
                "schema = 4\n"
                "id = \"DomainBadSlot\"\n"
                "name = \"DomainBadSlot\"\n"
                "provider_policy = \"GeometryOnly\"\n"
                "domain = \"StandardOpaque\"\n"
                "[fragment]\n"
                "material_source_module_name = \"pbr_surface_source\"\n"
                "[vertex]\n"
                "requirements = [\"Position\", \"UV0\", \"Normal\"]\n"
                "[resources]\n"
                "textures = [\n"
                "  { name = \"base_color\", sampler = \"Sampler2DArray\", required = false },\n"
                "  { name = \"roughness\", sampler = \"Sampler2DArray\", required = false },\n"
                "  { name = \"metallic\", sampler = \"Sampler2DArray\", required = false },\n"
                "  { name = \"occlusion\", sampler = \"Sampler2DArray\", required = false },\n"
                "  { name = \"opacity_mask\", sampler = \"Sampler2DArray\", required = false },\n"
                "  { name = \"normal\", sampler = \"Sampler2DArray\", required = false },\n"
                "  { name = \"splat_map\", sampler = \"Sampler2DArray\", required = false }\n"
                "]\n"
            },
            {
                "missing a must-declare texture slot",
                "schema = 4\n"
                "id = \"DomainBadMissingSlot\"\n"
                "name = \"DomainBadMissingSlot\"\n"
                "provider_policy = \"GeometryOnly\"\n"
                "domain = \"StandardOpaque\"\n"
                "[fragment]\n"
                "material_source_module_name = \"pbr_surface_source\"\n"
                "[vertex]\n"
                "requirements = [\"Position\", \"UV0\", \"Normal\"]\n"
                "[resources]\n"
                "textures = [\n"
                "  { name = \"base_color\", sampler = \"Sampler2DArray\", required = false }\n"
                "]\n"
            },
            {
                // 阶段四改成违规：领域已给出规范模块，资产**不得**自选别的来源模块。
                "self-chosen material source module",
                "schema = 4\n"
                "id = \"DomainBadWrongSource\"\n"
                "name = \"DomainBadWrongSource\"\n"
                "provider_policy = \"GeometryOnly\"\n"
                "domain = \"StandardOpaque\"\n"
                "[fragment]\n"
                "material_source_module_name = \"checkerboard_3d_source\"\n"
                "ntb_module_name = \"ntb_tangent_vbo_normalmap\"\n"
                "[vertex]\n"
                "requirements = [\"Position\", \"UV0\", \"Normal\"]\n"
                "[resources]\n"
                "textures = [\n"
                "  { name = \"base_color\", sampler = \"Sampler2DArray\", required = false },\n"
                "  { name = \"roughness\", sampler = \"Sampler2DArray\", required = false },\n"
                "  { name = \"metallic\", sampler = \"Sampler2DArray\", required = false },\n"
                "  { name = \"occlusion\", sampler = \"Sampler2DArray\", required = false },\n"
                "  { name = \"opacity_mask\", sampler = \"Sampler2DArray\", required = false },\n"
                "  { name = \"normal\", sampler = \"Sampler2DArray\", required = false }\n"
                "]\n"
            },
            {
                "self-chosen ntb module",
                "schema = 4\n"
                "id = \"DomainBadWrongNtb\"\n"
                "name = \"DomainBadWrongNtb\"\n"
                "provider_policy = \"GeometryOnly\"\n"
                "domain = \"StandardOpaque\"\n"
                "[fragment]\n"
                "material_source_module_name = \"pbr_surface_source\"\n"
                "ntb_module_name = \"ntb_orthonormal\"\n"
                "[vertex]\n"
                "requirements = [\"Position\", \"UV0\", \"Normal\"]\n"
                "[resources]\n"
                "textures = [\n"
                "  { name = \"base_color\", sampler = \"Sampler2DArray\", required = false },\n"
                "  { name = \"roughness\", sampler = \"Sampler2DArray\", required = false },\n"
                "  { name = \"metallic\", sampler = \"Sampler2DArray\", required = false },\n"
                "  { name = \"occlusion\", sampler = \"Sampler2DArray\", required = false },\n"
                "  { name = \"opacity_mask\", sampler = \"Sampler2DArray\", required = false },\n"
                "  { name = \"normal\", sampler = \"Sampler2DArray\", required = false }\n"
                "]\n"
            },
            {
                "unknown domain name",
                "schema = 4\n"
                "id = \"DomainBadUnknown\"\n"
                "name = \"DomainBadUnknown\"\n"
                "provider_policy = \"GeometryOnly\"\n"
                "domain = \"NoSuchDomain\"\n"
                "[fragment]\n"
                "material_source_module_name = \"pbr_surface_source\"\n"
                "[vertex]\n"
                "requirements = [\"Position\", \"UV0\", \"Normal\"]\n"
            },
            {
                // 无槽领域（UnlitSolid 等）必须拒绝一切纹理声明。
                "texture on a domain that declares no slot at all",
                "schema = 4\n"
                "id = \"DomainBadSlotonSless\"\n"
                "name = \"DomainBadSlotonSless\"\n"
                "provider_policy = \"GeometryOnly\"\n"
                "domain = \"UnlitSolid\"\n"
                "[fragment]\n"
                "[vertex]\n"
                "requirements = [\"Position\"]\n"
                "[resources]\n"
                "textures = [\n"
                "  { name = \"base_color\", sampler = \"Sampler2DArray\", required = false }\n"
                "]\n"
            },
            {
                // 无光表面族的顶点语义是封闭的：VertexColor 少了 Color 就
                // 不该被当成顶点色材质放行。
                "missing Color on a vertex-color domain",
                "schema = 4\n"
                "id = \"DomainBadVertexColor\"\n"
                "name = \"DomainBadVertexColor\"\n"
                "provider_policy = \"GeometryOnly\"\n"
                "domain = \"VertexColor\"\n"
                "[fragment]\n"
                "[vertex]\n"
                "requirements = [\"Position\"]\n"
            },
            {
                // VertexPalette 比 VertexColor 多要求 TransformID/Size：
                // 这份契约存在的意义就是拦住「以为自己是调色板其实缺属性」。
                "palette domain missing TransformID",
                "schema = 4\n"
                "id = \"DomainBadPalette\"\n"
                "name = \"DomainBadPalette\"\n"
                "provider_policy = \"GeometryOnly\"\n"
                "domain = \"VertexPalette\"\n"
                "[fragment]\n"
                "[vertex]\n"
                "requirements = [\"Position\", \"Color\", \"Size\"]\n"
            },
            {
                // no_material_source 领域的反向约束：自己不写来源模块是正确写法，
                // 但写了任何模块名都是错配（这条与 ③ 的正向兜底成对）。
                "source module declared on a domain that has none",
                "schema = 4\n"
                "id = \"DomainBadSkySource\"\n"
                "name = \"DomainBadSkySource\"\n"
                "provider_policy = \"GeometryOnly\"\n"
                "domain = \"Sky\"\n"
                "[fragment]\n"
                "material_source_module_name = \"unlit_source\"\n"
                "[vertex]\n"
                "requirements = [\"Position\"]\n"
            },
            {
                // `[transform]` 由领域决定：StandardOpaque 是 3D 通路，
                // 写 2D NDC 变换会让同一领域的材质走不同顶点管线，
                // 合批/layout 共享立刻失效——必须拒绝而不是静默接受。
                "self-chosen vertex transform on a 3D domain",
                "schema = 4\n"
                "id = \"DomainBadTransform\"\n"
                "name = \"DomainBadTransform\"\n"
                "provider_policy = \"GeometryOnly\"\n"
                "domain = \"StandardOpaque\"\n"
                "[transform]\n"
                "source = \"Vec2Position\"\n"
                "mapping = \"NDCLift\"\n"
                "orientation = \"World\"\n"
                "scale = \"World\"\n"
                "projection = \"LocalToWorldOnly\"\n"
                "[fragment]\n"
                "material_source_module_name = \"pbr_surface_source\"\n"
                "ntb_module_name = \"ntb_tangent_vbo_normalmap\"\n"
                "[vertex]\n"
                "requirements = [\"Position\", \"UV0\", \"Normal\"]\n"
                "[resources]\n"
                "textures = [\n"
                "  { name = \"base_color\", sampler = \"Sampler2DArray\", required = false },\n"
                "  { name = \"roughness\", sampler = \"Sampler2DArray\", required = false },\n"
                "  { name = \"metallic\", sampler = \"Sampler2DArray\", required = false },\n"
                "  { name = \"occlusion\", sampler = \"Sampler2DArray\", required = false },\n"
                "  { name = \"opacity_mask\", sampler = \"Sampler2DArray\", required = false },\n"
                "  { name = \"normal\", sampler = \"Sampler2DArray\", required = false }\n"
                "]\n"
            },
            {
                // 阶段三之二：槽形态（采样器种类）由领域决定。
                // 同一领域里换成 Sampler2D 会让引用行布局与采样方式都分叉，
                // 合批共享立刻失效——必须拒绝而不是静默接受。
                "self-chosen sampler on a domain texture slot",
                "schema = 4\n"
                "id = \"DomainBadSlotSampler\"\n"
                "name = \"DomainBadSlotSampler\"\n"
                "provider_policy = \"GeometryOnly\"\n"
                "domain = \"StandardOpaque\"\n"
                "[fragment]\n"
                "material_source_module_name = \"pbr_surface_source\"\n"
                "ntb_module_name = \"ntb_tangent_vbo_normalmap\"\n"
                "[vertex]\n"
                "requirements = [\"Position\", \"UV0\", \"Normal\"]\n"
                "[resources]\n"
                "textures = [\n"
                "  { name = \"base_color\", sampler = \"Sampler2D\", required = false },\n"
                "  { name = \"roughness\", sampler = \"Sampler2DArray\", required = false },\n"
                "  { name = \"metallic\", sampler = \"Sampler2DArray\", required = false },\n"
                "  { name = \"occlusion\", sampler = \"Sampler2DArray\", required = false },\n"
                "  { name = \"opacity_mask\", sampler = \"Sampler2DArray\", required = false },\n"
                "  { name = \"normal\", sampler = \"Sampler2DArray\", required = false }\n"
                "]\n"
            },
            {
                // 绑定必要性同理：领域说 optional，资产不能自行改成必绑
                // （否则同一领域里「缺图就报错」与「缺图用中性值」两种运行语义并存）。
                "self-chosen binding policy on a domain texture slot",
                "schema = 4\n"
                "id = \"DomainBadSlotRequired\"\n"
                "name = \"DomainBadSlotRequired\"\n"
                "provider_policy = \"GeometryOnly\"\n"
                "domain = \"StandardOpaque\"\n"
                "[fragment]\n"
                "material_source_module_name = \"pbr_surface_source\"\n"
                "ntb_module_name = \"ntb_tangent_vbo_normalmap\"\n"
                "[vertex]\n"
                "requirements = [\"Position\", \"UV0\", \"Normal\"]\n"
                "[resources]\n"
                "textures = [\n"
                "  { name = \"base_color\", sampler = \"Sampler2DArray\", required = true },\n"
                "  { name = \"roughness\", sampler = \"Sampler2DArray\", required = false },\n"
                "  { name = \"metallic\", sampler = \"Sampler2DArray\", required = false },\n"
                "  { name = \"occlusion\", sampler = \"Sampler2DArray\", required = false },\n"
                "  { name = \"opacity_mask\", sampler = \"Sampler2DArray\", required = false },\n"
                "  { name = \"normal\", sampler = \"Sampler2DArray\", required = false }\n"
                "]\n"
            },
            {
                // 通道数直接决定 shader 是否注入 MTL_TEX_<NAME>_CHANNELS 与 Z 还原逻辑。
                // normal 契约固定 2 通道，资产改成 4 就是另一种解码方式。
                "self-chosen channel count on a domain texture slot",
                "schema = 4\n"
                "id = \"DomainBadSlotChannels\"\n"
                "name = \"DomainBadSlotChannels\"\n"
                "provider_policy = \"GeometryOnly\"\n"
                "domain = \"StandardOpaque\"\n"
                "[fragment]\n"
                "material_source_module_name = \"pbr_surface_source\"\n"
                "ntb_module_name = \"ntb_tangent_vbo_normalmap\"\n"
                "[vertex]\n"
                "requirements = [\"Position\", \"UV0\", \"Normal\"]\n"
                "[resources]\n"
                "textures = [\n"
                "  { name = \"base_color\", sampler = \"Sampler2DArray\", required = false },\n"
                "  { name = \"roughness\", sampler = \"Sampler2DArray\", required = false },\n"
                "  { name = \"metallic\", sampler = \"Sampler2DArray\", required = false },\n"
                "  { name = \"occlusion\", sampler = \"Sampler2DArray\", required = false },\n"
                "  { name = \"opacity_mask\", sampler = \"Sampler2DArray\", required = false },\n"
                "  { name = \"normal\", sampler = \"Sampler2DArray\", required = false, channels = 4 }\n"
                "]\n"
            },
            {
                // material_data.type 决定 shader 按哪个结构读 payload 行。
                // StandardOpaque 规范为 PBRSurface，换成 EmissiveSurface 会让
                // 片元侧按错误的结构解码同一行字节——运行期不报错，只是数值全错。
                "self-chosen material_data type",
                "schema = 4\n"
                "id = \"DomainBadMaterialData\"\n"
                "name = \"DomainBadMaterialData\"\n"
                "provider_policy = \"GeometryOnly\"\n"
                "domain = \"StandardOpaque\"\n"
                "[fragment]\n"
                "material_source_module_name = \"pbr_surface_source\"\n"
                "ntb_module_name = \"ntb_tangent_vbo_normalmap\"\n"
                "[vertex]\n"
                "requirements = [\"Position\", \"UV0\", \"Normal\"]\n"
                "[resources]\n"
                "material_data = { type = \"EmissiveSurface\" }\n"
                "textures = [\n"
                "  { name = \"base_color\", sampler = \"Sampler2DArray\", required = false },\n"
                "  { name = \"roughness\", sampler = \"Sampler2DArray\", required = false },\n"
                "  { name = \"metallic\", sampler = \"Sampler2DArray\", required = false },\n"
                "  { name = \"occlusion\", sampler = \"Sampler2DArray\", required = false },\n"
                "  { name = \"opacity_mask\", sampler = \"Sampler2DArray\", required = false },\n"
                "  { name = \"normal\", sampler = \"Sampler2DArray\", required = false }\n"
                "]\n"
            },
        };

        for (const DomainViolationProbe &probe : probes)
        {
            MaterialDefinitionFileData data;
            const MaterialDefinitionFileParseResult parse_result =
                ParseMaterialDefinitionFile(
                    probe.content, static_cast<int>(std::strlen(probe.content)), data);
            if (parse_result == MaterialDefinitionFileParseResult::OK)
            {
                result.diagnostics.emplace_back(
                    std::string("domain contract let through a violation: ") + probe.label);
                continue;
            }

            // 报错必须落在 domain 这个键上，而不是飘去别的键——
            // 否则作者按提示改半天也修不好。
            if (data.parse_error.key != "domain")
            {
                result.diagnostics.emplace_back(
                    std::string("violation '") + probe.label
                    + "' must be reported on key 'domain', got '"
                    + data.parse_error.key + "'");
            }
            if (data.parse_error.detail.empty())
            {
                result.diagnostics.emplace_back(
                    std::string("violation '") + probe.label
                    + "' must carry a human-readable detail");
            }
        }

        // ── ③ 领域规范模块兜底（阶段四）───────────────────────────────────────
        // 资产只写 `domain` + 数值 + 贴图，两个模块名都省略 ⇒ 必须由领域契约填上，
        // 且填进来的必须正是契约里的规范名。这条与 ② 的 mismatch 探针成对：
        // 一个管「省略要能补上」，一个管「写了不能乱写」。
        {
            const char *minimal_domain_material =
                "schema = 4\n"
                "id = \"DomainMinimal\"\n"
                "name = \"DomainMinimal\"\n"
                "provider_policy = \"GeometryOnly\"\n"
                "domain = \"StandardOpaque\"\n"
                "[fragment]\n"
                "[vertex]\n"
                "requirements = [\"Position\", \"UV0\", \"Normal\"]\n"
                "[resources]\n"
                "textures = [\n"
                "  { name = \"base_color\", sampler = \"Sampler2DArray\", required = false },\n"
                "  { name = \"roughness\", sampler = \"Sampler2DArray\", required = false },\n"
                "  { name = \"metallic\", sampler = \"Sampler2DArray\", required = false },\n"
                "  { name = \"occlusion\", sampler = \"Sampler2DArray\", required = false },\n"
                "  { name = \"opacity_mask\", sampler = \"Sampler2DArray\", required = false },\n"
                "  { name = \"normal\", sampler = \"Sampler2DArray\", required = false }\n"
                "]\n";

            MaterialDefinitionFileData data;
            const MaterialDefinitionFileParseResult parse_result = ParseMaterialDefinitionFile(
                minimal_domain_material,
                static_cast<int>(std::strlen(minimal_domain_material)), data);
            if (parse_result != MaterialDefinitionFileParseResult::OK)
            {
                result.diagnostics.emplace_back(
                    "minimal domain material (no module names) must parse, got: "
                    + std::string(data.parse_error.key) + " / "
                    + data.parse_error.detail.c_str());
            }
            else
            {
                const char *expect_source = GetMaterialDomainReferenceMaterialSource(
                    MaterialDomain::StandardOpaque);
                const char *expect_ntb = GetMaterialDomainReferenceNtb(
                    MaterialDomain::StandardOpaque);

                if (!expect_source || !expect_ntb)
                {
                    result.diagnostics.emplace_back(
                        "StandardOpaque contract must declare both reference modules");
                }
                else
                {
                    if (std::string(data.definition.material_source_module_name
                                    ? data.definition.material_source_module_name : "")
                        != expect_source)
                        result.diagnostics.emplace_back(
                            std::string("derived material source module must be '")
                            + expect_source + "', got '"
                            + (data.definition.material_source_module_name
                                   ? data.definition.material_source_module_name : "<null>")
                            + "'");
                    if (std::string(data.definition.ntb_module_name
                                    ? data.definition.ntb_module_name : "")
                        != expect_ntb)
                        result.diagnostics.emplace_back(
                            std::string("derived ntb module must be '") + expect_ntb
                            + "', got '"
                            + (data.definition.ntb_module_name
                                   ? data.definition.ntb_module_name : "<null>")
                            + "'");

                    // `[transform]` 同理：缺省必须由领域契约填成规范变换。
                    if (!IsDefault3DNodeConfig(data.definition.vertex_node_config))
                        result.diagnostics.emplace_back(
                            "omitted [transform] on StandardOpaque must derive the domain "
                            "canonical 3D transform");
                }
            }
        }

        // ── ③b 非 3D 领域的顶点变换同样由领域契约给出 ──────────────────────────
        // 这条单独钉住「契约里的 canonical_vertex_node_config 不是摆设」：
        // 只测 StandardOpaque（= 默认值）的话，漏填契约也照样通过。
        //
        // 阶段三之二起这条同时钉住「最小形态」：纹理只写 name，
        // 不写 [transform]、不写 material_data —— 槽形态与数据类型全部由领域给出。
        {
            const char *text_material =
                "schema = 4\n"
                "id = \"DomainMinimalText\"\n"
                "name = \"DomainMinimalText\"\n"
                "provider_policy = \"GeometryOnly\"\n"
                "domain = \"Text2D\"\n"
                "[fragment]\n"
                "[vertex]\n"
                "requirements = [\"Position\", \"UV0\"]\n"
                "[resources]\n"
                "textures = [{ name = \"base_color\" }]\n";

            MaterialDefinitionFileData data;
            const MaterialDefinitionFileParseResult parse_result = ParseMaterialDefinitionFile(
                text_material, static_cast<int>(std::strlen(text_material)), data);
            if (parse_result != MaterialDefinitionFileParseResult::OK)
            {
                result.diagnostics.emplace_back(
                    "minimal Text2D material (only the texture name) must parse, got: "
                    + std::string(data.parse_error.key) + " / "
                    + data.parse_error.detail.c_str());
            }
            else
            {
                VertexShaderNodeConfig expect{};
                const MaterialDomainTextureSlot *slot =
                    FindMaterialDomainTextureSlot(MaterialDomain::Text2D, "base_color");

                if (!GetMaterialDomainCanonicalVertexNodeConfig(
                        MaterialDomain::Text2D, expect))
                {
                    result.diagnostics.emplace_back(
                        "Text2D contract must expose a canonical vertex transform");
                }
                else if (!IsSameVertexNodeConfig(
                             data.definition.vertex_node_config, expect))
                {
                    result.diagnostics.emplace_back(
                        "omitted [transform] on Text2D must derive the domain canonical "
                        "transform (expected PixelToLocal / OrthoViewport)");
                }
                else if (data.definition.vertex_node_config.projection
                         != ProjectionMode::OrthoViewport)
                {
                    result.diagnostics.emplace_back(
                        "Text2D canonical transform must project through the viewport ortho");
                }
                else if (!slot)
                {
                    result.diagnostics.emplace_back(
                        "Text2D contract must expose the base_color texture slot");
                }
                else if (data.definition.texture_declarations.size() != 1)
                {
                    result.diagnostics.emplace_back(
                        "minimal Text2D material must end up with exactly one texture slot");
                }
                else
                {
                    const MaterialTextureDeclaration &declared =
                        data.definition.texture_declarations.front();
                    if (declared.sampler_type != slot->sampler)
                    {
                        result.diagnostics.emplace_back(
                            "omitted sampler must derive the domain slot sampler");
                    }
                    else if (declared.required != slot->binding_required)
                    {
                        result.diagnostics.emplace_back(
                            "omitted required must derive the domain slot binding policy");
                    }
                    else if (declared.channels != slot->channels)
                    {
                        result.diagnostics.emplace_back(
                            "omitted channels must derive the domain slot channel count");
                    }
                }

                GlobalSSBOType expect_type = GlobalSSBOType::MeshDrawParams;
                if (!GetMaterialDomainCanonicalMaterialDataType(
                        MaterialDomain::Text2D, expect_type))
                {
                    result.diagnostics.emplace_back(
                        "Text2D contract must expose a canonical material_data type");
                }
                else if (data.definition.material_private_data != expect_type)
                {
                    result.diagnostics.emplace_back(
                        "omitted [resources].material_data must derive the domain type");
                }
            }
        }

        // ── ⑤ 无光表面族：合法材质必须通过（防误杀）───────────────────────────
        // ④ 全是「必须被拒」的探针。新加的领域若契约写得太严
        // （比如给无槽领域凭空要求一个纹理槽，或多要一个顶点语义），
        // 只会表现为「仓库材质全部加载失败」——那是最难定位的一种回归。
        // 所以这里正面钉住每个新领域的最小合法形态。
        struct DomainAcceptProbe
        {
            const char *label;
            const char *content;
            MaterialDomain expect_domain;
            bool expect_source_derived;   // true = 解析后应从契约兜底得到来源模块
            const char *expect_source;
        };

        const DomainAcceptProbe accept_probes[] = {
            {
                "UnlitSolid (no texture at all)",
                "schema = 4\n"
                "id = \"DomainOkUnlitSolid\"\n"
                "name = \"DomainOkUnlitSolid\"\n"
                "provider_policy = \"GeometryOnly\"\n"
                "domain = \"UnlitSolid\"\n"
                "[fragment]\n"
                "[vertex]\n"
                "requirements = [\"Position\"]\n",
                MaterialDomain::UnlitSolid, true, "unlit_source"
            },
            {
                "VertexColor",
                "schema = 4\n"
                "id = \"DomainOkVertexColor\"\n"
                "name = \"DomainOkVertexColor\"\n"
                "provider_policy = \"GeometryOnly\"\n"
                "domain = \"VertexColor\"\n"
                "[fragment]\n"
                "[vertex]\n"
                "requirements = [\"Position\", \"Color\"]\n",
                MaterialDomain::VertexColor, true, "vertex_color_source"
            },
            {
                "VertexPalette (four semantics)",
                "schema = 4\n"
                "id = \"DomainOkVertexPalette\"\n"
                "name = \"DomainOkVertexPalette\"\n"
                "provider_policy = \"GeometryOnly\"\n"
                "domain = \"VertexPalette\"\n"
                "[fragment]\n"
                "[vertex]\n"
                "requirements = [\"Position\", \"Color\", \"TransformID\", \"Size\"]\n",
                MaterialDomain::VertexPalette, true, "vertex_color_source"
            },
            {
                "VertexLuminance",
                "schema = 4\n"
                "id = \"DomainOkVertexLum\"\n"
                "name = \"DomainOkVertexLum\"\n"
                "provider_policy = \"GeometryOnly\"\n"
                "domain = \"VertexLuminance\"\n"
                "[fragment]\n"
                "[vertex]\n"
                "requirements = [\"Position\", \"Luminance\"]\n",
                MaterialDomain::VertexLuminance, true, "luminance_source"
            },
            {
                "Text2D",
                "schema = 4\n"
                "id = \"DomainOkText2D\"\n"
                "name = \"DomainOkText2D\"\n"
                "provider_policy = \"GeometryOnly\"\n"
                "domain = \"Text2D\"\n"
                "[fragment]\n"
                "[vertex]\n"
                "requirements = [\"Position\", \"UV0\"]\n"
                "[resources]\n"
                "textures = [\n"
                "  { name = \"base_color\", sampler = \"Sampler2DArray\", required = true }\n"
                "]\n",
                MaterialDomain::Text2D, true, "text_source_gpu"
            },
            {
                // Sky 是 no_material_source：来源模块必须**保持为空**，
                // 不能被兜底逻辑塞进一个不存在的规范名。
                "Sky (no material source by design)",
                "schema = 4\n"
                "id = \"DomainOkSky\"\n"
                "name = \"DomainOkSky\"\n"
                "provider_policy = \"GeometryOnly\"\n"
                "domain = \"Sky\"\n"
                "[fragment]\n"
                "[vertex]\n"
                "requirements = [\"Position\"]\n",
                MaterialDomain::Sky, false, nullptr
            },
            {
                "Checker2D (no TexCoord required)",
                "schema = 4\n"
                "id = \"DomainOkChecker2D\"\n"
                "name = \"DomainOkChecker2D\"\n"
                "provider_policy = \"GeometryOnly\"\n"
                "domain = \"Checker2D\"\n"
                "[fragment]\n"
                "[vertex]\n"
                "requirements = [\"Position\"]\n",
                MaterialDomain::Checker2D, true, "checkerboard_2d_source"
            },
            {
                "DebugNormal",
                "schema = 4\n"
                "id = \"DomainOkDebugNormal\"\n"
                "name = \"DomainOkDebugNormal\"\n"
                "provider_policy = \"GeometryOnly\"\n"
                "domain = \"DebugNormal\"\n"
                "[fragment]\n"
                "[vertex]\n"
                "requirements = [\"Position\", \"Normal\"]\n",
                MaterialDomain::DebugNormal, true, "debug_normal_source"
            },
        };

        for (const DomainAcceptProbe &probe : accept_probes)
        {
            MaterialDefinitionFileData data;
            const MaterialDefinitionFileParseResult parse_result =
                ParseMaterialDefinitionFile(
                    probe.content, static_cast<int>(std::strlen(probe.content)), data);
            if (parse_result != MaterialDefinitionFileParseResult::OK)
            {
                result.diagnostics.emplace_back(
                    std::string("domain contract rejected a legal material: ") + probe.label
                    + " ('" + data.parse_error.key + "': " + data.parse_error.detail.c_str()
                    + ")");
                continue;
            }

            if (data.definition.domain != probe.expect_domain)
            {
                result.diagnostics.emplace_back(
                    std::string("legal material lost its domain: ") + probe.label);
            }

            const char *derived = data.definition.material_source_module_name;
            if (probe.expect_source_derived)
            {
                if (!derived || std::strcmp(derived, probe.expect_source) != 0)
                    result.diagnostics.emplace_back(
                        std::string("legal material must derive source module '")
                        + probe.expect_source + "' for " + probe.label + ", got '"
                        + (derived ? derived : "<null>") + "'");
            }
            else if (derived && *derived)
            {
                // no_material_source 领域：兜底逻辑不得凭空注入来源模块。
                result.diagnostics.emplace_back(
                    std::string("no-source domain must not derive a source module, ")
                    + probe.label + " got '" + derived + "'");
            }
        }

        result.passed = result.diagnostics.empty();
        return result;
    }

    static GateResult RunFallbackInferenceCase()
    {
        GateResult result;
        result.name = "F.fallback-dimension-neutral";

        GeometryVertexFormat gvf_2d{};
        gvf_2d.Add(VertexSemantic::Position, VK_FORMAT_R32G32_SFLOAT, 2, sizeof(float) * 2);

        GeometryVertexFormat gvf_3d{};
        gvf_3d.Add(VertexSemantic::Position, VK_FORMAT_R32G32B32_SFLOAT, 3, sizeof(float) * 3);

        if (gvf_2d.GetVertexInputHash() == gvf_3d.GetVertexInputHash())
            result.diagnostics.emplace_back(
                "Geometry formats with different dimensions must remain distinct");
        if (std::strcmp(GetFallbackMaterialDefinitionID(), "builtin/pure_color") != 0)
            result.diagnostics.emplace_back("all Geometry dimensions must use unified PureColor fallback");

        result.passed = result.diagnostics.empty();
        return result;
    }

    static GateResult RunProviderResourceManifestCase()
    {
        GateResult result;
        result.name = "H1.provider-resource-manifest";

        ShaderCodeModuleRegistry registry;

        int file_count = 0;
        int error_count = 0;
        if (!registry.LoadDirectory(
                hgl::ToOSString(GetShaderLibraryPath()), &file_count, &error_count))
        {
            result.diagnostics.emplace_back("provider registry directory scan failed");
        }
        else if (error_count != 0)
        {
            result.diagnostics.emplace_back(
                "provider registry directory scan reported parse errors");
        }

        const auto *pbr_2d = registry.FindByName("pbr_surface_source");
        const auto *ntb_2d = registry.FindByName("ntb_tangent_vbo_normalmap");
        const auto *unlit = registry.FindByName("texture_source");
        if (!pbr_2d || !ntb_2d || !unlit)
        {
            result.diagnostics.emplace_back("provider metadata modules are missing");
        }
        else
        {
            const auto has_texture_reference =
                [](const ShaderCodeResourceManifest &manifest,
                   const char *name)
                {
                    for (uint32_t i = 0;
                         i < manifest.texture_reference_count;
                         ++i)
                    {
                        if (std::strcmp(
                                manifest.texture_references[i].texture_name,
                                name) == 0)
                            return true;
                    }
                    return false;
                };
            MaterialDefinition lit_definition{};
            MaterialDefinition unlit_definition{};
            if (!TryGetMaterialDefinitionByID("Lit", lit_definition)
             || !TryGetMaterialDefinitionByID(
                    "UnlitTexture",
                    unlit_definition))
            {
                result.diagnostics.emplace_back(
                    "provider texture-reference material definitions are missing");
            }

            const char *roots_2d[] = {pbr_2d->name, ntb_2d->name};
            ShaderCodeResourceManifest manifest_2d{};
            if (!BuildShaderCodeResourceManifest(
                    roots_2d, uint32_t(std::size(roots_2d)), manifest_2d, &registry))
            {
                result.diagnostics.emplace_back(
                    std::string("Texture2D provider manifest failed: ")
                    + GetShaderCodeResourceManifestErrorName(manifest_2d.error));
            }
            else
            {
                if (manifest_2d.texture_reference_count != 6
                 || !has_texture_reference(manifest_2d, "base_color")
                 || !has_texture_reference(manifest_2d, "roughness")
                 || !has_texture_reference(manifest_2d, "metallic")
                 || !has_texture_reference(manifest_2d, "occlusion")
                 || !has_texture_reference(manifest_2d, "opacity_mask")
                 || !has_texture_reference(manifest_2d, "normal")
                 || !ValidateShaderCodeResourceManifestTextureReferences(
                        manifest_2d,
                        lit_definition))
                    result.diagnostics.emplace_back(
                        "Texture2D providers must declare TOML-aligned texture references");
            }

            const char *ntb_root = ntb_2d->name;
            ShaderCodeResourceManifest ntb_manifest{};
            if (!BuildShaderCodeResourceManifest(
                    &ntb_root, 1, ntb_manifest, &registry)
             || ntb_manifest.texture_reference_count != 1
             || !has_texture_reference(ntb_manifest, "normal"))
               result.diagnostics.emplace_back(
                   "Normal-map provider must declare its named texture reference");

            const char *unlit_root = unlit->name;
            ShaderCodeResourceManifest unlit_manifest{};
            if (!BuildShaderCodeResourceManifest(
                    &unlit_root,
                    1,
                    unlit_manifest,
                    &registry)
             || unlit_manifest.texture_reference_count != 1
             || !has_texture_reference(unlit_manifest, "base_color")
             || !ValidateShaderCodeResourceManifestTextureReferences(
                    unlit_manifest,
                    unlit_definition))
            {
                result.diagnostics.emplace_back(
                    "generic texture material must use one TOML texture reference");
            }

            ShaderCodeResourceManifest invalid_reference_manifest{};
            if (invalid_reference_manifest.texture_references.Add(
                    {"missing_texture",
                     uint32_t(hgl::graph::ShaderStage::Fragment),
                     false,
                     false}) < 0)
            {
                result.diagnostics.emplace_back(
                    "invalid texture-reference manifest setup failed");
            }
            else
                invalid_reference_manifest.texture_reference_count = 1;
            const char *invalid_texture_name = nullptr;
            if (ValidateShaderCodeResourceManifestTextureReferences(
                    invalid_reference_manifest,
                    unlit_definition,
                    &invalid_texture_name)
             || !invalid_texture_name
             || std::strcmp(invalid_texture_name, "missing_texture") != 0)
            {
                result.diagnostics.emplace_back(
                    "TOML-external provider texture reference must be rejected");
            }
        }

        result.passed = result.diagnostics.empty();
        return result;
    }

    static GateResult RunShaderCodeModuleFileCase()
    {
        GateResult result;
        result.name = "I.glsl-code-module-file-parse";

        auto expect_parse = [&result](const char *content,
                                      const ShaderCodeModuleParseResult expected,
                                      const char *label)
        {
            ShaderCodeModuleFileData data;
            const ShaderCodeModuleParseResult actual =
                ParseShaderCodeModuleFile(content, int(std::strlen(content)), data);
            if (actual != expected)
            {
                result.diagnostics.emplace_back(std::string(label) + ": got "
                    + GetShaderCodeModuleParseResultName(actual)
                    + " expected " + GetShaderCodeModuleParseResultName(expected));
                return false;
            }
            return true;
        };

        // Valid full metadata block.
        const char full_meta[] =
            "// @ulre begin\n"
            "// @ulre name sample_ntb\n"
            "// @ulre kind Utility\n"
            "// @ulre priority 10\n"
            "// @ulre require GeometryAttribute Normal Float 3 3\n"
            "// @ulre require GeometryAttribute Tangent Any\n"
            "// @ulre provide Normal\n"
            "// @ulre provide Tangent\n"
            "// @ulre texture_reference normal Fragment optional fallback\n"
            "// @ulre uses s2_lift_xy0\n"
            "// @ulre end\n";
        {
            ShaderCodeModuleFileData data;
            const ShaderCodeModuleParseResult parse =
                ParseShaderCodeModuleFile(full_meta, int(std::strlen(full_meta)), data);
            if (parse != ShaderCodeModuleParseResult::OK)
                result.diagnostics.emplace_back("full-metadata parse failed");
            else
            {
                if (std::strcmp(data.name.c_str(), "sample_ntb") != 0)
                    result.diagnostics.emplace_back("full-metadata name mismatch");
                if (data.kind != ShaderCodeModuleKind::Utility)
                    result.diagnostics.emplace_back("full-metadata kind mismatch");
                if (data.priority != 10)
                    result.diagnostics.emplace_back("full-metadata priority mismatch");
                if (data.semantic_requirements.GetCount() != 2)
                    result.diagnostics.emplace_back("full-metadata require count mismatch");
                else
                {
                    const auto &normal_req = data.semantic_requirements[0];
                    if (normal_req.source != ShaderCodeModuleCapabilitySource::GeometryAttribute
                     || normal_req.semantic != ShaderCodeModuleSemantic::Normal
                     || normal_req.numeric_class_mask != uint32_t(ShaderCodeModuleNumericClass::Float)
                     || normal_req.min_component_count != 3
                     || normal_req.max_component_count != 3)
                        result.diagnostics.emplace_back("full-metadata require[0] mismatch");

                    const auto &tangent_req = data.semantic_requirements[1];
                    if (tangent_req.source != ShaderCodeModuleCapabilitySource::GeometryAttribute
                     || tangent_req.semantic != ShaderCodeModuleSemantic::Tangent
                     || tangent_req.numeric_class_mask != uint32_t(ShaderCodeModuleNumericClass::Any)
                     || tangent_req.min_component_count != 0
                     || tangent_req.max_component_count != 0)
                        result.diagnostics.emplace_back("full-metadata require[1] mismatch");
                }
                if (data.semantic_provides.GetCount() != 2
                 || data.semantic_provides[0] != ShaderCodeModuleSemantic::Normal
                 || data.semantic_provides[1] != ShaderCodeModuleSemantic::Tangent)
                    result.diagnostics.emplace_back("full-metadata provide mismatch");
                if (data.texture_reference_requirements.GetCount() != 1
                 || !data.texture_reference_requirements[0].texture_name
                 || std::strcmp(
                        data.texture_reference_requirements[0].texture_name,
                        "normal") != 0
                 || data.texture_reference_requirements[0].stage_flags
                    != uint32_t(hgl::graph::ShaderStage::Fragment)
                 || data.texture_reference_requirements[0].required
                 || !data.texture_reference_requirements[0].allow_fallback)
                {
                    result.diagnostics.emplace_back(
                        "full-metadata texture-reference mismatch");
                }
                if (data.pending_module_requirements.GetCount() != 1
                 || std::strcmp(data.pending_module_requirements[0].c_str(), "s2_lift_xy0") != 0)
                    result.diagnostics.emplace_back("full-metadata uses mismatch");
            }
        }

        expect_parse("// @ulre begin\n// @ulre end\n", ShaderCodeModuleParseResult::OK, "minimal");
        expect_parse("void main() {}\n", ShaderCodeModuleParseResult::Skipped, "no-metadata");
        expect_parse("// @ulre name x\n// @ulre begin\n// @ulre end\n", ShaderCodeModuleParseResult::MissingBegin, "missing-begin");
        expect_parse("// @ulre begin\n// @ulre begin\n// @ulre end\n", ShaderCodeModuleParseResult::DuplicateBegin, "duplicate-begin");
        expect_parse("// @ulre begin\n// @ulre name x\n", ShaderCodeModuleParseResult::MissingEnd, "missing-end");
        expect_parse("// @ulre begin\n// @ulre nope 1\n// @ulre end\n", ShaderCodeModuleParseResult::UnknownDirective, "unknown-directive");
        expect_parse("// @ulre begin\n// @ulre name a\n// @ulre name b\n// @ulre end\n", ShaderCodeModuleParseResult::DuplicateDirective, "duplicate-directive");
        expect_parse("// @ulre begin\n// @ulre kind\n// @ulre end\n", ShaderCodeModuleParseResult::MissingDirectiveArgument, "missing-argument");
        expect_parse("// @ulre begin\n// @ulre kind NoSuchKind\n// @ulre end\n", ShaderCodeModuleParseResult::InvalidKind, "invalid-kind");
        expect_parse("// @ulre begin\n// @ulre provide NoSuchSemantic\n// @ulre end\n", ShaderCodeModuleParseResult::InvalidSemantic, "invalid-semantic");
        expect_parse("// @ulre begin\n// @ulre require BadSource Normal\n// @ulre end\n", ShaderCodeModuleParseResult::InvalidSource, "invalid-source");
        expect_parse("// @ulre begin\n// @ulre require GeometryAttribute Normal NotAClass\n// @ulre end\n", ShaderCodeModuleParseResult::InvalidNumericClass, "invalid-numclass");
        expect_parse("// @ulre begin\n// @ulre priority notanumber\n// @ulre end\n", ShaderCodeModuleParseResult::InvalidNumber, "invalid-number");
        expect_parse("// @ulre begin\n// @ulre texture_reference invalid-name Fragment\n// @ulre end\n", ShaderCodeModuleParseResult::InvalidResource, "invalid-texture-reference");
        expect_parse("// @ulre begin\n// @ulre conflicts\n// @ulre end\n", ShaderCodeModuleParseResult::UnknownDirective, "conflicts-directive-removed");

        // Registry scan of the real ShaderLibrary directory.
        ShaderCodeModuleRegistry registry;

        int file_count = 0;
        int error_count = 0;
        if (!registry.LoadDirectory(hgl::ToOSString(GetShaderLibraryPath()), &file_count, &error_count))
            result.diagnostics.emplace_back("LoadDirectory failed to scan directory");
        else
        {
            // Consolidated scene Set 0 UBOs into single scene_ubo.glsl; added RenderItemResolve.glsl, shadow_receiver_source.glsl, pcf_shadow.glsl.
            // Stage 5 removed the duplicate ntb_derivative_normalmap provider (71 → 70).
            // 回退标记色：新增 error_checkerboard_3d_source（70 → 71）。
            // 阶段一：新增 pass_environment_interface（71 → 72）。
            // 超薄架构阶段一：直接光拆成 diffuse/specular 两槽，删除
            // direct_cook_torrance_pbr，新增 diffuse_{lambert,burley,toon,none}
            // + specular_{cook_torrance,toon_step,none} + indirect_none +
            // forward_toon + shadow/full（72 → 81）。
            // 超薄架构阶段二之二：新增 ntb_none 空对象（81 → 82）。
            // 顶点色统一阶段：新增 s1_color_default 空对象（82 → 83）。
            // 网格阶段收口 Layer 2a：新增 s1_stream_defaults 空对象（83 → 84）。
            // 计数常量已收敛到 `kShaderCodeModuleCount`（文件顶部），增删模块时
            // 只改那一处并在此追加一行理由。
            if (file_count != kShaderCodeModuleCount)
                result.diagnostics.emplace_back("LoadDirectory expected "
                    + std::to_string(kShaderCodeModuleCount)
                    + " file modules, got "
                    + std::to_string(file_count));
            if (error_count != 0)
                result.diagnostics.emplace_back("LoadDirectory reported "
                    + std::to_string(error_count) + " errors");

            if (registry.GetCount() != kShaderCodeModuleCount)
                result.diagnostics.emplace_back("registry count after LoadDirectory mismatch: got "
                    + std::to_string(registry.GetCount()));

            ShaderCodeModuleMetadataValidationDiagnostic metadata_diagnostic{};
            if (!ValidateShaderCodeModuleRegistryMetadata(
                    registry, metadata_diagnostic))
            {
                result.diagnostics.emplace_back(
                    "formal metadata validation failed: error="
                    + std::string(
                        GetShaderCodeModuleMetadataValidationErrorName(
                            metadata_diagnostic.error))
                    + " module="
                    + std::string(metadata_diagnostic.module_name.c_str())
                    + " related="
                    + std::string(
                        metadata_diagnostic.related_module_name.c_str()));
            }
        }

        const auto *lift = registry.FindByName("s2_lift_xy0");
        if (!lift)
            result.diagnostics.emplace_back("s2_lift_xy0 not found by name");
        else if (lift->kind != ShaderCodeModuleKind::Position
              || lift->semantic_requirement_count != 1
              || lift->semantic_requirements[0].semantic != ShaderCodeModuleSemantic::Position
              || lift->semantic_requirements[0].min_component_count != 2
              || lift->semantic_requirements[0].max_component_count != 2)
            result.diagnostics.emplace_back("s2_lift_xy0 capability metadata mismatch");

        const auto *surface_interface = registry.FindByName("surface_interface");
        if (!surface_interface)
            result.diagnostics.emplace_back("surface_interface not found by name");
        else if (surface_interface->kind != ShaderCodeModuleKind::Shared)
            result.diagnostics.emplace_back("surface_interface kind mismatch");

        // 单趟发射改造（2026-09）：compositor 骨架降级为发射器常量，
        // skeleton key 为 "forward_surface" / "depth_only" / "forward_sky"。

        const auto *forward_input = registry.FindByName("forward_lighting");
        if (!forward_input || forward_input->kind != ShaderCodeModuleKind::Utility)
            result.diagnostics.emplace_back("forward_lighting input assembly module is missing or has wrong kind");

        const auto *forward_algorithm = registry.FindByName("forward_pbr");
        if (!forward_algorithm || forward_algorithm->kind != ShaderCodeModuleKind::Utility)
            result.diagnostics.emplace_back("forward_pbr lighting algorithm module is missing or has wrong kind");

        const auto *alternate_algorithm = registry.FindByName("forward_flat");
        if (!alternate_algorithm || alternate_algorithm->kind != ShaderCodeModuleKind::Utility)
            result.diagnostics.emplace_back("forward_flat alternate lighting algorithm is missing or has wrong kind");

        // 超薄架构阶段一：直接光拆成两条正交轴，每条轴都必须有“真实实现 + 空对象”。
        // 缺任何一个都会让“换模型 = 换一行 include”的承诺失效，因此在这里钉住。
        const auto require_slot_module = [&](const char *name,
                                             const char *slot_name)
        {
            const auto *module = registry.FindByName(name);
            if (!module)
            {
                result.diagnostics.emplace_back(
                    std::string(name) + " slot module is missing");
                return;
            }
            if (module->kind != ShaderCodeModuleKind::Utility)
                result.diagnostics.emplace_back(
                    std::string(name) + " kind mismatch (expected Utility)");
            if (std::strcmp(GetShaderModuleSlotRoleName(module->slot_role),
                            slot_name) != 0)
                result.diagnostics.emplace_back(
                    std::string(name) + " declares wrong slot (expected "
                    + slot_name + ")");
        };

        for (const char *name : { "diffuse_lambert", "diffuse_burley",
                                  "diffuse_toon", "diffuse_none" })
            require_slot_module(name, "diffuse_light_provider");
        for (const char *name : { "specular_cook_torrance", "specular_toon_step",
                                  "specular_none" })
            require_slot_module(name, "specular_light_provider");
        require_slot_module("indirect_none", "ambient_light_provider");
        require_slot_module("forward_toon", "lighting_model");
        require_slot_module("full_shadow", "shadow_provider");
        // 超薄架构阶段二之二：NTB / AO 变成「必需槽 + 空对象补位」，
        // 用空对象替掉编译期开关（HGL_USE_NTB_PROVIDER / HGL_USE_AMBIENT_OCCLUSION_PROVIDER）。
        require_slot_module("ntb_none", "ntb_provider");
        require_slot_module("identity_ao", "ambient_occlusion_provider");


        result.passed = result.diagnostics.empty();
        return result;
    }

    static GateResult RunShaderCodeModuleMetadataValidationCase()
    {
        GateResult result;
        result.name = "I1.glsl-code-module-metadata-validation";

        const auto make_definition = [](
            const char *name) -> ShaderCodeModuleDefinition
        {
            ShaderCodeModuleDefinition definition{};
            definition.name = name;
            definition.glsl_code = "// metadata validation";
            definition.kind = ShaderCodeModuleKind::Utility;
            return definition;
        };

        {
            const ShaderCodeModuleSemantic duplicate_provides[] =
            {
                ShaderCodeModuleSemantic::Position,
                ShaderCodeModuleSemantic::Position
            };
            ShaderCodeModuleDefinition definition =
                make_definition("duplicate_provide");
            definition.semantic_provides = duplicate_provides;
            definition.semantic_provide_count = 2;

            ShaderCodeModuleMetadataValidationDiagnostic diagnostic{};
            if (ValidateShaderCodeModuleMetadata(definition, diagnostic)
             || diagnostic.error
                    != ShaderCodeModuleMetadataValidationError::DuplicateProvide)
            {
                result.diagnostics.emplace_back(
                    "duplicate provide metadata must be rejected");
            }
        }

        {
            // 依赖环的权威判据是装配期 DFS（`BuildShaderCodeResourceManifest`），
            // 不是元数据静态校验 —— 元数据层不再重复检测。
            ShaderCodeModuleDefinition first =
                make_definition("cycle_first");
            ShaderCodeModuleDefinition second =
                make_definition("cycle_second");
            const ShaderCodeModuleDependency first_dependencies[] =
            {
                {second.name}
            };
            const ShaderCodeModuleDependency second_dependencies[] =
            {
                {first.name}
            };
            first.dependencies = first_dependencies;
            first.dependency_count = 1;
            second.dependencies = second_dependencies;
            second.dependency_count = 1;

            ShaderCodeModuleRegistry registry;
            ShaderCodeResourceManifest manifest{};
            const char *roots[] = {first.name};
            if (!registry.Register(first)
             || !registry.Register(second)
             || BuildShaderCodeResourceManifest(roots, 1, manifest, &registry)
             || manifest.error
                    != ShaderCodeResourceManifestError::CodeModuleCycle)
            {
                result.diagnostics.emplace_back(
                    "explicit dependency cycle must be rejected by the assembly DFS");
            }

            const char *missing_roots[] = {"cycle_first_missing_probe"};
            ShaderCodeModuleDefinition orphan =
                make_definition(missing_roots[0]);
            const ShaderCodeModuleDependency orphan_dependencies[] =
            {
                {"no_such_module"}
            };
            orphan.dependencies = orphan_dependencies;
            orphan.dependency_count = 1;

            ShaderCodeModuleRegistry orphan_registry;
            ShaderCodeResourceManifest orphan_manifest{};
            if (!orphan_registry.Register(orphan)
             || BuildShaderCodeResourceManifest(
                    missing_roots, 1, orphan_manifest, &orphan_registry)
             || orphan_manifest.error
                    != ShaderCodeResourceManifestError::UnknownCodeModule)
            {
                result.diagnostics.emplace_back(
                    "missing dependency must be rejected by the assembly DFS");
            }
        }

        {
            const ShaderCodeModuleSemantic position[] =
            {
                ShaderCodeModuleSemantic::Position
            };
            ShaderCodeModuleDefinition first =
                make_definition("ambiguous_first");
            ShaderCodeModuleDefinition second =
                make_definition("ambiguous_second");
            first.semantic_provides = position;
            first.semantic_provide_count = 1;
            second.semantic_provides = position;
            second.semantic_provide_count = 1;

            ShaderCodeModuleRegistry registry;
            ShaderCodeModuleMetadataValidationDiagnostic diagnostic{};
            if (!registry.Register(first)
             || !registry.Register(second)
             || ValidateShaderCodeModuleRegistryMetadata(registry, diagnostic)
             || diagnostic.error
                    != ShaderCodeModuleMetadataValidationError::
                        AmbiguousProviderPriority)
            {
                result.diagnostics.emplace_back(
                    "equal-priority providers must be rejected as ambiguous");
            }
        }

        {
            ShaderCodeModuleDefinition base =
                make_definition("metadata_hash");
            ShaderCodeModuleDefinition changed = base;
            changed.priority = 10;
            if (GetShaderCodeModuleDefinitionHash(base)
                == GetShaderCodeModuleDefinitionHash(changed))
            {
                result.diagnostics.emplace_back(
                    "module metadata changes must affect definition hash");
            }
        }

        result.passed = result.diagnostics.empty();
        return result;
    }

    static GateResult RunMaterialBDAFixedBindingsCase()
    {
        GateResult result;
        result.name = "Z.material-bda-fixed-bindings";
        const StageSPVCompilerInput compiler_input{
            "MaterialPrivateDataMaterial",
            PrimitiveType::Triangles
        };

        const std::string root_bindings =
            ReadFileText(RepoRootPath("ShaderLibrary/domain/root_bindings.glsl"));
        const std::string emissive_bindings =
            ReadFileText(RepoRootPath("ShaderLibrary/domain/emissive_row_bindings.glsl"));
        // 结构本体已拆到独立文件（由 C++ 结构 + 注解生成）；bindings 只留寻址宏。
        const std::string emissive_row_glsl =
            ReadFileText(RepoRootPath("ShaderLibrary/ssbo/emissive_surface_row.glsl"));

        // 本门用**合成源**（root_bindings + emissive bindings 文本直拼），不经过编译器
        // 的正文拼接 ⇒ 独立结构文件的 `#include` 要在这里就地展开，否则
        // `GetFinalGLSL()` 里只有一行 #include、看不到结构声明。
        std::string emissive_bindings_expanded = emissive_bindings;
        static constexpr char kRowInclude[] = "#include \"ssbo/emissive_surface_row.glsl\"";
        {
            const size_t include_pos = emissive_bindings_expanded.find(kRowInclude);
            if (include_pos != std::string::npos)
                emissive_bindings_expanded.replace(include_pos,
                                                   sizeof(kRowInclude) - 1,
                                                   emissive_row_glsl);
        }

        const std::string emissive_frag_source =
            root_bindings + "\n" + emissive_bindings_expanded;

        StageSPVCompileConfig config{};
        config.material_private_data = GlobalSSBOType::EmissiveSurface;
        config.defer_finalize = true;
        config.fixed_mesh_source = "#version 460\nvoid main(){}\n";
        config.fixed_fragment_source = emissive_frag_source.c_str();
        // mesh 化后顶点路径统一走 Mesh stage（VS 已彻底废弃）
        config.shader_stage_flag_bits = uint32_t(hgl::graph::mtl::ShaderStage::MeshFragment);

        ShaderBuildContext *build_spec = CompileMaterialStages(
            nullptr,
            compiler_input,
            config);
        if (!build_spec)
        {
            result.diagnostics.emplace_back("material compiler did not produce a build spec");
            result.passed = false;
            return result;
        }

        const ShaderCreateInfo *fragment =
            build_spec->GetStageShader(ShaderStage::Fragment);
        if (!fragment)
        {
            result.diagnostics.emplace_back("material compiler did not produce a fragment stage");
        }
        else
        {
            const std::string &source = fragment->GetFinalGLSL();
            const auto count_occurrences = [&source](const char *token)
            {
                size_t count = 0;
                size_t offset = 0;
                while (true)
                {
                    const size_t found = source.find(token, offset);
                    if (found == std::string::npos)
                        break;
                    ++count;
                    offset = found + 1;
                }
                return count;
            };

            // Arena+BDA：材质数据经设备地址行表寻址。
            // 域固定架构后这些文本**不再被发射**：它们物理固定在
            // ShaderLibrary/domain/emissive_row_bindings.glsl 里。所以判据改成
            // 「库文件自己声明的行别名，是否原样、只出现一次地进了最终 stage 文档」——
            // 测试不再自己拼一份期望字符串去回声（那样连发射器丢了整段内容都能过）。
            const std::string expected_alias = ExtractDefineLine(
                emissive_bindings, "#define MTL_ROW(i)");
            if (expected_alias.empty())
            {
                result.diagnostics.emplace_back(
                    "emissive_row_bindings.glsl 不再定义 MTL_ROW 行别名"
                    "（材质私有数据行无法按 payload_index 解引用）");
            }
            else
            {
                const size_t alias_hits = count_occurrences(expected_alias.c_str());
                if (alias_hits != 1)
                {
                    result.diagnostics.emplace_back(
                        "库文件的 MTL_ROW 行别名没有恰好一次地进入 stage 文档（hits="
                        + std::to_string(alias_hits) + "）");
                }
            }

            // 三要素（值结构 / 行引用 / 行别名）的声明点是库文件本身：
            // 少一个、改名、或多个文件重复声明，都在这里硬失败。
            for (const char *required : {"struct EmissiveSurfaceData",
                                         "buffer EmissiveSurfaceRow",
                                         "MaterialInstanceAddressesRef"})
            {
                if (emissive_bindings.find(required) == std::string::npos
                 && root_bindings.find(required) == std::string::npos
                 && emissive_row_glsl.find(required) == std::string::npos)
                    result.diagnostics.emplace_back(
                        std::string("库文件缺少材质私有数据行声明：") + required);
            }

            // payload 行需求必须与 MTL_ROW 一起出现：运行期的材质私有数据 SSBO
            // 绑定校验只认这一位。
            if (!build_spec->RequiresRecipePayloadRow())
                result.diagnostics.emplace_back(
                    "payload material did not report a payload row requirement");

            if (source.find("struct MaterialInstanceAddresses") == std::string::npos
             || source.find("MaterialInstanceAddresses values[]") == std::string::npos)
                result.diagnostics.emplace_back(
                    "dual-address material instance table was not emitted");

            if (count_occurrences("struct EmissiveSurfaceData") != 1)
                result.diagnostics.emplace_back("repeated SSBO type emitted duplicate GLSL struct");

            if (source.find("buffer EmissiveSurfaceRow") == std::string::npos)
                result.diagnostics.emplace_back("arena row reference declaration is incomplete");

            const size_t extension = source.find(
                "#extension GL_EXT_mesh_shader : require");
            const size_t declaration = source.find(
                "struct EmissiveSurfaceData");
            const size_t alias = source.find(expected_alias);
            if (extension == std::string::npos)
                result.diagnostics.emplace_back("extension missing");
            if (declaration == std::string::npos)
                result.diagnostics.emplace_back("declaration missing");
            if (alias == std::string::npos)
                result.diagnostics.emplace_back("alias missing");
            if (extension == std::string::npos
             || declaration == std::string::npos
             || alias == std::string::npos
             || !(extension < declaration && declaration < alias))
            {
                result.diagnostics.emplace_back(
                    "material stage document injection order changed: ext=" + std::to_string(extension) + " decl=" + std::to_string(declaration) + " alias=" + std::to_string(alias));
            }
        }

        delete build_spec;

        // 独立纯纹理材质：没有 payload 时仍必须发射双地址行表和 MTL_TEX。
        // 说明：这里的片元源是**合成**的（真实库里 `standard_opaque_bindings.glsl`
        // 同时声明 MTL_ROW/MTL_TEX，无法用来证明「只有贴图行时不会多发 payload 行」）。
        // 本段要证明的是编译器的**判定语义**，不是库文件内容，故刻意自建最小源。
        MaterialDefinition texture_definition{};
        texture_definition.definition_id = "MaterialTextureReferenceOnly";
        texture_definition.texture_declarations.push_back(
            {"base_color", SamplerType::Sampler2D, true});
        texture_definition.texture_declarations.push_back(
            {"normal", SamplerType::Sampler2DArray, false});
        texture_definition.vertex_varying.emit_data_index_id = false;

        const std::string texture_frag_source =
            root_bindings + "\n"
            "struct MaterialTextureReferences { uvec2 tex_base_color; uvec2 tex_normal; };\n"
            "layout(buffer_reference, scalar, buffer_reference_align=16) buffer MaterialTextureReferencesRef { uvec2 tex_base_color; uvec2 tex_normal; };\n"
            "#define MTL_TEX(i) MaterialTextureReferencesRef(pc_root.addr_texture_references + uint64_t(MaterialInstanceAddressesRef(pc_root.addr_mtl_data_addrs).values[(i)].texture_reference_index) * uint64_t(16))\n";

        StageSPVCompilerInput texture_compiler_input{
            "MaterialTextureReferenceOnly",
            PrimitiveType::Triangles};
        StageSPVCompileConfig texture_config{};
        texture_config.material_private_data = GlobalSSBOType::PBRSurface;
        texture_config.material_definition = &texture_definition;
        texture_config.defer_finalize = true;
        texture_config.fixed_mesh_source = "#version 460\nvoid main(){}\n";
        texture_config.fixed_fragment_source = texture_frag_source.c_str();
        texture_config.shader_stage_flag_bits =
            uint32_t(hgl::graph::mtl::ShaderStage::MeshFragment);

        ShaderBuildContext *texture_build_spec = CompileMaterialStages(
            nullptr,
            texture_compiler_input,
            texture_config);
        if (!texture_build_spec)
        {
            result.diagnostics.emplace_back(
                "texture-reference-only compiler did not produce a build spec");
        }
        else
        {
            if (!texture_build_spec->RequiresRecipeRuntimeRows())
            {
                result.diagnostics.emplace_back(
                    "pure texture material did not require runtime address rows");
            }

            // 合并信号为真，但 payload 行需求必须是假 —— 运行期靠这一位决定
            // 「recipe 是否必须给 material_ssbo_binding」，两者混淆会误杀纯纹理材质。
            if (texture_build_spec->RequiresRecipePayloadRow())
            {
                result.diagnostics.emplace_back(
                    "pure texture material reported a payload row requirement");
            }

            const ShaderCreateInfo *texture_fragment =
                texture_build_spec->GetStageShader(ShaderStage::Fragment);
            if (!texture_fragment)
            {
                result.diagnostics.emplace_back(
                    "texture-reference-only compiler did not produce a fragment stage");
            }
            else
            {
                const std::string &source =
                    texture_fragment->GetFinalGLSL();
                if (source.find("struct MaterialInstanceAddresses")
                        == std::string::npos
                 || source.find("MaterialInstanceAddresses values[]")
                        == std::string::npos
                 || source.find("buffer MaterialTextureReferencesRef")
                        == std::string::npos
                 || source.find("uvec2 tex_base_color;")
                        == std::string::npos
                 || source.find("uvec2 tex_normal;")
                        == std::string::npos
                 || source.find(
                        "#define MTL_TEX(i) MaterialTextureReferencesRef(pc_root.addr_texture_references + uint64_t(MaterialInstanceAddressesRef(pc_root.addr_mtl_data_addrs).values[(i)].texture_reference_index) * uint64_t(16))")
                        == std::string::npos)
                    result.diagnostics.emplace_back(
                        "named material texture reference row was not emitted");

                if (source.find("#define MTL_ROW(i)") != std::string::npos)
                    result.diagnostics.emplace_back(
                        "payload row alias emitted for a pure texture material");
            }

            delete texture_build_spec;
        }

        // 无纹理材质（VertexColor / Checkerboard 类）不一定需要 payload 行：
        // 判据是材质提供的 GLSL 模块有没有 `require Resource MaterialData`。
        // 这类材质既不读 MTL_ROW 也不该向 recipe 索要 material_ssbo_binding，
        // 按「有没有纹理声明」反推会把它们全部误杀。
        MaterialDefinition vertex_color_definition{};
        vertex_color_definition.definition_id = "MaterialPayloadFreeVertexColor";
        vertex_color_definition.material_source_module_name = "vertex_color_source";
        vertex_color_definition.vertex_varying.emit_data_index_id = false;

        StageSPVCompilerInput vertex_color_compiler_input{
            "MaterialPayloadFreeVertexColor",
            PrimitiveType::Triangles};
        StageSPVCompileConfig vertex_color_config{};
        vertex_color_config.material_private_data = GlobalSSBOType::PBRSurface;
        vertex_color_config.material_definition = &vertex_color_definition;
        vertex_color_config.defer_finalize = true;
        vertex_color_config.fixed_mesh_source = "#version 460\nvoid main(){}\n";
        vertex_color_config.fixed_fragment_source = root_bindings.c_str();
        vertex_color_config.shader_stage_flag_bits =
            uint32_t(hgl::graph::mtl::ShaderStage::MeshFragment);

        ShaderBuildContext *vertex_color_build_spec = CompileMaterialStages(
            nullptr,
            vertex_color_compiler_input,
            vertex_color_config);
        if (!vertex_color_build_spec)
        {
            result.diagnostics.emplace_back(
                "no-texture vertex-color compiler did not produce a build spec");
        }
        else
        {
            if (vertex_color_build_spec->RequiresRecipePayloadRow())
                result.diagnostics.emplace_back(
                    "no-texture vertex-color material reported a payload row requirement");

            const ShaderCreateInfo *vertex_color_fragment =
                vertex_color_build_spec->GetStageShader(ShaderStage::Fragment);
            if (!vertex_color_fragment)
            {
                result.diagnostics.emplace_back(
                    "no-texture vertex-color compiler did not produce a fragment stage");
            }
            else
            {
                const std::string &source =
                    vertex_color_fragment->GetFinalGLSL();
                if (source.find("#define MTL_ROW(i)") != std::string::npos)
                    result.diagnostics.emplace_back(
                        "payload row alias emitted for a payload-free material");
                if (source.find("#define MTL_TEX(i)") != std::string::npos)
                    result.diagnostics.emplace_back(
                        "texture reference alias emitted for a texture-free material");
                if (source.find("struct MaterialInstanceAddresses")
                        == std::string::npos)
                    result.diagnostics.emplace_back(
                        "material instance address rows missing for a payload-free material");
            }

            delete vertex_color_build_spec;
        }

        // 反向守卫：同样没有纹理声明，但材质源显式声明了 MaterialData
        // （pbr_surface_source）⇒ payload 行需求必须为真，别把判据放宽成恒假。
        MaterialDefinition pbr_definition{};
        pbr_definition.definition_id = "MaterialPayloadDeclaredNoTexture";
        pbr_definition.material_source_module_name = "pbr_surface_source";
        pbr_definition.vertex_varying.emit_data_index_id = true;

        StageSPVCompilerInput pbr_compiler_input{
            "MaterialPayloadDeclaredNoTexture",
            PrimitiveType::Triangles};
        StageSPVCompileConfig pbr_config{};
        pbr_config.material_private_data = GlobalSSBOType::PBRSurface;
        pbr_config.material_definition = &pbr_definition;
        pbr_config.defer_finalize = true;
        pbr_config.fixed_mesh_source = "#version 460\nvoid main(){}\n";
        pbr_config.fixed_fragment_source = root_bindings.c_str();
        pbr_config.shader_stage_flag_bits =
            uint32_t(hgl::graph::mtl::ShaderStage::MeshFragment);

        ShaderBuildContext *pbr_build_spec = CompileMaterialStages(
            nullptr,
            pbr_compiler_input,
            pbr_config);
        if (!pbr_build_spec)
        {
            result.diagnostics.emplace_back(
                "payload-declared compiler did not produce a build spec");
        }
        else
        {
            if (!pbr_build_spec->RequiresRecipePayloadRow())
                result.diagnostics.emplace_back(
                    "texture-free material with a MaterialData source lost its payload row requirement");

            delete pbr_build_spec;
        }

        result.passed = result.diagnostics.empty();
        return result;
    }

    static GateResult RunBindingMacroSingleSourceCase()
    {
        GateResult result;
        result.name = "AE.binding-macro-single-source";

        // 固定 ABI 的 set/binding 宏唯一真源是 descriptor_macros.glsl（生成物）。
        // 编译器不得再注入 #define（原 kBindingDefineTable 注入路径已删除，Phase 3）。
        // 输入 GLSL 特意 include 并使用宏（与真实模板一致）——旧注入路径会因此向
        // mesh 阶段注入 L2W_SET/L2W_BINDING（该注入路径已删除，本用例钉住不许复活）。
        const StageSPVCompilerInput compiler_input{
            "BindingMacroSingleSourceMaterial",
            PrimitiveType::Triangles
        };

        static const char kMeshSource[] =
            "#version 460\n"
            "#include \"common/descriptor_macros.glsl\"\n"
            "layout(location=0) in vec2 Position;\n"
            "void main(){gl_Position=vec4(Position,L2W_SET,L2W_BINDING);}\n";
        static const char kFragSource[] =
            "#version 460\n"
            "layout(location=0) out vec4 outColor;\n"
            "void main(){outColor=vec4(1.0);}\n";

        StageSPVCompileConfig config{};
        config.defer_finalize = true;
        config.fixed_mesh_source = kMeshSource;
        config.fixed_fragment_source = kFragSource;
        config.shader_stage_flag_bits = uint32_t(hgl::graph::mtl::ShaderStage::MeshFragment);

        ShaderBuildContext *build_spec = CompileMaterialStages(
            nullptr,
            compiler_input,
            config);
        if (!build_spec)
        {
            result.diagnostics.emplace_back("compiler did not produce a build spec");
            result.passed = false;
            return result;
        }

        static const char *injected_defines[] = {
            "#define L2W_SET",
            "#define L2W_BINDING",
            "#define MESH_DRAW_PARAMS_SET",
            "#define MESH_DRAW_PARAMS_BINDING",
            "#define VIEWPORT_SET",
            "#define CAMERA_SET",
            "#define SKY_SET",
            "#define COLOR_PALETTE_SET",
        };

        const ShaderCreateInfo *mesh = build_spec->GetStageShader(ShaderStage::Mesh);
        if (!mesh)
        {
            result.diagnostics.emplace_back("did not produce a mesh stage");
        }
        else
        {
            const std::string &source = mesh->GetFinalGLSL();
            if (source.find("#include \"common/descriptor_macros.glsl\"")
                == std::string::npos)
                result.diagnostics.emplace_back("descriptor_macros include is missing");

            for (const char *macro : injected_defines)
                if (source.find(macro) != std::string::npos)
                    result.diagnostics.emplace_back(
                        std::string("compiler injected fixed-ABI define: ") + macro);
        }

        delete build_spec;
        result.passed = result.diagnostics.empty();
        return result;
    }

    // S3a（2026-10-11）：Vk 枚举 name↔value 单一真源（inc/hgl/mtl/VkEnumText.h）。
    // 判据用 for 遍历全表 —— 表里新增一项自动纳入，不需要手列断言。
    static GateResult RunVkEnumTextCase()
    {
        GateResult result;
        result.name = "C.vk-enum-text";

        // ① 每个条目：正查能命中且值一致，反查名字一致。
        for (const auto &e : hgl::graph::mtl::kCullModeText)
        {
            VkCullModeFlags parsed = VK_CULL_MODE_NONE;
            if (!hgl::graph::mtl::ParseVkCullMode(e.text, parsed) || parsed != e.value)
                result.diagnostics.emplace_back(std::string("CullMode 条目不可正查: ") + e.text);
            if (hgl::graph::mtl::GetVkCullModeName(e.value) == nullptr)
                result.diagnostics.emplace_back(std::string("CullMode 条目不可反查: ") + e.text);
        }
        for (const auto &e : hgl::graph::mtl::kCompareOpText)
        {
            VkCompareOp parsed = VK_COMPARE_OP_NEVER;
            if (!hgl::graph::mtl::ParseVkCompareOp(e.text, parsed) || parsed != e.value)
                result.diagnostics.emplace_back(std::string("CompareOp 条目不可正查: ") + e.text);
            if (hgl::graph::mtl::GetVkCompareOpName(e.value) == nullptr)
                result.diagnostics.emplace_back(std::string("CompareOp 条目不可反查: ") + e.text);
        }
        for (const auto &e : hgl::graph::mtl::kBlendFactorText)
        {
            VkBlendFactor parsed = VK_BLEND_FACTOR_ZERO;
            if (!hgl::graph::mtl::ParseVkBlendFactor(e.text, parsed) || parsed != e.value)
                result.diagnostics.emplace_back(std::string("BlendFactor 条目不可正查: ") + e.text);
            if (hgl::graph::mtl::GetVkBlendFactorName(e.value) == nullptr)
                result.diagnostics.emplace_back(std::string("BlendFactor 条目不可反查: ") + e.text);
        }
        for (const auto &e : hgl::graph::mtl::kFilterText)
        {
            VkFilter parsed = VK_FILTER_LINEAR;
            if (!hgl::graph::mtl::ParseVkFilter(e.text, parsed) || parsed != e.value)
                result.diagnostics.emplace_back(std::string("Filter 条目不可正查: ") + e.text);
            if (hgl::graph::mtl::GetVkFilterName(e.value) == nullptr)
                result.diagnostics.emplace_back(std::string("Filter 条目不可反查: ") + e.text);
        }
        for (const auto &e : hgl::graph::mtl::kSamplerMipmapModeText)
        {
            VkSamplerMipmapMode parsed = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            if (!hgl::graph::mtl::ParseVkSamplerMipmapMode(e.text, parsed) || parsed != e.value)
                result.diagnostics.emplace_back(std::string("MipmapMode 条目不可正查: ") + e.text);
            if (hgl::graph::mtl::GetVkSamplerMipmapModeName(e.value) == nullptr)
                result.diagnostics.emplace_back(std::string("MipmapMode 条目不可反查: ") + e.text);
        }
        for (const auto &e : hgl::graph::mtl::kSamplerAddressModeText)
        {
            VkSamplerAddressMode parsed = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            if (!hgl::graph::mtl::ParseVkSamplerAddressMode(e.text, parsed) || parsed != e.value)
                result.diagnostics.emplace_back(std::string("AddressMode 条目不可正查: ") + e.text);
            if (hgl::graph::mtl::GetVkSamplerAddressModeName(e.value) == nullptr)
                result.diagnostics.emplace_back(std::string("AddressMode 条目不可反查: ") + e.text);
        }

        // ② 未知名 / 空名必须被拒（不静默落默认值）。
        {
            VkCompareOp op = VK_COMPARE_OP_NEVER;
            if (hgl::graph::mtl::ParseVkCompareOp("Bogus", op)
             || hgl::graph::mtl::ParseVkCompareOp("", op)
             || hgl::graph::mtl::ParseVkCompareOp(nullptr, op))
                result.diagnostics.emplace_back("CompareOp 未知名/空名必须被拒");

            VkFilter f = VK_FILTER_LINEAR;
            if (hgl::graph::mtl::ParseVkFilter("Trilinear", f))
                result.diagnostics.emplace_back("Filter 未知名必须被拒");

            VkSamplerAddressMode am = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            if (hgl::graph::mtl::ParseVkSamplerAddressMode("Clamp", am))
                result.diagnostics.emplace_back("AddressMode 未知名必须被拒");

            VkBlendFactor bf = VK_BLEND_FACTOR_ZERO;
            if (hgl::graph::mtl::ParseVkBlendFactor("Src", bf))
                result.diagnostics.emplace_back("BlendFactor 未知名必须被拒");

            VkCullModeFlags cm = VK_CULL_MODE_NONE;
            if (hgl::graph::mtl::ParseVkCullMode("FrontAndBackBit", cm))
                result.diagnostics.emplace_back("CullMode 未知名必须被拒");
        }

        result.passed = result.diagnostics.empty();
        return result;
    }

    static GateResult RunSamplerPresetLibraryCase()
    {
        GateResult result;
        result.name = "AC.sampler-preset-library";

        auto &lib = SamplerPresetLibrary::Instance();

        // 1. 加载 sampler.toml（统一注册机制唯一数据源）。
        const std::string root = GetShaderLibraryPath();
        const hgl::filesystem::Path sampler_toml(
            hgl::ToOSString(root + "/sampler.toml"));
        if (!lib.Load(sampler_toml.ToOSString()))
        {
            result.diagnostics.emplace_back("sampler.toml load failed");
            result.passed = false;
            return result;
        }

        // 2. 名字→索引一致（顺序 = sampler.toml 数组顺序）。
        struct NameIndex
        {
            const char *name;
            uint32_t index;
        };
        const NameIndex expected[] =
        {
            { "Nearest",        0u },
            { "Linear",         1u },
            { "Trilinear",      2u },
            { "TrilinearAniso", 3u },
            { "ShadowPCF",      4u },
            { "Terrain",        5u },
            { "UI",             6u },
        };
        if (lib.GetCount() != uint32_t(std::size(expected)))
        {
            result.diagnostics.emplace_back(
                "preset count mismatch: expected "
                + std::to_string(std::size(expected))
                + " got " + std::to_string(lib.GetCount()));
        }
        for (const auto &e : expected)
        {
            if (lib.GetIndex(e.name) != e.index)
            {
                result.diagnostics.emplace_back(
                    std::string("name->index mismatch: ") + e.name);
            }
        }

        // 3. 未知名 / 空名 / 空指针 → 显式无效（~0u）——调用方必须处理，
        //    不再静默保底 0（=Nearest）掩盖 sampler.toml 顺序错位。
        if (lib.GetIndex("DoesNotExistSampler") != ~0u)
            result.diagnostics.emplace_back("unknown name must yield invalid index");
        if (lib.GetIndex("") != ~0u)
            result.diagnostics.emplace_back("empty name must yield invalid index");
        if (lib.GetIndex(nullptr) != ~0u)
            result.diagnostics.emplace_back("null name must yield invalid index");

        // 4. max_lod 统一 15.0（不再按纹理 mip 级数派生）。
        for (uint32_t i = 0; i < lib.GetCount(); ++i)
        {
            const VkSamplerCreateInfo *sci = lib.GetCreateInfo(i);
            if (!sci)
            {
                result.diagnostics.emplace_back(
                    "GetCreateInfo(" + std::to_string(i) + ") returned null");
                continue;
            }
            if (sci->maxLod != 15.0f)
            {
                result.diagnostics.emplace_back(
                    "preset " + std::to_string(i) + " maxLod != 15.0");
            }
        }

        // 5. 关键预设过滤语义。
        const VkSamplerCreateInfo *nearest = lib.GetCreateInfo(0);
        if (nearest
            && (nearest->magFilter != VK_FILTER_NEAREST
             || nearest->minFilter != VK_FILTER_NEAREST))
            result.diagnostics.emplace_back("Nearest filter mismatch");

        const VkSamplerCreateInfo *trilinear = lib.GetCreateInfo(2);
        if (trilinear
            && trilinear->mipmapMode != VK_SAMPLER_MIPMAP_MODE_LINEAR)
            result.diagnostics.emplace_back("Trilinear mipmap mode mismatch");

        const VkSamplerCreateInfo *aniso = lib.GetCreateInfo(3);
        if (aniso && aniso->anisotropyEnable != VK_TRUE)
            result.diagnostics.emplace_back("TrilinearAniso anisotropy mismatch");

        const VkSamplerCreateInfo *pcf = lib.GetCreateInfo(4);
        if (pcf
            && (pcf->compareEnable != VK_TRUE
             || pcf->addressModeU != VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE))
            result.diagnostics.emplace_back("ShadowPCF compare/clamp mismatch");

        // 6. sampler 宏注入（name→idx 与运行时 binding=1 数组下标一致）。
        const std::vector<std::string> names =
            { "Trilinear", "Linear", "Terrain" };
        const std::string macros = BuildSamplerMacros(names);
        const std::string expected_macros =
            "#define TrilinearSampler 2u\n"
            "#define LinearSampler 1u\n"
            "#define TerrainSampler 5u\n";
        if (macros != expected_macros)
        {
            result.diagnostics.emplace_back(
                "sampler macro injection mismatch: got [" + macros + "]");
        }

        // 7. 未知名 → 不生成宏（shader 编译显式失败暴露，不再静默保底 0）；
        //    空列表、空名跳过。
        if (!BuildSamplerMacros({ "UnknownSampler" }).empty())
            result.diagnostics.emplace_back("unknown sampler must produce no macro");
        if (!BuildSamplerMacros({}).empty())
            result.diagnostics.emplace_back("empty sampler list must produce no macros");
        if (!BuildSamplerMacros({ "" }).empty())
            result.diagnostics.emplace_back("empty sampler name must be skipped");

        result.passed = result.diagnostics.empty();
        return result;
    }

    static GateResult RunShaderLibraryPathCase()
    {
        GateResult result;
        result.name = "AB.shader-library-path";

        // Baseline: default resolution must find the repository ShaderLibrary.
        const std::string root = GetShaderLibraryPath("ShaderLibrary");
        const hgl::filesystem::Path root_path(hgl::ToOSString(root));
        if (!IsShaderLibraryRoot(root_path))
        {
            result.diagnostics.emplace_back(
                "ShaderLibrary path resolver did not find a valid material root");
        }

        const hgl::OSString env_name(kShaderLibraryPathEnvironmentVariable);
        const hgl::OSString env_root_os =
            hgl::ToOSString(RepoRootPath("ShaderLibrary"));

        // Capture the inherited value BEFORE any in-process mutation, so the
        // final restore puts the environment back exactly as the gate found it.
        const wchar_t *saved_env =
            _wgetenv(kShaderLibraryPathEnvironmentVariable);
        const hgl::OSString saved_env_os =
            saved_env && saved_env[0] ? hgl::OSString(saved_env) : hgl::OSString();

        // 1. Env-var override wins over an explicit (invalid) request.
        _wputenv((env_name + OS_TEXT("=") + env_root_os).c_str());
        const std::string env_override =
            GetShaderLibraryPath("C:/definitely/not/a/library");
        const hgl::filesystem::Path env_override_path(
            hgl::ToOSString(env_override));
        if (!IsShaderLibraryRoot(env_override_path))
        {
            result.diagnostics.emplace_back(
                "ULRE_SHADERLIBRARY_PATH override must beat the requested path");
        }

        // 2. Invalid env-var value must be ignored, not fatal.
        _wputenv((env_name + OS_TEXT("=C:/definitely/not/a/library")).c_str());
        const std::string invalid_env = GetShaderLibraryPath("ShaderLibrary");
        if (invalid_env.empty())
        {
            result.diagnostics.emplace_back(
                "invalid ULRE_SHADERLIBRARY_PATH must fall back, not fail");
        }

        // 3. Restoring the original value (or clearing, when unset) restores
        //    default resolution and must not poison later gate cases.
        if (!saved_env_os.IsEmpty())
            _wputenv((env_name + OS_TEXT("=") + saved_env_os).c_str());
        else
            _wputenv(env_name.c_str());
        const std::string cleared = GetShaderLibraryPath("ShaderLibrary");
        if (cleared.empty())
        {
            result.diagnostics.emplace_back(
                "restoring ULRE_SHADERLIBRARY_PATH must not break resolution");
        }

        result.passed = result.diagnostics.empty();
        return result;
    }

    static GateResult RunSPVReflectionCase()
    {
        GateResult result;
        result.name = "SPV.reflection-api";

        static constexpr char shader_source[] = R"GLSL(#version 460
layout(location = 0) in vec3 in_position;
layout(location = 1) out vec3 out_position;
layout(set = 0, binding = 7, std140) uniform ReflectionProbe
{
    vec4 tint;
} probe;
layout(push_constant) uniform ReflectionPush
{
    vec4 offset;
} push_data;

void main()
{
    vec3 position = in_position + probe.tint.xyz + push_data.offset.xyz;
    out_position = position;
    gl_Position = vec4(position, 1.0);
}
)GLSL";

        SPVData *spv = CompileShader(
            static_cast<uint32_t>(VK_SHADER_STAGE_VERTEX_BIT), shader_source);
        if (!spv)
        {
            result.diagnostics.emplace_back("failed to compile reflection probe GLSL");
            return result;
        }

        hgl::ValueArray<uint8_t> flat_storage;
        ShaderFlatView view{};
        const bool reflected = ReflectSPVData(
            spv,
            static_cast<uint32_t>(VK_SHADER_STAGE_VERTEX_BIT),
            "main",
            flat_storage,
            view);
        FreeSPVData(spv);

        if (!reflected)
        {
            result.diagnostics.emplace_back("ReflectSPVData failed");
            return result;
        }

        hgl::graph::mtl::ShaderReflectionData extracted;
        if (!extracted.Extract(view))
        {
            result.diagnostics.emplace_back("ShaderReflectionData extraction failed");
            return result;
        }
        flat_storage.Clear();

        if (extracted.shader_stage != static_cast<uint32_t>(VK_SHADER_STAGE_VERTEX_BIT)
         || std::strcmp(extracted.entrypoint.c_str(), "main") != 0)
            result.diagnostics.emplace_back("reflection stage or entrypoint mismatch");

        if (extracted.stage_inputs.GetCount() != 1
         || extracted.stage_inputs[0].location != 0
         || extracted.stage_inputs[0].basetype != SF_BASETYPE_FLOAT
         || extracted.stage_inputs[0].vec_size != 3)
            result.diagnostics.emplace_back("vertex input reflection mismatch");

        if (!extracted.FindDescriptor(
                SF_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0, 7))
            result.diagnostics.emplace_back("uniform-buffer set/binding reflection mismatch");

        bool found_output = false;
        for (int i = 0; i < extracted.stage_outputs.GetCount(); ++i)
        {
            if (extracted.stage_outputs[i].location == 1
             && extracted.stage_outputs[i].basetype == SF_BASETYPE_FLOAT
             && extracted.stage_outputs[i].vec_size == 3)
            {
                found_output = true;
                break;
            }
        }
        if (!found_output)
            result.diagnostics.emplace_back("vertex output reflection mismatch");

        uint32_t push_constant_end = 0;
        if (extracted.push_constants.GetCount() != 1
         || extracted.push_constants[0].offset != 0
         || !extracted.GetPushConstantEnd(push_constant_end)
         || push_constant_end < sizeof(float) * 4)
            result.diagnostics.emplace_back("push-constant reflection mismatch");

        result.passed = result.diagnostics.empty();
        return result;
    }

    static GateResult RunSPVBufferReferenceReflectionCase()
    {
        GateResult result;
        result.name = "SPV.buffer-reference-reflection";

        static constexpr char shader_source[] = R"GLSL(#version 460
#extension GL_EXT_buffer_reference : require
#extension GL_ARB_gpu_shader_int64 : require

layout(buffer_reference, scalar, buffer_reference_align = 16) buffer PayloadRow
{
    vec4 color;
};
layout(push_constant) uniform RootAddresses
{
    uint64_t addr_payload;
} pc_root;
layout(location = 0) out vec4 out_color;

void main()
{
    PayloadRow row = PayloadRow(pc_root.addr_payload);
    out_color = row.color;
    gl_Position = vec4(0.0, 0.0, 0.0, 1.0);
}
)GLSL";

        SPVData *spv = CompileShader(
            static_cast<uint32_t>(VK_SHADER_STAGE_VERTEX_BIT), shader_source);
        if (!spv)
        {
            result.diagnostics.emplace_back(
                "failed to compile buffer-reference reflection probe");
            return result;
        }

        hgl::ValueArray<uint8_t> flat_storage;
        ShaderFlatView view{};
        const bool reflected = ReflectSPVData(
            spv,
            static_cast<uint32_t>(VK_SHADER_STAGE_VERTEX_BIT),
            "main",
            flat_storage,
            view);
        FreeSPVData(spv);
        if (!reflected)
        {
            result.diagnostics.emplace_back(
                "buffer-reference SPIR-V reflection failed");
            return result;
        }

        hgl::graph::mtl::ShaderReflectionData extracted;
        if (!extracted.Extract(view))
        {
            result.diagnostics.emplace_back(
                "buffer-reference reflection extraction failed");
            return result;
        }

        for (uint32_t type = 0; type < SF_DESCRIPTOR_TYPE_COUNT; ++type)
        {
            if (!extracted.resources[type].IsEmpty())
            {
                result.diagnostics.emplace_back(
                    "buffer_reference was unexpectedly reported as a descriptor resource");
                break;
            }
        }

        uint32_t push_constant_end = 0;
        if (extracted.push_constants.GetCount() != 1
         || !extracted.GetPushConstantEnd(push_constant_end)
         || push_constant_end != sizeof(uint64_t))
            result.diagnostics.emplace_back(
                "buffer-reference address push-constant reflection mismatch");

        result.passed = result.diagnostics.empty();
        return result;
    }

        // ══════════════════════════════════════════════════════════════════════
        // MV.fixed-mesh-varying-invariants
        //
        // 原 MV.mesh-varying-writability（生成器可写性表测试）的终态替代：
        // 在完全物理固定的网格主着色器库（ShaderLibrary/mesh/shape/*.mesh）上，
        // 直接断言所有 varying 声明、调度契约、真实写入及 Stage2/3 组合不变量。
        // ══════════════════════════════════════════════════════════════════════
        static GateResult RunFixedMeshVaryingInvariantsCase()
        {
            GateResult result;
            result.name = "MV.fixed-mesh-varying-invariants";
            AuditFixedMeshMasters(result);
            result.passed = result.diagnostics.empty();
            return result;
        }

    static GateResult RunModuleInvariantsCase()
    {
        GateResult result;
        result.name = "MI.module-invariants";

        // 1. Real ShaderLibrary scan: names unique, stable IDs unique.
        ShaderCodeModuleRegistry registry;
        int file_count = 0;
        int error_count = 0;
        if (!registry.LoadDirectory(
                hgl::ToOSString(GetShaderLibraryPath()),
                &file_count, &error_count))
        {
            result.diagnostics.emplace_back(
                "module registry directory scan failed");
        }
        if (error_count != 0)
        {
            result.diagnostics.emplace_back(
                "module registry scan reported "
                + std::to_string(error_count) + " errors");
        }

        const int count = registry.GetCount();
        for (int i = 0; i < count; ++i)
        {
            const auto *left = registry.GetModuleByIndex(i);
            if (!left || !left->name || !left->name[0])
            {
                result.diagnostics.emplace_back(
                    "module at index " + std::to_string(i)
                    + " has no name");
                continue;
            }

            for (int j = i + 1; j < count; ++j)
            {
                const auto *right = registry.GetModuleByIndex(j);
                if (!right || !right->name)
                    continue;

                if (std::strcmp(left->name, right->name) == 0)
                {
                    result.diagnostics.emplace_back(
                        "duplicate module name: "
                        + std::string(left->name));
                }
            }
        }

        result.passed = result.diagnostics.empty();
        return result;
    }

    // ══════════════════════════════════════════════════════════════════════
    // MI.sampler-reference-contract
    //
    // 模块正文里的 <Name>Sampler 宏只由材质 TOML 的 [resources].samplers 生成
    // （BuildSamplerMacros）。模块必须用
    //   // @ulre sampler_reference <PresetName> [required|optional]
    // 自证依赖，否则"换了 provider / 材质漏配 sampler"只会在 GLSL 编译期以
    // 未定义宏的形式炸出来，且索引还可能被硬编码兜底掩盖成静默错值。
    // 本用例锁定两侧：正文出现的 *Sampler 必须被声明；声明必须是真实预设。
    // ══════════════════════════════════════════════════════════════════════
    static GateResult RunSamplerReferenceContractCase()
    {
        GateResult result;
        result.name = "MI.sampler-reference-contract";

        ShaderCodeModuleRegistry registry;
        int file_count = 0;
        int error_count = 0;
        if (!registry.LoadDirectory(
                hgl::ToOSString(GetShaderLibraryPath()),
                &file_count, &error_count))
        {
            result.diagnostics.emplace_back(
                "module registry directory scan failed");
            result.passed = false;
            return result;
        }
        if (error_count != 0)
        {
            result.diagnostics.emplace_back(
                "module registry scan reported "
                + std::to_string(error_count) + " errors");
        }

        auto &lib = SamplerPresetLibrary::Instance();
        if (lib.GetCount() == 0
         && !lib.Load(hgl::ToOSString(GetShaderLibraryPath() + "/sampler.toml")))
        {
            result.diagnostics.emplace_back("sampler.toml load failed");
            result.passed = false;
            return result;
        }

        const auto declares = [](const ShaderCodeModuleDefinition &definition,
                                 const char *preset)
        {
            for (uint32_t i = 0;
                 i < definition.sampler_reference_requirement_count;
                 ++i)
            {
                const auto &requirement =
                    definition.sampler_reference_requirements[i];
                if (requirement.sampler_name
                 && std::strcmp(requirement.sampler_name, preset) == 0)
                    return true;
            }
            return false;
        };

        const auto is_identifier_char = [](const char c)
        {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
                || (c >= '0' && c <= '9') || c == '_';
        };

        for (int i = 0; i < registry.GetCount(); ++i)
        {
            const auto *definition = registry.GetModuleByIndex(i);
            if (!definition || !definition->name || !definition->glsl_code)
                continue;

            // 1. 声明的预设必须真实存在于 sampler.toml。
            for (uint32_t j = 0;
                 j < definition->sampler_reference_requirement_count;
                 ++j)
            {
                const char *name =
                    definition->sampler_reference_requirements[j].sampler_name;
                if (!name || lib.GetIndex(name) == ~0u)
                {
                    result.diagnostics.emplace_back(
                        std::string("module ") + definition->name
                        + " declares unknown sampler preset: "
                        + (name ? name : "<null>"));
                }
            }

            // 2. 正文中出现的每个 <Name>Sampler 都必须自我声明（跳过注释行）。
            const std::string text(definition->glsl_code);
            size_t pos = 0;
            while (pos < text.size())
            {
                const size_t eol = text.find('\n', pos);
                const size_t line_end =
                    eol == std::string::npos ? text.size() : eol;

                size_t first = pos;
                while (first < line_end
                    && (text[first] == ' ' || text[first] == '\t'))
                    ++first;

                const bool is_comment =
                    first + 1 < line_end
                 && text[first] == '/' && text[first + 1] == '/';

                size_t cursor = pos;
                while (cursor < line_end)
                {
                    if (!is_identifier_char(text[cursor]))
                    {
                        ++cursor;
                        continue;
                    }

                    size_t ident_end = cursor;
                    while (ident_end < line_end
                        && is_identifier_char(text[ident_end]))
                        ++ident_end;

                    constexpr size_t kSamplerSuffixLength = 7; // "Sampler"
                    const size_t length = ident_end - cursor;
                    if (!is_comment
                     && length > kSamplerSuffixLength
                     && text.compare(ident_end - kSamplerSuffixLength,
                                     kSamplerSuffixLength, "Sampler") == 0)
                    {
                        const std::string preset =
                            text.substr(cursor, length - kSamplerSuffixLength);
                        if (!declares(*definition, preset.c_str()))
                        {
                            result.diagnostics.emplace_back(
                                "module " + std::string(definition->name)
                                + " uses " + preset + "Sampler without "
                                  "@ulre sampler_reference " + preset);
                        }
                    }
                    cursor = ident_end;
                }

                pos = line_end + 1;
            }
        }

        // 3. 声明→材质校验器：required 缺失必须失败并报出名字，optional 放行。
        {
            ShaderCodeResourceManifest manifest;
            ShaderCodeModuleSamplerReferenceRequirement required_sampler;
            required_sampler.sampler_name = "Trilinear";
            required_sampler.required = true;
            if (manifest.sampler_references.Add(required_sampler) < 0)
                result.diagnostics.emplace_back(
                    "manifest sampler reference add failed");
            manifest.sampler_reference_count =
                static_cast<uint32_t>(manifest.sampler_references.GetCount());

            MaterialDefinition material;
            const char *missing = nullptr;
            if (ValidateShaderCodeResourceManifestSamplerReferences(
                    manifest, material, &missing))
            {
                result.diagnostics.emplace_back(
                    "undeclared required sampler must fail validation");
            }
            else if (!missing || std::strcmp(missing, "Trilinear") != 0)
            {
                result.diagnostics.emplace_back(
                    "undeclared required sampler diagnostic name mismatch");
            }

            material.sampler_names.push_back("Trilinear");
            if (!ValidateShaderCodeResourceManifestSamplerReferences(
                    manifest, material))
            {
                result.diagnostics.emplace_back(
                    "declared required sampler must pass validation");
            }

            manifest.sampler_references[0].required = false;
            MaterialDefinition empty_material;
            if (!ValidateShaderCodeResourceManifestSamplerReferences(
                    manifest, empty_material))
            {
                result.diagnostics.emplace_back(
                    "optional sampler must not require a material declaration");
            }
        }

        result.passed = result.diagnostics.empty();
        return result;
    }

    // ══════════════════════════════════════════════════════════════════════
    // MI.module-include-declaration-contract
    //
    // `@ulre uses` 是模块依赖的唯一真源：manifest 模块图、能力闭包、
    // 资源/采样器契约与模块冲突检查只沿 uses 边展开。而真正被编译进
    // 程序的是正文手写的 `#include`。两侧一旦不一致，include 进来的
    // 模块就整条从图里消失——它的 provides/require 不计入能力闭合、
    // 它声明的 texture/sampler 引用不被校验、它与其他模块的冲突不被
    // 检测（P4：provider 靠 include 隐式拉入依赖模块，正是这种隐形边）。
    //
    // 本用例锁定三件事：
    //   1. 扫描器只认行首 `#include`（注释掉的 include 不算依赖）；
    //   2. 全库加载零错误（Registry 已把「include 未声明」判为硬错误）；
    //   3. 关键隐式边必须在 dependencies 里可见，且真的进入 manifest 图。
    // ══════════════════════════════════════════════════════════════════════
    static GateResult RunModuleIncludeDeclarationContractCase()
    {
        GateResult result;
        result.name = "MI.module-include-declaration-contract";

        // 1. 扫描器：行首 include 才计入，注释行/行尾不算。
        {
            static constexpr char kProbe[] =
                "// #include \"common/commented_out.glsl\"\n"
                "/* #include \"common/block_comment.glsl\" */\n"
                "#include \"ubo/scene_ubo.glsl\"\n"
                "  #include <common/descriptor_macros.glsl>\n"
                "float x; // #include \"common/trailing.glsl\"\n";

            hgl::AnsiStringList paths;
            const int collected = CollectShaderCodeModuleIncludePaths(
                kProbe, static_cast<int>(sizeof(kProbe) - 1), paths);

            if (collected != 2 || paths.GetCount() != 2)
                result.diagnostics.emplace_back(
                    "include scanner must ignore commented includes (expected 2, got "
                    + std::to_string(collected) + ")");
            else if (!(paths[0] == "ubo/scene_ubo.glsl")
                  || !(paths[1] == "common/descriptor_macros.glsl"))
                result.diagnostics.emplace_back(
                    "include scanner collected unexpected paths");
        }

        ShaderCodeModuleRegistry registry;
        int file_count = 0;
        int error_count = 0;
        if (!registry.LoadDirectory(
                hgl::ToOSString(GetShaderLibraryPath()),
                &file_count, &error_count))
        {
            result.diagnostics.emplace_back(
                "module registry directory scan failed");
            result.passed = false;
            return result;
        }

        // 2. Registry 加载期已强制 include ⊆ uses，任何违例都是硬错误。
        if (error_count != 0)
            result.diagnostics.emplace_back(
                "module registry scan reported "
                + std::to_string(error_count)
                + " errors (undeclared #include dependency?)");

        const auto depends_on = [&registry](const char *module_name,
                                            const char *dependency_name)
        {
            const auto *definition = registry.FindByName(module_name);
            if (!definition)
                return false;

            for (uint32_t i = 0; i < definition->dependency_count; ++i)
            {
                const char *name = definition->dependencies[i].module_name;
                if (name && std::strcmp(name, dependency_name) == 0)
                    return true;
            }
            return false;
        };

        // 3. P4 的核心隐式边：provider 正文 include 拉入的模块必须显式声明。
        struct ExpectedEdge
        {
            const char *module_name;
            const char *dependency_name;
            const char *why;
        };

        static const ExpectedEdge kExpectedEdges[] =
        {
            { "indirect_sky_ambient", "sky_atmosphere",
              "sky ambient provider pulls the atmosphere model in by #include" },
            { "sky_minimal_surface", "sky_atmosphere",
              "sky surface provider pulls the atmosphere model in by #include" },
            { "sky_atmosphere", "scene_ubo",
              "atmosphere reads main light / sky rows from the global tables" },
            { "indirect_ibl", "bindless_textures",
              "IBL provider samples env maps through bindless textures" },
            { "material_surface", "ntb_interface",
              "surface provider branches on the NTB provider contract" },
            { "ntb_tangent_vbo_normalmap", "ntb_orthonormal",
              "NTB provider falls back to ntb_orthonormal on UV-degenerate frames" },
            { "surface_interface", "descriptor_macros",
              "surface interface depends on the standard descriptor macros" },
            { "scene_ubo", "descriptor_macros",
              "scene data declarations depend on the standard descriptor macros" },
            { "entity_gpu_cull", "entity_gpu",
              "GPU cull reads the entity row mirror" },
            { "entity_gpu_cull", "RenderItemResolve",
              "GPU cull resolves render items through RenderItemResolve" },
        };

        for (const ExpectedEdge &edge : kExpectedEdges)
        {
            if (registry.FindByName(edge.module_name) == nullptr)
            {
                result.diagnostics.emplace_back(
                    std::string("module missing from registry: ") + edge.module_name);
                continue;
            }

            if (!depends_on(edge.module_name, edge.dependency_name))
                result.diagnostics.emplace_back(
                    std::string("module ") + edge.module_name
                    + " must declare @ulre uses " + edge.dependency_name
                    + " (" + edge.why + ")");
        }

        // 4. 声明必须真的进图：以 sky_minimal_surface 为根构建 manifest，
        //    sky_atmosphere 必须出现在 code_module_names 里（P4 之前它是
        //    被 include 编进去、却在图里完全不可见的模块）。
        {
            const char *roots[] = { "sky_minimal_surface" };
            ShaderCodeResourceManifest manifest{};
            if (!BuildShaderCodeResourceManifest(
                    roots, 1, manifest, &registry))
            {
                result.diagnostics.emplace_back(
                    std::string("sky_minimal_surface manifest failed: ")
                    + GetShaderCodeResourceManifestErrorName(manifest.error));
            }
            else
            {
                bool found_atmosphere = false;
                bool found_scene_ubo = false;
                for (uint32_t i = 0; i < manifest.code_module_count; ++i)
                {
                    const char *name = manifest.code_module_names[i];
                    if (!name)
                        continue;
                    if (std::strcmp(name, "sky_atmosphere") == 0)
                        found_atmosphere = true;
                    else if (std::strcmp(name, "scene_ubo") == 0)
                        found_scene_ubo = true;
                }

                if (!found_atmosphere)
                    result.diagnostics.emplace_back(
                        "manifest closure of sky_minimal_surface must contain "
                        "sky_atmosphere (forced #include must be a declared edge)");
                if (!found_scene_ubo)
                    result.diagnostics.emplace_back(
                        "manifest closure of sky_minimal_surface must contain scene_ubo");
            }
        }

        result.passed = result.diagnostics.empty();
        return result;
    }

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

    /// 发行形态清单门
    ///
    /// 发行版没有生成器、没有 GLSL 源码，只能靠 `program_manifest.pmf` 把「运行时
    /// 自算的语义键」映射到 program link。本门守三件事：
    ///   1) 磁盘往返逐字段等值（序列化/反序列化不许丢字段、不许张冠李戴）；
    ///   2) `ComputeMaterialProgramManifestKey` 对每个决定 SPV 内容的维度都敏感
    ///      —— 任一维度漏折，发行形态就会取到别人的 SPV，而且是**静默**的；
    ///   3) 损坏的清单必须整体拒绝，绝不"尽力装载半份"。
    static GateResult RunProgramManifestCase()
    {
        GateResult result;
        result.name = "PM.program-manifest-roundtrip";

        // ── 1. 磁盘往返 ──
        ShaderProgramManifest source;
        constexpr hgl::uint64 kProfileHash = 0x1234'5678'9ABC'DEF0ull;
        source.SetStamps(kProfileHash, contract::SHADERGEN_CODEGEN_GENERATION);

        for (hgl::uint32 i = 1; i <= 4; ++i)
        {
            ShaderProgramManifestEntry entry{};
            entry.lookup_key = 0x1000'0000ull + i;
            entry.program_link.mesh_stage.stage = ShaderStage::Mesh;
            entry.program_link.mesh_stage.definition_hash = 0x11ull * i;
            entry.program_link.mesh_stage.compile_inputs_hash = 0x22ull * i;
            entry.program_link.mesh_stage.interface_hash = 0x33ull * i;
            entry.program_link.mesh_stage.compiler_hash = 0x55ull * i;
            entry.program_link.fragment_stage.stage = ShaderStage::Fragment;
            entry.program_link.fragment_stage.definition_hash = 0x66ull * i;
            entry.program_link.fragment_stage.compile_inputs_hash = 0x77ull * i;
            entry.program_link.fragment_stage.interface_hash = 0x88ull * i;
            entry.program_link.fragment_stage.compiler_hash = 0xAAull * i;
            entry.program_link.link_contract_hash = 0xDDull * i;
            // 行需求的两个信号必须能各自独立往返：三个合法组合来自
            // 「payload ⊂ rows」这条不变量 ——
            //   (rows=F,payload=F) 无行无 payload（VertexColor 一类）
            //   (rows=T,payload=F) 只要纹理引用行表（UnlitTexture 一类）
            //   (rows=T,payload=T) 读 payload 行（Lit 一类）
            // (rows=F,payload=T) 非法，由下面的拒绝断言单独覆盖，不放进往返样本。
            // 下标 0 是占位（下面按 1..4 取样本），数组长度必须是 5 —— 长度 4 时
            // `[4]` 会读到数组外，静态存储的越界读会随链接布局改变，表现为
            // 「这条断言时好时坏」而不是编译错误。
            static const bool kRuntimeRows[5] = { false, false, true, true, true };
            static const bool kPayloadRows[5] = { false, false, false, true, false };
            entry.requires_recipe_runtime_rows = kRuntimeRows[i];
            entry.requires_recipe_payload_row = kPayloadRows[i];

            if (!source.Upsert(entry))
            {
                result.diagnostics.emplace_back(
                    "Upsert rejected a valid manifest entry");
                break;
            }

            // 幂等：同一份内容重复写入不得报"有变化"。
            if (source.Upsert(entry))
                result.diagnostics.emplace_back(
                    "Upsert reported a change for identical content");
        }

        if (source.GetEntryCount() != 4)
            result.diagnostics.emplace_back(
                "manifest entry count mismatch after upsert");

        // ── 行需求不变量：payload ⊂ rows ──
        //
        // 「要 payload 行但不要行表」不可能成立（payload_index 来自行表）。放行会
        // 变成「程序读 payload，运行期从不物化行」——运行期读到越界/陈旧 SSBO，
        // 而不是一条明确失败。三个合法组合已在上面往返过，这里守非法的那一个。
        {
            ShaderProgramManifestEntry illegal{};
            illegal.lookup_key = 0x2000'0000ull;
            illegal.program_link.mesh_stage.stage = ShaderStage::Mesh;
            illegal.program_link.mesh_stage.compile_inputs_hash = 0x11ull;
            illegal.program_link.mesh_stage.interface_hash = 0x22ull;
            illegal.program_link.mesh_stage.compiler_hash = 0x44ull;
            illegal.program_link.fragment_stage.stage = ShaderStage::Fragment;
            illegal.program_link.fragment_stage.compile_inputs_hash = 0x55ull;
            illegal.program_link.fragment_stage.interface_hash = 0x66ull;
            illegal.program_link.fragment_stage.compiler_hash = 0x88ull;
            illegal.program_link.link_contract_hash = 0xBBull;
            illegal.requires_recipe_runtime_rows = false;
            illegal.requires_recipe_payload_row = true;

            if (illegal.IsConsistent())
                result.diagnostics.emplace_back(
                    "manifest entry invariant: payload row without runtime rows "
                    "must be reported inconsistent");

            ShaderProgramManifest reject_source;
            reject_source.SetStamps(kProfileHash,
                                    contract::SHADERGEN_CODEGEN_GENERATION);
            if (reject_source.Upsert(illegal))
                result.diagnostics.emplace_back(
                    "Upsert accepted a payload-row-without-runtime-rows entry");
        }

        const std::filesystem::path temp_root =
            std::filesystem::temp_directory_path() / "ulre-program-manifest-gate";
        std::error_code ec;
        std::filesystem::remove_all(temp_root, ec);

        const hgl::OSString manifest_path =
            MakeShaderProgramManifestPath(hgl::OSString(temp_root.wstring().c_str()));

        if (!source.Save(manifest_path))
        {
            result.diagnostics.emplace_back("manifest Save failed");
            result.passed = false;
            return result;
        }

        ShaderProgramManifest loaded;
        if (!loaded.Load(manifest_path))
        {
            result.diagnostics.emplace_back("manifest Load failed on freshly saved file");
            result.passed = false;
            return result;
        }

        // ── Load 侧的行需求不变量守卫 ──
        //
        // 合法清单落盘后把第 0 条（rows=F,payload=F）的 flags 改成 (rows=F,payload=T)：
        // 这是「要 payload 行却不要行表」的非法组合，必须整份拒绝。校验和不符会先
        // 一步拦下，所以篡改后必须重算 payload_hash —— 否则测到的是校验和守卫。
        {
            hgl::uint8 *raw = nullptr;
            const hgl::int64 raw_size = hgl::filesystem::LoadFileToMemory(
                manifest_path, reinterpret_cast<void **>(&raw));
            const hgl::uint64 payload_offset =
                static_cast<hgl::uint64>(sizeof(ShaderArtifactFileHeader));
            // payload = version(u32) + count(u32) + entry*N；flags 是每条 entry 的末位 u32。
            const hgl::uint64 first_flags_offset = payload_offset
                + static_cast<hgl::uint64>(sizeof(hgl::uint32) * 2)
                + static_cast<hgl::uint64>(ShaderProgramManifestEntryPayloadSize)
                - static_cast<hgl::uint64>(sizeof(hgl::uint32));

            if (raw_size < 0 || !raw
             || static_cast<hgl::uint64>(raw_size) < first_flags_offset + sizeof(hgl::uint32))
            {
                if (raw)
                    delete[] raw;
                result.diagnostics.emplace_back(
                    "manifest fixture unreadable for load-time invariant test");
            }
            else
            {
                const hgl::uint64 payload_size =
                    static_cast<hgl::uint64>(raw_size) - payload_offset;

                hgl::uint32 tampered_flags = 0;
                std::memcpy(&tampered_flags, raw + first_flags_offset,
                            sizeof(tampered_flags));
                tampered_flags |= ShaderProgramManifestFlagRequiresRecipePayloadRow;
                tampered_flags &= ~ShaderProgramManifestFlagRequiresRecipeRows;
                std::memcpy(raw + first_flags_offset, &tampered_flags,
                            sizeof(tampered_flags));

                hgl::hash::FNV1aHasher64 hasher;
                hasher.AppendBytes(raw + payload_offset,
                                   static_cast<size_t>(payload_size));
                const hgl::uint64 refreshed_hash = hasher;
                std::memcpy(raw + offsetof(ShaderArtifactFileHeader, payload_hash),
                            &refreshed_hash, sizeof(refreshed_hash));

                const hgl::int64 written = hgl::filesystem::SaveMemoryToFile(
                    manifest_path, raw, raw_size);
                delete[] raw;

                if (written != raw_size)
                    result.diagnostics.emplace_back(
                        "failed to write tampered manifest fixture");
                else
                {
                    ShaderProgramManifest tampered;
                    if (tampered.Load(manifest_path))
                        result.diagnostics.emplace_back(
                            "Load accepted a manifest carrying "
                            "payload-row-without-runtime-rows");
                }
            }
        }

        if (loaded.GetCompilerProfileHash() != kProfileHash
         || loaded.GetCodegenGeneration() != contract::SHADERGEN_CODEGEN_GENERATION
         || loaded.GetEntryCount() != source.GetEntryCount())
            result.diagnostics.emplace_back("manifest stamps/entry count lost in roundtrip");

        for (hgl::uint32 i = 1; i <= 4; ++i)
        {
            const ShaderProgramManifestEntry *original =
                source.Find(0x1000'0000ull + i);
            const ShaderProgramManifestEntry *restored =
                loaded.Find(0x1000'0000ull + i);

            if (!original || !restored)
            {
                result.diagnostics.emplace_back(
                    "manifest entry vanished in roundtrip");
                continue;
            }

            if (*original != *restored)
                result.diagnostics.emplace_back(
                    "manifest entry differs after roundtrip");
        }

        // ── 2. 语义键敏感性 ──
        //
        // 每个维度都必须独立地把键推开；反之（两个不同变体算出同一个键）就会让
        // 发行形态取到不匹配的 SPV —— 这种错误在运行时完全不报错。
        {
            MaterialDefinition definition{};
            if (!TryGetMaterialDefinitionByID("Lit", definition))
                result.diagnostics.emplace_back(
                    "material definition unavailable: Lit");
            else
            {
                const GeometryVertexFormat geometry{
                    {VertexSemantic::Position, VF_V3F},
                    {VertexSemantic::TexCoord, VF_V2F},
                    {VertexSemantic::Normal,   VF_V3F}
                };
                const GeometryVertexFormat other_geometry{
                    {VertexSemantic::Position, VF_V3F},
                    {VertexSemantic::TexCoord, VF_V2F}
                };

                MaterialDefinitionBuildRequest base{};
                base.recipe.mtl_def_id = definition.definition_id;
                NormalizeRecipe(base.recipe);
                base.geometry_vertex_format = &geometry;
                base.defer_finalize = true;
                base.shader_program_purpose = ShaderProgramPurpose::ForwardColor;

                const hgl::uint64 base_key =
                    ComputeMaterialProgramManifestKey(definition, base, nullptr);

                if (base_key == 0)
                    result.diagnostics.emplace_back(
                        "manifest key is zero for a valid request");

                if (ComputeMaterialProgramManifestKey(definition, base, nullptr)
                    != base_key)
                    result.diagnostics.emplace_back(
                        "manifest key is not deterministic across calls");

                MaterialDefinitionBuildRequest purpose = base;
                purpose.shader_program_purpose = ShaderProgramPurpose::DepthOnly;
                if (ComputeMaterialProgramManifestKey(definition, purpose, nullptr)
                    == base_key)
                    result.diagnostics.emplace_back(
                        "manifest key ignores shader_program_purpose");

                MaterialDefinitionBuildRequest ambient = base;
                ambient.ambient_module_name = "AmbientVertexGlobalColor";
                if (ComputeMaterialProgramManifestKey(definition, ambient, nullptr)
                    == base_key)
                    result.diagnostics.emplace_back(
                        "manifest key ignores ambient_module_name");

                MaterialDefinitionBuildRequest vertex_format = base;
                vertex_format.geometry_vertex_format = &other_geometry;
                if (ComputeMaterialProgramManifestKey(definition, vertex_format, nullptr)
                    == base_key)
                    result.diagnostics.emplace_back(
                        "manifest key ignores geometry_vertex_format");

                MaterialDefinitionBuildRequest recipe = base;
                recipe.recipe.recipe_name = "gate-variant";

                // ── 变体维度：必须把键推开 ──
                //
                // 只保留**真正改变 GLSL** 的字段。反过来（两个不同变体算出同一个
                // 键）会让发行形态取到不匹配的 SPV，且运行时完全不报错。
                //
                // 旧的断言用 `recipe_name` 当「配方」的代表 —— 那是把**实例**
                // 身份当变体身份，正是缺陷 #1/#3 本身：recipe_name 变了不会重新
                // 发射 GLSL，键却变了 ⇒ 烘焙期只填一条、发行期查另一条即 miss。
                // 现在拆成下面两组正/负断言。
                {
                    MaterialDefinitionBuildRequest alpha_test = base;
                    alpha_test.recipe.render_state_overrides.has_alpha_test = true;
                    alpha_test.recipe.render_state_overrides.alpha_test = true;
                    if (ComputeMaterialProgramManifestKey(definition, alpha_test, nullptr)
                        == base_key)
                        result.diagnostics.emplace_back(
                            "manifest key ignores alpha_test coverage");

                    MaterialDefinitionBuildRequest dither = base;
                    dither.recipe.render_state_overrides.has_dither = true;
                    dither.recipe.render_state_overrides.dither = true;
                    if (ComputeMaterialProgramManifestKey(definition, dither, nullptr)
                        == base_key)
                        result.diagnostics.emplace_back(
                            "manifest key ignores dither coverage");

                    MaterialDefinitionBuildRequest alpha_cutoff = base;
                    alpha_cutoff.recipe.render_state_overrides.has_alpha_cutoff = true;
                    alpha_cutoff.recipe.render_state_overrides.alpha_cutoff =
                        base.recipe.render_state_overrides.alpha_cutoff + 0.125f;
                    if (ComputeMaterialProgramManifestKey(
                            definition, alpha_cutoff, nullptr) == base_key)
                        result.diagnostics.emplace_back(
                            "manifest key ignores alpha_cutoff (baked as a GLSL literal)");

                    MaterialDefinitionBuildRequest alpha_blend = base;
                    alpha_blend.recipe.render_state_overrides.has_pipeline_config = true;
                    alpha_blend.recipe.render_state_overrides.pipeline_config.alpha_blend = true;
                    if (ComputeMaterialProgramManifestKey(
                            definition, alpha_blend, nullptr) == base_key)
                        result.diagnostics.emplace_back(
                            "manifest key ignores alpha_blend (changes fragment output)");

                    MaterialDefinitionBuildRequest vertex_nodes = base;
                    vertex_nodes.recipe.vertex_node_config =
                        Make2DNodeConfigNDC(true);
                    if (ComputeMaterialProgramManifestKey(
                            definition, vertex_nodes, nullptr) == base_key)
                        result.diagnostics.emplace_back(
                            "manifest key ignores vertex_node_config");
                }

                // ── 实例维度：必须**不**改变键 ──
                //
                // 这些字段改的是描述符绑定、运行时材质行或 VkPipeline，不是 SPV。
                // 若被折进变体键，同一条 program link 会在烘焙清单里分裂成多条
                // lookup_key，烘焙期触碰到哪条由遍历顺序决定 ⇒ 发行期查另一条
                // 必然 miss（缺陷 #1/#3 的根因）。
                {
                    if (ComputeMaterialProgramManifestKey(definition, recipe, nullptr)
                        != base_key)
                        result.diagnostics.emplace_back(
                            "manifest key must not depend on recipe_name (instance identity)");

                    MaterialDefinitionBuildRequest texture = base;
                    UpsertRecipeTextureBinding(
                        texture.recipe, "base_color", "gate/texture-a", true);
                    if (ComputeMaterialProgramManifestKey(definition, texture, nullptr)
                        != base_key)
                        result.diagnostics.emplace_back(
                            "manifest key must not depend on texture bindings (instance identity)");

                    MaterialDefinitionBuildRequest ssbo = base;
                    ssbo.recipe.material_ssbo_binding.ssbo_type = GlobalSSBOType::PBRSurface;
                    ssbo.recipe.material_ssbo_binding.ssbo_id = 7;
                    ssbo.recipe.material_ssbo_binding.data_index = 3;
                    if (ComputeMaterialProgramManifestKey(definition, ssbo, nullptr)
                        != base_key)
                        result.diagnostics.emplace_back(
                            "manifest key must not depend on material_ssbo_binding (instance identity)");

                    MaterialDefinitionBuildRequest cull = base;
                    cull.recipe.render_state_overrides.has_pipeline_config = true;
                    cull.recipe.render_state_overrides.pipeline_config.cull_mode =
                        VK_CULL_MODE_NONE;
                    if (ComputeMaterialProgramManifestKey(definition, cull, nullptr)
                        != base_key)
                        result.diagnostics.emplace_back(
                            "manifest key must not depend on pipeline-only state (cull_mode)");

                    MaterialDefinitionBuildRequest fallback_color = base;
                    fallback_color.recipe.fallback_marker_color[0] = 0.25f;
                    if (ComputeMaterialProgramManifestKey(
                            definition, fallback_color, nullptr) != base_key)
                        result.diagnostics.emplace_back(
                            "manifest key must not depend on fallback_marker_color "
                            "(delivered through the material data row)");
                }

                MaterialDefinitionBuildRequest primitive = base;
                primitive.primitive_type =
                    primitive.primitive_type == PrimitiveType::Triangles
                        ? PrimitiveType::Lines
                        : PrimitiveType::Triangles;
                if (ComputeMaterialProgramManifestKey(definition, primitive, nullptr)
                    == base_key)
                    result.diagnostics.emplace_back(
                        "manifest key ignores primitive_type");
            }
        }

        // ── 3. 损坏清单必须整体拒绝 ──
        {
            const std::wstring manifest_file = manifest_path.c_str();

            std::string bytes;
            if (!ReadGoldenFile(
                    std::filesystem::path(manifest_file).string(), bytes))
                result.diagnostics.emplace_back(
                    "cannot re-read saved manifest for corruption test");
            else
            {
                // 改最后一个字节：落在最后一条 payload 里，应被校验和发现。
                bytes.back() = static_cast<char>(bytes.back() ^ 0x5A);

                const std::wstring corrupt_root =
                    (temp_root / "corrupt").wstring();
                const hgl::OSString corrupt_path =
                    MakeShaderProgramManifestPath(
                        hgl::OSString(corrupt_root.c_str()));

                if (!WriteTextFile(
                        std::filesystem::path(corrupt_root).string(), bytes))
                    result.diagnostics.emplace_back(
                        "cannot write corrupted manifest");
                else
                {
                    ShaderProgramManifest corrupt;
                    if (corrupt.Load(corrupt_path) || !corrupt.IsEmpty())
                        result.diagnostics.emplace_back(
                            "corrupted manifest was accepted");
                }
            }
        }

        std::filesystem::remove_all(temp_root, ec);

        result.passed = result.diagnostics.empty();
        return result;
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
        const MaterialDefinition &definition,
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
        out << "definition_id=" << definition.definition_id.c_str() << "\n"
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

    /// keep_artifacts=true 时不清空 store_root，并把本次烘焙的期望值（含清单与两
    /// stage SPV 的完整校验值）留在磁盘上 —— 供发行形态探针（RuntimeFormProbe，
    /// ULRE_ENABLE_SHADERGEN=0 构建）跨进程消费，证明「生成器烘焙 → 无生成器运行时
    /// 查表」这条链路成立。
    static GateResult RunProgramManifestLookupCase(
        const std::filesystem::path &store_root,
        const bool keep_artifacts = false)
    {
        GateResult result;
        result.name = keep_artifacts
            ? "PM.emit-release-cache"
            : "PM.release-manifest-lookup";

        MaterialDefinition definition{};
        if (!TryGetMaterialDefinitionByID("Lit", definition))
        {
            result.diagnostics.emplace_back(
                "material definition unavailable: Lit");
            result.passed = false;
            return result;
        }

        const GeometryVertexFormat geometry{
            {VertexSemantic::Position, VF_V3F},
            {VertexSemantic::TexCoord, VF_V2F},
            {VertexSemantic::Normal,   VF_V3F}
        };

        MaterialDefinitionBuildRequest request{};
        request.recipe.mtl_def_id = definition.definition_id;
        NormalizeRecipe(request.recipe);
        request.geometry_vertex_format = &geometry;
        request.defer_finalize = true;
        request.shader_program_purpose = ShaderProgramPurpose::ForwardColor;
        RenderTemplateValidationDiagnostic template_diagnostic{};
        if (!ResolveSceneRenderTemplateRequest(
                RenderTemplateID::ForwardLitShadowedAO,
                hgl::graph::ShaderStage::Fragment,
                MakeIdentityForwardLitProfile(),
                request.render_template_request, template_diagnostic)
         || !AppendMaterialRenderTemplateRoots(
                definition, request.render_template_request))
        {
            result.diagnostics.emplace_back(
                "render template resolution failed for Lit");
            result.passed = false;
            return result;
        }

        std::unique_ptr<ShaderBuildContext> baked;
        {
            // 片段源走领域固定源：本用例算的是清单/缓存身份，与片段怎么写无关。
            FixedFragmentSource fixed;
            if (!ResolveFixedFragmentSource(definition, request, fixed))
            {
                result.diagnostics.emplace_back(
                    std::string("fixed fragment source unavailable: ")
                    + GetLastDomainShaderBuildBackendError());
                result.passed = false;
                return result;
            }
            FixedStageInputs fixed_stage;
            fixed_stage.fragment_source = fixed.source.c_str();
            fixed_stage.fragment_interface_hash = fixed.interface_hash;
            fixed_stage.fragment_options_digest = fixed.options_digest;
            baked.reset(
                AssembleMaterialProgram(nullptr, request, definition, nullptr, fixed_stage));
        }
        if (!baked || !baked->HasProgramLink() || !baked->HasProgramArtifactMetadata())
        {
            result.diagnostics.emplace_back(
                "generator-side build failed to produce a program identity");
            result.passed = false;
            return result;
        }

        std::error_code ec;
        if (!keep_artifacts)
            std::filesystem::remove_all(store_root, ec);

        ShaderArtifactStore store(
            hgl::OSString(store_root.wstring().c_str()),
            ShaderCacheMode::BuildIfMissing);

        // 参数化成对的假 SPIR-V：只要魔数正确、长度 4 字节对齐，装载侧就应当收下。
        const uint32_t fake_mesh_spv[] =
            {ShaderArtifactSPVMagic, 0x4101u, 0x4102u, 0x4103u};
        const uint32_t fake_fragment_spv[] =
            {ShaderArtifactSPVMagic, 0x4201u, 0x4202u, 0x4203u};

        const ShaderLinkSpec &link = baked->GetProgramLink();
        const ShaderProgramArtifactMetadata metadata =
            baked->GetProgramArtifactMetadata();

        if (!store.SaveStageSPV(
                link.mesh_stage, fake_mesh_spv, sizeof(fake_mesh_spv), 0)
         || !store.SaveStageSPV(
                link.fragment_stage, fake_fragment_spv,
                sizeof(fake_fragment_spv), 0)
         || !store.SaveProgramMetadata(link, metadata))
        {
            result.diagnostics.emplace_back(
                "failed to write program artifacts into the baking store");
            result.passed = false;
            return result;
        }

        ShaderProgramManifestEntry entry{};
        entry.lookup_key =
            ComputeMaterialProgramManifestKey(definition, request, nullptr);
        entry.program_link = link;
        entry.requires_recipe_runtime_rows = baked->RequiresRecipeRuntimeRows();

        ShaderProgramBuildBackend *previous_backend =
            GetShaderProgramBuildBackend();
        SetShaderProgramBuildBackend(nullptr);

        if (!EnsureShaderProgramManifestBackend(&store))
        {
            result.diagnostics.emplace_back(
                "manifest backend installation failed");
        }
        else if (!RecordShaderProgramManifestEntry(&store, nullptr, entry))
        {
            result.diagnostics.emplace_back(
                "manifest entry recording failed");
        }
        else
        {
            // 关键：把内存态丢掉，逼后端从磁盘重新装载，证明查表不依赖任何
            // 同进程残留状态（发行形态开机就是这样冷的）。
            UninstallShaderProgramManifestBackend();
            if (!EnsureShaderProgramManifestBackend(&store))
            {
                result.diagnostics.emplace_back(
                    "manifest backend reinstallation failed");
            }
            else if (GetShaderProgramManifestEntryCount() != 1)
            {
                result.diagnostics.emplace_back(
                    "manifest reloaded from disk with unexpected entry count");
            }
            else
            {
                ShaderProgramBuildBackend *backend =
                    GetShaderProgramBuildBackend();

                std::unique_ptr<ShaderBuildContext> loaded(
                    backend->CreateBuildContext(nullptr, definition, request));
                if (!loaded)
                {
                    result.diagnostics.emplace_back(
                        "manifest backend failed to assemble a build context");
                }
                else if (!loaded->HasProgramLink()
                      || loaded->GetProgramLink().BuildKey().GetDigest()
                            != link.BuildKey().GetDigest()
                      || !loaded->UsedCachedMeshStage()
                      || !loaded->UsedCachedFragmentStage()
                      || loaded->RequiresRecipeRuntimeRows()
                            != entry.requires_recipe_runtime_rows
                      || !backend->FinalizeBuildContext(loaded.get()))
                {
                    result.diagnostics.emplace_back(
                        "manifest-assembled build context is inconsistent");
                }
                else
                {
                    const ShaderCreateInfo *mesh =
                        loaded->GetStageShader(ShaderStage::Mesh);
                    const ShaderCreateInfo *fragment =
                        loaded->GetStageShader(ShaderStage::Fragment);
                    if (!mesh || !fragment
                     || mesh->GetSPVSize() != sizeof(fake_mesh_spv)
                     || fragment->GetSPVSize() != sizeof(fake_fragment_spv)
                     || std::memcmp(
                            mesh->GetSPVData(), fake_mesh_spv,
                            sizeof(fake_mesh_spv)) != 0
                     || std::memcmp(
                            fragment->GetSPVData(), fake_fragment_spv,
                            sizeof(fake_fragment_spv)) != 0)
                    {
                        result.diagnostics.emplace_back(
                            "manifest-lookup SPV bytes do not match the baked artifacts");
                    }
                }

                // 查不到必须硬失败：发行形态没有编译器可回落。
                MaterialDefinitionBuildRequest ambient_miss = request;
                ambient_miss.ambient_module_name = "AmbientVertexGlobalColor";
                std::unique_ptr<ShaderBuildContext> miss(
                    backend->CreateBuildContext(nullptr, definition, ambient_miss));
                if (miss)
                    result.diagnostics.emplace_back(
                        "manifest backend must hard-fail on an unknown key");

                // ── 多实例共享同一变体（缺陷 #1/#3 的回归）──
                //
                // 同一变体的多个实例只在**实例维度**上不同（配方名、贴图绑定、
                // SSBO 行）。它们发射出的 GLSL 逐字节相同 ⇒ 同一条 program link。
                // 若清单键折进实例维度，烘焙期只填了第一个实例的那条键，发行期
                // 用第二个实例查表就必然 miss —— 现象是「换张贴图材质就没了」。
                MaterialDefinitionBuildRequest second_instance = request;
                second_instance.recipe.recipe_name = "gate-secondary-instance";
                UpsertRecipeTextureBinding(
                    second_instance.recipe, "base_color",
                    "gate/secondary-texture", true);
                second_instance.recipe.material_ssbo_binding.ssbo_type =
                    GlobalSSBOType::PBRSurface;
                second_instance.recipe.material_ssbo_binding.ssbo_id = 11;
                second_instance.recipe.material_ssbo_binding.data_index = 5;

                if (ComputeMaterialProgramManifestKey(
                        definition, second_instance, nullptr)
                    != entry.lookup_key)
                    result.diagnostics.emplace_back(
                        "manifest key splits on instance-only recipe data");

                std::unique_ptr<ShaderBuildContext> second_instance_ctx(
                    backend->CreateBuildContext(
                        nullptr, definition, second_instance));
                if (!second_instance_ctx
                 || !second_instance_ctx->HasProgramLink()
                 || second_instance_ctx->GetProgramLink().BuildKey().GetDigest()
                        != link.BuildKey().GetDigest())
                    result.diagnostics.emplace_back(
                        "second instance of the same variant misses the baked manifest");
            }
        }

        // 无论成败都要把后端槽位还原，避免污染同进程里的其它门。
        UninstallShaderProgramManifestBackend();
        SetShaderProgramBuildBackend(previous_backend);

        result.passed = result.diagnostics.empty();

        if (!keep_artifacts)
        {
            std::filesystem::remove_all(store_root, ec);
            return result;
        }

        // 发行形态探针要对着「已知正确值」比对，故把本次烘焙的结果固化成文本。
        if (result.passed && !WriteManifestExpectation(
                store, definition, entry))
            result.diagnostics.emplace_back(
                "failed to write manifest expectation file");

        result.passed = result.diagnostics.empty();
        return result;
    }

    /// compute 程序的「烘焙 → 只读查表」端到端门。
    ///
    /// 与上面两条门互补：那两条走材质语义键（program_manifest.pmf），这条走
    /// compute 程序名（compute_manifest.pmf）。发行形态里 GPU cull 这一类程序
    /// 既没有材质定义也没有 GLSL 源，唯一的来源就是这里烘焙出的 SPV ——
    /// 门挂掉就意味着发行包起不来。
    ///
    /// 编译用的是**真实** `entity_gpu_cull.glsl`（跑通反而能提前发现源被改坏），
    /// 校验的却只有「发行形态真正需要的那几块数据」：清单可装载、SPV 字节与
    /// digest 对得上、push constant 契约带得回来。
    static GateResult RunComputeProgramCacheCase(
        const std::filesystem::path &store_root,
        const bool keep_artifacts = false)
    {
        GateResult result;
        result.name = keep_artifacts
            ? "PM.emit-compute-release-cache"
            : "PM.compute-release-cache";

        const char *kProgramName = "GPUCullSystem.Pipeline";
        // 与 GPUCullSystem.cpp 的 CullPushConstants 同值：那边 static_assert 锁死 112B，
        // 这里用字面量是刻意的——门要独立断言这个数，而不是把被测代码的常量抄一遍。
        const uint32_t kPushConstantSize = 112;

        std::string glsl_source;
        const std::filesystem::path glsl_path =
            std::filesystem::path(GetShaderLibraryPath().c_str())
            / "ecs" / "entity_gpu_cull.glsl";
        if (!ReadGoldenFile(glsl_path.string(), glsl_source)
         || glsl_source.empty())
        {
            result.diagnostics.emplace_back(
                "cannot read compute GLSL source: " + glsl_path.string());
            result.passed = false;
            return result;
        }

        ShaderCreateInfo sci(ShaderStage::Compute);
        sci.SetFinalGLSL(glsl_source.c_str());
        if (!sci.CompileFinalGLSLToSPV()
         || !sci.HasSPVData() || sci.GetSPVSize() == 0)
        {
            result.diagnostics.emplace_back(
                "compute GLSL failed to compile (source or include graph broken)");
            result.passed = false;
            return result;
        }

        const uint64_t spv_size = static_cast<uint64_t>(sci.GetSPVSize());
        uint32_t reflected_push_constant_size = 0;
        if (!GetSPVPushConstantEnd(
                sci.GetSPVData(),
                sci.GetSPVSize(),
                VK_SHADER_STAGE_COMPUTE_BIT,
                reflected_push_constant_size))
        {
            result.diagnostics.emplace_back(
                "compute SPIR-V push-constant reflection failed");
            result.passed = false;
            return result;
        }
        if (reflected_push_constant_size != kPushConstantSize)
        {
            result.diagnostics.emplace_back(
                "reflected compute push-constant extent differs from CullPushConstants");
            result.passed = false;
            return result;
        }

        std::error_code ec;
        if (!keep_artifacts)
            std::filesystem::remove_all(store_root, ec);

        ShaderArtifactStore store(
            hgl::OSString(store_root.wstring().c_str()),
            ShaderCacheMode::BuildIfMissing);

        const hgl::OSString manifest_path =
            MakeShaderComputeProgramManifestPath(store.GetRootPath());

        if (!RecordShaderComputeProgramArtifact(
                store, kProgramName, sci.GetSPVData(), spv_size,
                reflected_push_constant_size))
        {
            result.diagnostics.emplace_back(
                "failed to record the compute artifact");
            result.passed = false;
            return result;
        }

        // 重复烘焙同一份产物必须幂等：清单不该长出第二条，文件也不该被改坏。
        if (!RecordShaderComputeProgramArtifact(
                store, kProgramName, sci.GetSPVData(), spv_size,
                reflected_push_constant_size))
        {
            result.diagnostics.emplace_back(
                "re-recording the same compute artifact failed");
        }

        {
            ShaderComputeProgramManifest manifest;
            if (!manifest.Load(manifest_path))
                result.diagnostics.emplace_back(
                    "compute manifest unreadable after baking");
            else if (manifest.GetEntryCount() != 1)
                result.diagnostics.emplace_back(
                    "compute manifest must hold exactly one entry after re-baking");
            else if (manifest.GetCodegenGeneration()
                        != contract::SHADERGEN_CODEGEN_GENERATION)
                result.diagnostics.emplace_back(
                    "compute manifest carried a stale codegen generation");
        }

        // 关键：换一个**只读** store 冷装，证明查表不依赖烘焙时留在内存里的任何东西。
        ShaderArtifactStore readonly_store(
            hgl::OSString(store_root.wstring().c_str()),
            ShaderCacheMode::ReadOnly);

        if (readonly_store.GetCacheMode() != ShaderCacheMode::ReadOnly)
            result.diagnostics.emplace_back(
                "read-only store did not honour its cache mode");

        // 只读 store 绝不能回写：哪怕烘焙入口被误调，也必须被拒。
        if (RecordShaderComputeProgramArtifact(
                readonly_store, kProgramName, sci.GetSPVData(),
                spv_size, reflected_push_constant_size))
            result.diagnostics.emplace_back(
                "read-only store accepted a compute artifact write");

        hgl::ValueArray<uint8_t> loaded_spv{};
        uint32_t loaded_push_constant_size = 0;
        if (!LoadShaderComputeProgramArtifact(
                readonly_store, kProgramName, loaded_spv,
                &loaded_push_constant_size))
        {
            result.diagnostics.emplace_back(
                "read-only lookup of the baked compute artifact failed");
        }
        else
        {
            if (loaded_spv.GetCount() != static_cast<int>(spv_size))
                result.diagnostics.emplace_back(
                    "loaded compute SPV size differs from the baked size");
            else if (std::memcmp(loaded_spv.GetData(), sci.GetSPVData(),
                                 static_cast<size_t>(spv_size)) != 0)
                result.diagnostics.emplace_back(
                    "loaded compute SPV bytes differ from the baked bytes");

            if (loaded_push_constant_size != reflected_push_constant_size)
                result.diagnostics.emplace_back(
                    "compute push constant contract did not survive the cache");
        }

        // 查不到必须硬失败：发行形态没有编译器可回落。
        {
            hgl::ValueArray<uint8_t> missing{};
            if (LoadShaderComputeProgramArtifact(
                    readonly_store, "NoSuch.Compute.Program", missing))
                result.diagnostics.emplace_back(
                    "unknown compute program must not resolve from the cache");
        }

        // 清单被改一个字节就必须整份作废（宁可缺 artifact，也不要用坏数据建管线）。
        if (result.diagnostics.empty())
        {
            std::string bytes;
            if (!ReadGoldenFile(std::filesystem::path(manifest_path.c_str()).string(),
                                bytes))
                result.diagnostics.emplace_back(
                    "cannot re-read the compute manifest for corruption test");
            else if (bytes.empty())
                result.diagnostics.emplace_back(
                    "compute manifest is empty");
            else
            {
                bytes.back() = static_cast<char>(bytes.back() ^ 0x5A);
                const std::filesystem::path corrupt_root =
                    std::filesystem::path(store_root) / "compute-corrupt";

                const hgl::OSString corrupt_path =
                    MakeShaderComputeProgramManifestPath(
                        hgl::OSString(corrupt_root.wstring().c_str()));

                if (!WriteTextFile(
                        std::filesystem::path(corrupt_path.c_str()).string(),
                        bytes))
                    result.diagnostics.emplace_back(
                        "cannot write the corrupted compute manifest");
                else
                {
                    ShaderComputeProgramManifest corrupt;
                    if (corrupt.Load(corrupt_path) || !corrupt.IsEmpty())
                        result.diagnostics.emplace_back(
                            "corrupted compute manifest was accepted");
                }

                std::filesystem::remove_all(corrupt_root, ec);
            }
        }

        result.passed = result.diagnostics.empty();

        if (!keep_artifacts)
        {
            std::filesystem::remove_all(store_root, ec);
            return result;
        }

        if (result.passed)
        {
            // 发行形态探针要对着「已知正确值」比对；字段名与 RuntimeFormProbe.cpp 的
            // LoadComputeExpectation() 严格对应，改一处必须同步另一处。
            const std::filesystem::path expect_path =
                std::filesystem::path(manifest_path.c_str()).parent_path()
                / "compute_expectation.txt";

            hgl::hash::FNV1aHasher64 hasher;
            hasher.AppendBytes(sci.GetSPVData(), static_cast<size_t>(spv_size));

            std::ofstream out(expect_path, std::ios::binary);
            if (!out)
                result.diagnostics.emplace_back(
                    "cannot write the compute expectation file");
            else
            {
                out << "program_name=" << kProgramName << "\n"
                    << "spv_bytes=" << spv_size << "\n"
                    << "spv_digest=" << static_cast<uint64_t>(hasher) << "\n"
                    << "push_constant_size=" << kPushConstantSize << "\n";

                if (!out.good())
                    result.diagnostics.emplace_back(
                        "failed writing the compute expectation file");
            }
        }

        result.passed = result.diagnostics.empty();
        return result;
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
        const char *definition_id;
        const char *variant_slug;
        ShaderProgramPurpose purpose;
        RenderTemplateID template_id;
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
    };

    /// 快照矩阵：四条发射路径（ForwardLit / ForwardUnlit / ShadowCaster / Sky）
    /// × 领域差异（有纹理 / 纯顶点 / 无顶点格式的 SSBO 材质）。
    ///
    /// 两张快照（结构、发射顺序）共用此表——矩阵只有一份，新增材质不会漏测其中一张。
    static const ReflectionCoverageVariant kReflectionCoverageVariants[] =
    {
        { "Lit", "lit-forward-opaque",
          ShaderProgramPurpose::ForwardColor,
          RenderTemplateID::ForwardLitShadowedAO, true, false },
        { "Lit", "lit-depth-only",
          ShaderProgramPurpose::DepthOnly,
          RenderTemplateID::ShadowCasterOpaque, true, false },
        { "Lit", "lit-shadow-depth",
          ShaderProgramPurpose::ShadowDepth,
          RenderTemplateID::ShadowCasterOpaque, true, false },
        { "Lit", "lit-depth-masked",
          ShaderProgramPurpose::DepthOnly,
          RenderTemplateID::ShadowCasterMasked, true, true },
        { "Lit", "lit-shadow-masked",
          ShaderProgramPurpose::ShadowDepth,
          RenderTemplateID::ShadowCasterMasked, true, true },
        { "VertexPaletteColor", "vertex-palette-color-forward",
          ShaderProgramPurpose::ForwardColor,
          RenderTemplateID::ForwardUnlit, true, false },
        { "VertexColor", "vertex-color-forward",
          ShaderProgramPurpose::ForwardColor,
          RenderTemplateID::ForwardUnlit, true, false },
        { "VertexLuminance", "vertex-luminance-forward",
          ShaderProgramPurpose::ForwardColor,
          RenderTemplateID::ForwardUnlit, true, false },
        { "UnlitTexture", "unlit-texture-forward",
          ShaderProgramPurpose::ForwardColor,
          RenderTemplateID::ForwardUnlit, true, false,
          PrimitiveType::Triangles, true },
        { "DebugNormalColor", "debug-normal-forward",
          ShaderProgramPurpose::ForwardColor,
          RenderTemplateID::ForwardUnlit, true, false },
        { "SkyCube", "sky-cube-forward",
          ShaderProgramPurpose::ForwardColor,
          RenderTemplateID::Sky, true, false },
        { "SkyMinimal", "sky-minimal-forward",
          ShaderProgramPurpose::ForwardColor,
          RenderTemplateID::Sky, true, false },
        { "builtin/checkerboard_2d", "checkerboard-2d-forward",
          ShaderProgramPurpose::ForwardColor,
          RenderTemplateID::ForwardUnlit, true, false,
          PrimitiveType::Triangles, true },
        { "builtin/text_gpu", "text-gpu-charquad",
          ShaderProgramPurpose::ForwardColor,
          RenderTemplateID::ForwardUnlit, false, false },
        // LineQuad 是第三种 mesh 发射模式，只能由 `primitive_type = Lines` 推出来
        // （材质定义里没有对应字段）。这些用例覆盖 LineQuad 固定拓扑 master 与
        // `mesh/line_quad_body.glsl` 的端点读取路径，避免该独立几何路径退化后漏报。
        { "VertexColor", "vertex-color-lines-quad",
          ShaderProgramPurpose::ForwardColor,
          RenderTemplateID::ForwardUnlit, true, false,
          PrimitiveType::Lines, false },
        // 2D 位置需要与顶点格式匹配的 2D 映射模块。用 checkerboard_2d
        // （NDCLift）覆盖 VF_V2F 夹具，避免把 Passthrough3D 的 vec3 ABI 错配进来。
        { "builtin/checkerboard_2d", "checkerboard-2d-lines-quad-2d",
          ShaderProgramPurpose::ForwardColor,
          RenderTemplateID::ForwardUnlit, true, false,
          PrimitiveType::Lines, true },
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

        const std::string path = GetShaderLibraryPath() + "/sampler.toml";
        const hgl::filesystem::Path sampler_toml(hgl::ToOSString(path));
        if (lib.Load(sampler_toml.ToOSString()))
            return true;

        diagnostic = "cannot load sampler presets from " + path;
        return false;
    }

    static SceneRenderTemplateProfile MakeReflectionCoverageProfile(
        const RenderTemplateID template_id)
    {
        switch (template_id)
        {
            case RenderTemplateID::ForwardLitShadowedAO:
            case RenderTemplateID::ForwardLitShadowedIdentityAO:
                return MakeIdentityForwardLitProfile();
            case RenderTemplateID::ForwardUnlit:
                return MakeForwardUnlitProfile();
            case RenderTemplateID::Sky:
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
        MaterialDefinition definition{};
        if (!TryGetMaterialDefinitionByID(variant.definition_id, definition))
        {
            diagnostic = std::string("definition unavailable: ") + variant.definition_id;
            return nullptr;
        }

        // 必须在解析 template request 之前生效：coverage 契约（是否需要求 alpha、
        // 需要哪些 interstage 语义）由 `ResolveMaterialRenderState` 从这里读。
        if (variant.alpha_test)
        {
            definition.default_render_state.alpha_test = true;
            definition.default_render_state.alpha_cutoff = 0.5f;
        }

        MaterialDefinitionBuildRequest request{};
        request.recipe.mtl_def_id = definition.definition_id;
        request.primitive_type = variant.primitive_type;
        request.geometry_vertex_format =
            variant.has_geometry
                ? (definition.domain == MaterialDomain::VertexPalette
                       ? &kReflectionCoveragePaletteGeometry
                       : (variant.vec2_position
                              ? &kReflectionCoverageGeometry2D
                              : &kReflectionCoverageGeometry))
                : nullptr;
        request.defer_finalize = true;
        request.shader_program_purpose = variant.purpose;

        RenderTemplateValidationDiagnostic template_diagnostic{};
        if (!ResolveSceneRenderTemplateRequest(
                variant.template_id, hgl::graph::ShaderStage::Fragment,
                MakeReflectionCoverageProfile(variant.template_id),
                request.render_template_request, template_diagnostic)
         || !AppendMaterialRenderTemplateRoots(
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

        // 形状主着色器（`mesh/shape/*.mesh`）是唯一的固定源：形状名即文件名，
        // 三种几何模式（VertexPassthrough / LineQuad / CharQuad）全在此目录。
        const std::filesystem::path dirs[1] =
        {
            RepoRootPath("ShaderLibrary/mesh/shape")
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


    // ══════════════════════════════════════════════════════════════════════
    // SPV.generated-interface
    //
    // 生成接口的**反射门**：SPIR-V 是接口的唯一权威。
    //
    // 生成器在编译前会自述「我生成了什么」，而自述可以自洽却与真正编出的二进
    // 制不一致——唯一能证伪它的是编译器。本用例把整张覆盖矩阵真的编译成
    // SPIR-V 并反射，按 Vulkan 的链接规则校验：
    //   ① 两 stage 都必须编译成功。此前这 16 个变体的 GLSL 在门里从未被编译过，
    //      生成器发错一行就能让整套 shader 只在跑 demo 时才炸；
    //   ② 片元每个 stage input 必须在 mesh 的 stage output 里找到同 location 同
    //      类型的对应项 —— 这是管线创建时驱动会硬拒的条件；
    //   ③ 前向着色必须写至少一个颜色附件；纯深度/阴影用途必须一个都不写；
    //   ④ 两 stage 的 push constant 末端必须等于 RootAddresses 的 CPU 侧大小。
    // ①②③④ 全部来自二进制，完全不看生成器怎么描述自己。
    static GateResult RunGeneratedInterfaceReflectionCase()
    {
        GateResult result;
        result.name = "SPV.generated-interface";

        std::string preset_diagnostic;
        if (!EnsureSamplerPresetsLoaded(preset_diagnostic))
        {
            result.diagnostics.emplace_back(preset_diagnostic);
            result.passed = false;
            return result;
        }

        // RootAddresses = 10 个 uint64 地址 + 2 个 uint 标量（camera_row + 显式
        // padding）。这里刻意写死数值（与 `PM.compute-release-cache` 的 112 同理）：
        // 若两侧一起漂，本门才会失败。
        const uint32_t kRootAddressesPCBytes = 88;

        // 诊断通道自检：故意编一段非法 GLSL，必须同时得到 nullptr 与非空报错。
        // 这道断言保护的是「本用例下方那些编译失败诊断有内容」这件事本身 ——
        // 没有它，编译器一旦退回「静默 false」，本门就只会输出「没编过」。
        {
            hgl::AnsiString broken_log;
            if (hgl::graph::CompileShader(
                    uint32_t(VK_SHADER_STAGE_FRAGMENT_BIT),
                    "layout(location=0) out vec4 c;\nvoid main() { c = not_declared; }\n",
                    &broken_log) != nullptr)
            {
                result.diagnostics.emplace_back(
                    "invalid GLSL compiled successfully (diagnostic channel broken)");
            }
            else if (broken_log.IsEmpty())
            {
                result.diagnostics.emplace_back(
                    "compile failure carried no compiler log (diagnostic channel broken)");
            }
        }

        // 完整性：每个变体都必须走到「两 stage 都反射成功」这一步。
        // 任何 `continue` 提前退出都会留下诊断，这里再加一道计数，
        // 防止将来新增的跳过分支把变体静默漏掉。
        uint32_t reflected_variant_count = 0;

        std::vector<std::string> fixed_mesh_catalog;
        {
            std::string catalog_diagnostic;
            if (!LoadFixedMeshCatalog(fixed_mesh_catalog, catalog_diagnostic))
                result.diagnostics.emplace_back(catalog_diagnostic);
        }

        for (const ReflectionCoverageVariant &variant : kReflectionCoverageVariants)
        {
            std::string build_diagnostic;
            FixedFragmentSource fixed_source;
            hgl::AnsiString mesh_options_text;
            hgl::AnsiString common_options_text;
            std::unique_ptr<ShaderBuildContext> ctx =
                BuildReflectionCoverageVariant(
                    variant, build_diagnostic, &fixed_source,
                    &mesh_options_text, &common_options_text);
            if (!ctx)
            {
                result.diagnostics.emplace_back(build_diagnostic);
                continue;
            }

            // 网格主着色器里有 `#include "generated/mesh_options.glsl"`：先在
            // 片段 options 已落盘的那个根下把它写出来，下面手写编译才有第二根可读。
            if (!mesh_options_text.IsEmpty()
             && !hgl::graph::mtl::WriteMeshOptionsSource(
                    fixed_source.options_root, mesh_options_text))
            {
                result.diagnostics.emplace_back(
                    std::string(variant.variant_slug)
                    + ": cannot write generated/mesh_options.glsl | "
                    + hgl::graph::mtl::GetLastDomainShaderBuildBackendError());
                continue;
            }

            // 网格主着色器同时也 include `generated/common_options.glsl`
            //（阶段间 varying 开关）：它与片段共用一份，必须落在同一个根。
            if (!common_options_text.IsEmpty()
             && !hgl::graph::mtl::WriteInterStageVaryingOptionsSource(
                    fixed_source.options_root, common_options_text))
            {
                result.diagnostics.emplace_back(
                    std::string(variant.variant_slug)
                    + ": cannot write generated/common_options.glsl | "
                    + hgl::graph::mtl::GetLastDomainShaderBuildBackendError());
                continue;
            }

            const std::string label = variant.variant_slug;

            hgl::graph::mtl::ShaderReflectionData reflection[2];
            const uint32_t stage_bits[2] =
            {
                VK_SHADER_STAGE_MESH_BIT_EXT,
                VK_SHADER_STAGE_FRAGMENT_BIT
            };
            const hgl::graph::ShaderStage stage_enums[2] =
            {
                hgl::graph::ShaderStage::Mesh,
                hgl::graph::ShaderStage::Fragment
            };
            const char *const stage_names[2] = { "mesh", "fragment" };

            bool reflected[2] = { false, false };

            for (int i = 0; i < 2; ++i)
            {
                // 编译由本门自己做（而不是让 BuildReflectionCoverageVariant 顺手 finalize），
                // 这样「哪个 stage 的哪一步失败」能落到诊断里：
                // `FinalizeShaderBuildContext` 对空源码是静默 false，看不出根因。
                ShaderCreateInfo *si = ctx->GetStageShader(stage_enums[i]);
                if (!si)
                {
                    result.diagnostics.emplace_back(
                        label + " " + stage_names[i] + ": stage missing");
                    continue;
                }

                if (si->GetFinalGLSL().empty())
                {
                    result.diagnostics.emplace_back(
                        label + " " + stage_names[i]
                        + ": generated GLSL is empty");
                    continue;
                }

                // 每个变体的网格阶段都必须能在固定源目录里找到同一份文本。
                // 6.5 删掉 meshgen 之后没有生成器兜底：目录里缺这一份，该变体
                // 就没有 mesh 阶段可编译。这条断言把「矩阵新增了一个形状但没人
                // 捕获固定源」变成红灯，而不是等到运行时才发现某个域画不出来。
                if (i == 0 && !FixedMeshCatalogHas(fixed_mesh_catalog, si->GetFinalGLSL()))
                {
                    result.diagnostics.emplace_back(
                        label + ": mesh stage has no source in "
                        "ShaderLibrary/mesh/shape 下没有对应形状 master"
                        "（形状 master 需人工按形状新增，见 plan.md 阶段七-2）");
                }

                if (!si->HasSPVData())
                {
                    // 带诊断的编译入口：glslang 的原始报错（含行号与生成的 GLSL 片段）
                    // 直接进诊断。没有它，失败信息只剩「没编过」—— SPVData 一被释
                    // 放日志就取不回来了。
                    //
                    // 片段走生产那条编译路径（第二 include 根由后端管）：固定源里含
                    // `#include "generated/frag_options.glsl"`，手写编译没有那个根就必然
                    // 编不过，而复制一份根管理只会等它慢慢漂移。
                    hgl::AnsiString compile_diagnostic;
                    const bool compiled =
                        i == 1
                            ? CompileFixedFragmentSource(*ctx, fixed_source, compile_diagnostic)
                            : CompileFixedMeshSource(*ctx, fixed_source, compile_diagnostic);
                    if (!compiled)
                    {
                        result.diagnostics.emplace_back(
                            label + " " + stage_names[i]
                            + ": generated GLSL failed to compile: "
                            + (compile_diagnostic.c_str() ? compile_diagnostic.c_str() : ""));
                        continue;
                    }
                }

                if (!si->HasSPVData() || si->GetSPVSize() == 0)
                {
                    result.diagnostics.emplace_back(
                        label + " " + stage_names[i]
                        + ": no SPIR-V after compile");
                    continue;
                }

                hgl::ValueArray<uint8_t> flat_storage;
                ShaderFlatView view{};
                if (!ReflectSPVData(
                        si->GetSPVData(), si->GetSPVSize(), stage_bits[i],
                        "main", flat_storage, view)
                 || !reflection[i].Extract(view))
                {
                    result.diagnostics.emplace_back(
                        label + " " + stage_names[i]
                        + ": SPIR-V reflection failed");
                    continue;
                }

                reflected[i] = true;

                if (reflection[i].push_constants.IsEmpty())
                {
                    result.diagnostics.emplace_back(
                        label + " " + stage_names[i]
                        + ": push constant block missing");
                    continue;
                }

                uint32_t pc_end = 0;
                if (!reflection[i].GetPushConstantEnd(pc_end)
                 || pc_end != kRootAddressesPCBytes)
                {
                    result.diagnostics.emplace_back(
                        label + " " + stage_names[i]
                        + ": push constant end " + std::to_string(pc_end)
                        + " != " + std::to_string(kRootAddressesPCBytes));
                }
            }

            if (!reflected[0] || !reflected[1])
                continue;

            ++reflected_variant_count;

            // ② 片元 input ⊆ mesh output（同 location、同 basetype、同 vec_size）
            for (int i = 0; i < reflection[1].stage_inputs.GetCount(); ++i)
            {
                const hgl::graph::mtl::ShaderReflectionAttribute &fin =
                    reflection[1].stage_inputs[i];

                const hgl::graph::mtl::ShaderReflectionAttribute *match = nullptr;
                for (int j = 0; j < reflection[0].stage_outputs.GetCount(); ++j)
                {
                    if (reflection[0].stage_outputs[j].location == fin.location)
                    {
                        match = &reflection[0].stage_outputs[j];
                        break;
                    }
                }

                if (!match)
                {
                    result.diagnostics.emplace_back(
                        label + ": fragment input #"
                        + std::to_string(int(fin.location)) + " ("
                        + fin.name + ") has no matching mesh output");
                    continue;
                }

                if (match->basetype != fin.basetype
                 || match->vec_size != fin.vec_size
                 || match->col_count != fin.col_count)
                {
                    result.diagnostics.emplace_back(
                        label + ": interface type mismatch at #"
                        + std::to_string(int(fin.location))
                        + " mesh(" + match->name + ",base="
                        + std::to_string(int(match->basetype)) + ",vec="
                        + std::to_string(int(match->vec_size)) + ",col="
                        + std::to_string(int(match->col_count)) + ") fragment("
                        + fin.name + ",base=" + std::to_string(int(fin.basetype))
                        + ",vec=" + std::to_string(int(fin.vec_size)) + ",col="
                        + std::to_string(int(fin.col_count)) + ")");
                }
            }

            // ③ 输出附件：前向着色写颜色，纯深度/阴影一个都不写
            const bool expects_color =
                variant.purpose == ShaderProgramPurpose::ForwardColor;
            const int color_outputs = reflection[1].stage_outputs.GetCount();

            if (expects_color && color_outputs <= 0)
                result.diagnostics.emplace_back(
                    label + ": forward purpose wrote no color attachment");
            else if (!expects_color && color_outputs != 0)
                result.diagnostics.emplace_back(
                    label + ": depth purpose wrote "
                    + std::to_string(color_outputs) + " color attachment(s)");
        }

        const uint32_t expected_variant_count =
            static_cast<uint32_t>(sizeof(kReflectionCoverageVariants)
                                  / sizeof(kReflectionCoverageVariants[0]));
        if (reflected_variant_count != expected_variant_count)
            result.diagnostics.emplace_back(
                "only " + std::to_string(reflected_variant_count) + " of "
                + std::to_string(expected_variant_count)
                + " structure-dump variants reached SPIR-V reflection");

        result.passed = result.diagnostics.empty();
        return result;
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
        const char *definition_id;
        ShaderProgramPurpose purpose;
        bool alpha_test;
    };

    /// 等价矩阵：每个形状 = 「某个材质定义 + 某个 pass」的编译单元。
    /// 与前缀无关：条目顺序被 `DH.domain-backend-cache` 按下标引用，**不要重排**，
    /// 新增条目一律追加到末尾。
    static const DomainBackendEquivShape kDomainBackendEquivShapes[] =
    {
        { "fwd",          "Lit",     ShaderProgramPurpose::ForwardColor, false },
        { "fwd-alpha",    "Lit",     ShaderProgramPurpose::ForwardColor, true  },
        { "shadow",       "Lit",     ShaderProgramPurpose::ShadowDepth,  false },
        { "shadow-alpha", "Lit",     ShaderProgramPurpose::ShadowDepth,  true  },
        { "sky",          "SkyCube", ShaderProgramPurpose::ForwardColor, false },
        { "unlit-tex",    "UnlitTexture", ShaderProgramPurpose::ForwardColor, false },
        { "vert-color",   "VertexColor",  ShaderProgramPurpose::ForwardColor, false },
        { "vert-lum",     "VertexLuminance", ShaderProgramPurpose::ForwardColor, false },
        { "vert-pal",     "VertexPaletteColor", ShaderProgramPurpose::ForwardColor, false },
        { "unlit-solid",  "builtin/pure_color", ShaderProgramPurpose::ForwardColor, false },
        { "checker-2d",   "builtin/checkerboard_2d", ShaderProgramPurpose::ForwardColor, false },
        { "checker-3d",   "builtin/checkerboard_3d", ShaderProgramPurpose::ForwardColor, false },
        { "err-check-3d", "builtin/error_checkerboard_3d", ShaderProgramPurpose::ForwardColor, false },
        { "text-gpu",     "builtin/text_gpu", ShaderProgramPurpose::ForwardColor, false },
        { "debug-normal", "DebugNormalColor", ShaderProgramPurpose::ForwardColor, false },
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
        const MaterialDefinition &definition,
        GeometryVertexFormat &geometry,
        MaterialDefinitionBuildRequest &request,
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
            hgl::ValueArray<ShaderCodeModuleSemanticRequirement> requirements;
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
                const ShaderCodeModuleSemantic wanted =
                    GetShaderCodeModuleSemanticFromVertexSemantic(entry.semantic);
                if (wanted == ShaderCodeModuleSemantic::Unknown)
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

                const VkFormat format =
                    entry.semantic == VertexSemantic::Color
                     && definition.domain == MaterialDomain::VertexPalette
                        ? VK_FORMAT_R8_UINT
                        : entry.format;

                if (required && !geometry.Add(entry.semantic, format, 0, 0))
                {
                    diagnostic = "geometry.Add(contract semantic) failed";
                    return false;
                }
            }
        }

        request.recipe.mtl_def_id = definition.definition_id;
        request.recipe.render_state_overrides.has_alpha_test = shape.alpha_test;
        request.recipe.render_state_overrides.alpha_test = shape.alpha_test;
        request.recipe.render_state_overrides.has_alpha_cutoff = shape.alpha_test;
        request.recipe.render_state_overrides.alpha_cutoff = 0.5f;
        request.geometry_vertex_format = &geometry;
        request.shader_program_purpose = shape.purpose;

        // 模板选择**必须**走引擎的路由器：它按「深度？/有 NTB？/有材质源？」
        // 选模板与 profile，并顺带挂上材质模块根。早期这里手写过一份等价
        // 分支，Sky 立刻炸了（SkyCube 没有 NTB provider，路由到 ForwardUnlit
        // 而不是 ForwardLitShadowedAO）——不要再复制这些规则。
        if (!SelectSceneRenderTemplateRequest(
                definition, shape.purpose, request.recipe,
                request.render_template_request, nullptr))
        {
            diagnostic = "SelectSceneRenderTemplateRequest failed";
            return false;
        }

        // 路由器只负责「选哪个模板 + 哪个 profile」；材质自身的能力
        // （surface / ntb / shadow … 槽）要靠这一步搬进请求，否则
        // `ValidateRenderTemplateRequest` 会以「槽缺失」拒绝构建。
        if (!AppendMaterialRenderTemplateRoots(
                definition, request.render_template_request))
        {
            diagnostic = "AppendMaterialRenderTemplateRoots failed";
            return false;
        }

        return true;
    }

    static GateResult RunDomainBackendEquivalenceCase()
    {
        GateResult result;
        result.name = "DH.domain-backend-equivalence";

        std::string preset_diagnostic;
        if (!EnsureSamplerPresetsLoaded(preset_diagnostic))
        {
            result.diagnostics.emplace_back(preset_diagnostic);
            result.passed = false;
            return result;
        }

        for (const DomainBackendEquivShape &shape : kDomainBackendEquivShapes)
        {
            const std::string label = std::string(shape.label)
                                    + " (" + shape.definition_id + ")";

            MaterialDefinition definition{};
            if (!TryGetMaterialDefinitionByID(shape.definition_id, definition))
            {
                result.diagnostics.emplace_back(label + ": definition unavailable");
                continue;
            }

            GeometryVertexFormat geometry;
            MaterialDefinitionBuildRequest request{};
            std::string build_diagnostic;
            if (!BuildDomainEquivRequest(
                    shape, definition, geometry, request, build_diagnostic))
            {
                result.diagnostics.emplace_back(label + ": " + build_diagnostic);
                continue;
            }

            // ── ① 领域后端路径（临时接管槽位，跑完交还）────────────────────
            request.defer_finalize = false;
            std::string build_error;
            std::unique_ptr<ShaderBuildContext> domain_built =
                BuildViaDomainBackend(definition, request, build_error);
            if (!domain_built)
            {
                result.diagnostics.emplace_back(
                    label + ": domain backend build failed: " + build_error);
                continue;
            }

            // ── ② 片段阶段必须有 SPV，且必须真的吃 generated/frag_options.glsl ──
            const ShaderCreateInfo *dom_fs = domain_built->has_fragment()
                ? domain_built->GetStageShader(ShaderStage::Fragment) : nullptr;
            if (!dom_fs)
            {
                result.diagnostics.emplace_back(label + ": fragment stage missing");
                continue;
            }
            if (!dom_fs->HasSPVData())
            {
                result.diagnostics.emplace_back(label + ": fragment SPIR-V missing");
                continue;
            }

            const std::string &dom_glsl = dom_fs->GetFinalGLSL();
            if (dom_glsl.find("generated/frag_options.glsl") == std::string::npos)
            {
                result.diagnostics.emplace_back(
                    label + ": domain GLSL does not include generated/frag_options.glsl"
                    " (include root wiring broken)");
                continue;
            }

            hgl::ValueArray<uint8_t> dom_storage;
            ShaderFlatView dom_view{};
            if (!ReflectSPVData(
                    dom_fs->GetSPVData(), dom_fs->GetSPVSize(),
                    uint32_t(ShaderStage::Fragment), "main", dom_storage, dom_view))
            {
                result.diagnostics.emplace_back(label + ": domain reflection failed");
                continue;
            }

            const std::string dom_text = FormatDomainEquivReflection(dom_view);

            // ── ③ 冻结基线 ─────────────────────────────────────────────────
            //
            // 这里曾经有第二个"生成器"侧构建，用两条独立拼装路径产出同一形状
            // 再逐字节比对——那才是"等价"的证据。片段阶段被领域主着色器物理
            // 固定、且 MaterialProgramAssembler 已拒收没有固定片段源的构建之后，
            // 第二条路径不存在了，继续留着只会是同一份输出的自证。
            //
            // 基线本身仍然有价值：它是这 15 个形状的反射文本 + SPV 体积的
            // 变更探测器，任何领域主着色器 / options 发射的漂移都会在这里
            // 现形。逐字节 SPV 比对已无对象，故不再参与。
            //
            // 注意：本用例**不做** mesh ↔ fragment varying 链接校验，
            // `fragDataIndexID` 那类"片段声明了、网格没发射"的缺陷它看不见。
            // 那类问题由 SPV.generated-interface 用真实 Vulkan 规则覆盖。
            const std::string &golden_text = dom_text;
            const size_t golden_size = dom_fs->GetSPVSize();

            // ── ④ 与冻结基线比对 ─────────────────────────────────────────
            {
                char header[256];
                std::snprintf(header, sizeof(header),
                              "# DH.domain-backend-equivalence shape=%s"
                              " definition=%s purpose=%d alpha_test=%d\n",
                              shape.label, shape.definition_id,
                              int(shape.purpose), shape.alpha_test ? 1 : 0);
                char size_line[64];
                std::snprintf(size_line, sizeof(size_line),
                              "spv_bytes=%zu\n", golden_size);

                const std::string dump =
                    std::string(header) + NormalizeDumpEOL(golden_text) + size_line;

                const std::string golden_path = RepoRootPath(
                    (std::string("src/Tools/ShaderGen/golden/domain-backend/")
                     + shape.label + ".equiv.txt").c_str());

                CompareAgainstGolden(
                    golden_path, dump, "DH.domain-equiv", result.diagnostics);

                char ok[160];
                std::snprintf(ok, sizeof(ok), "%s: baseline ok (spv=%zu bytes)",
                              label.c_str(), golden_size);
                std::fprintf(stdout, "[INFO] DH.domain-backend %s\n", ok);
            }
        }

        result.passed = result.diagnostics.empty();
        return result;
    }

    // ═══════════════════════════════════════════════════════════════════════
    // DH.domain-backend-cache — 领域后端产物真的能烘焙进 SPV Cache
    // ═══════════════════════════════════════════════════════════════════════
    //
    // 等价性用例比的是「两条路径编出来的东西一样」，但它**故意不带**
    // artifact store（带了就可能读到彼此的缓存，测到的就不再是编译）。这条
    // 用例反过来只证明落盘/复用这条链，以及换源后的元数据自洽：
    //
    //   ① 冷构建：空目录 ⇒ 两阶段都 miss，编译并写盘；
    //   ② 重开一个新 store 对象（同一目录）⇒ 两阶段都 hit，且 SPV 逐字节
    //      等于冷构建那次；
    //   ③ `generated_source_digest` 必须等于**当前**两阶段最终 GLSL 的摘要。
    //      `AssembleMaterialProgram` 是在换源前建元数据的，它算的是那份根本没被
    //      编译的生成物——不在换源后重算，这个字段就永远描述另一份文本；
    //   ④ 落盘的 `.frag` 源必须含 `generated/frag_options.glsl`，即缓存里存的是
    //      固定主着色器，不是旧的生成物。
    //
    // 只跑 `StandardOpaque` 的 4 个 pass 形状，外加无光族的两个前向形状
    // （`UnlitTexture` 与 `VertexColor`）：前者覆盖「同 domain 多 pass」的
    // 缓存分歧，后者覆盖「多 domain 共享同一份缓存机制」且两个形状的主着色器
    // 文本与 options 都不同 ⇒ 摘要必然不同，能继续守住「摘要必须覆盖 options」。
    //
    // 再加两个必须单独覆盖的形态：
    //   · `DebugNormal` —— 材质私有行是 `EmissiveSurface`（跨度 16），与前面
    //     全部 `PBRSurface`（跨度 32）不同，缓存键里若掺了行布局差异会在这里炸；
    //   · `builtin/text_gpu` —— 走 unlit 骨架 + 贴图表 + `styleID` 通道，
    //     是唯一 `HGL_ALPHA_OUTPUT` 真的生效的形状。
    //
    // 不加 `VertexPaletteColor`：它与 `VertexColor` 的固定主着色器**逐字节相同**、
    // options 也相同，摘要天然相等——放进「两两互不相同」的断言里会假失败
    //（它们本来就该共享同一份产物，只是落在不同定义各自的缓存目录里）。

    static GateResult RunDomainBackendCacheCase()
    {
        GateResult result;
        result.name = "DH.domain-backend-cache";

        std::string preset_diagnostic;
        if (!EnsureSamplerPresetsLoaded(preset_diagnostic))
        {
            result.diagnostics.emplace_back(preset_diagnostic);
            result.passed = false;
            return result;
        }

        const DomainBackendEquivShape *shapes[] =
        {
            &kDomainBackendEquivShapes[0],  // fwd
            &kDomainBackendEquivShapes[1],  // fwd-alpha
            &kDomainBackendEquivShapes[2],  // shadow
            &kDomainBackendEquivShapes[3],  // shadow-alpha
            &kDomainBackendEquivShapes[5],  // unlit-tex
            &kDomainBackendEquivShapes[6],  // vert-color
            &kDomainBackendEquivShapes[13], // text-gpu
            &kDomainBackendEquivShapes[14], // debug-normal
        };
        constexpr size_t kCacheShapeCount = sizeof(shapes) / sizeof(shapes[0]);

        // 每个形状的片段阶段 compile_inputs_hash。最后要求两两互不相同：
        // domain options 的差异只体现在 `generated/frag_options.glsl`，主着色器文本
        // 逐字节相同，所以选项必须进 compile_inputs_hash —— 否则两个不同 pass 变体
        // 会共享同一个 stage 键，撞上缓存文件名就是静默复用错误的 SPV。
        std::string digest_labels[kCacheShapeCount];
        hgl::uint64 digest_values[kCacheShapeCount] = {};
        size_t digest_count = 0;

        for (const DomainBackendEquivShape *shape : shapes)
        {
            const std::string label = std::string(shape->label)
                                    + " (" + shape->definition_id + ")";

            MaterialDefinition definition{};
            if (!TryGetMaterialDefinitionByID(shape->definition_id, definition))
            {
                result.diagnostics.emplace_back(label + ": definition unavailable");
                continue;
            }

            GeometryVertexFormat geometry;
            MaterialDefinitionBuildRequest request{};
            std::string build_diagnostic;
            if (!BuildDomainEquivRequest(
                    *shape, definition, geometry, request, build_diagnostic))
            {
                result.diagnostics.emplace_back(label + ": " + build_diagnostic);
                continue;
            }

            // 每个形状独占一个目录：共用会让别条用例的产物冒充命中。
            const std::filesystem::path cache_root =
                std::filesystem::path(RepoRootPath("build"))
                / "domain-backend-cache" / shape->label;
            std::error_code remove_ec;
            std::filesystem::remove_all(cache_root, remove_ec);
            // store 自己在写盘时才建 `<root>/shader-cache/stage`，而它的 `MakePath`
            // 依赖父目录已存在——先把根建出来，否则失败看起来像「元数据不合法」。
            std::filesystem::create_directories(cache_root, remove_ec);

            const hgl::OSString cache_root_os =
                hgl::ToOSString(cache_root.generic_string());
            request.defer_finalize = true;

            // ── ① 冷构建 ─────────────────────────────────────────────────
            std::unique_ptr<ShaderBuildContext> cold_ctx;
            {
                ShaderArtifactStore store(cache_root_os, ShaderCacheMode::BuildIfMissing);
                request.shader_artifact_store = &store;

                InstallDomainShaderBuildBackend();
                ShaderProgramBuildBackend *backend = GetShaderProgramBuildBackend();
                if (!backend)
                {
                    InstallDomainShaderBuildBackend();
                    result.diagnostics.emplace_back(label + ": domain backend slot empty");
                    continue;
                }

                cold_ctx.reset(backend->CreateBuildContext(nullptr, definition, request));
                if (!cold_ctx)
                {
                    const char *err = GetLastDomainShaderBuildBackendError();
                    InstallDomainShaderBuildBackend();
                    result.diagnostics.emplace_back(
                        label + ": cold build failed: "
                        + (err && err[0] ? err : "unknown error"));
                    continue;
                }
                if (!backend->FinalizeBuildContext(cold_ctx.get()))
                {
                    InstallDomainShaderBuildBackend();
                    char detail[256];
                    std::snprintf(detail, sizeof(detail),
                                  ": cold finalize failed (link=%d metadata=%d"
                                  " mesh_spv=%d frag_spv=%d)",
                                  cold_ctx->HasProgramLink() ? 1 : 0,
                                  cold_ctx->HasProgramArtifactMetadata() ? 1 : 0,
                                  cold_ctx->GetStageShader(ShaderStage::Mesh)
                                      && cold_ctx->GetStageShader(ShaderStage::Mesh)->HasSPVData() ? 1 : 0,
                                  cold_ctx->GetStageShader(ShaderStage::Fragment)
                                      && cold_ctx->GetStageShader(ShaderStage::Fragment)->HasSPVData() ? 1 : 0);
                    result.diagnostics.emplace_back(label + detail);
                    continue;
                }
                InstallDomainShaderBuildBackend();
            }

            const ShaderCreateInfo *cold_fs =
                cold_ctx->GetStageShader(ShaderStage::Fragment);
            const ShaderCreateInfo *cold_ms =
                cold_ctx->GetStageShader(ShaderStage::Mesh);
            if (!cold_fs || !cold_fs->HasSPVData() || !cold_ms || !cold_ms->HasSPVData())
            {
                result.diagnostics.emplace_back(label + ": cold build produced no SPIR-V");
                continue;
            }

            // 冷构建必须真的记账为 miss——不然「命中」这条断言恒真，什么也没测到。
            if (cold_ctx->UsedCachedFragmentStage() || cold_ctx->UsedCachedMeshStage())
            {
                result.diagnostics.emplace_back(
                    label + ": cold build reported a cache hit on an empty store");
                continue;
            }

            const std::string cold_frag_glsl = cold_fs->GetFinalGLSL();
            const std::string cold_mesh_glsl = cold_ms->GetFinalGLSL();
            const hgl::uint64 cold_frag_digest =
                ComputeGeneratedStageSourceDigest(cold_frag_glsl.data(), cold_frag_glsl.size());

            // ── ② 重开 store，同目录 ⇒ 必须命中且 SPV 一致 ────────────────
            std::unique_ptr<ShaderBuildContext> warm_ctx;
            {
                ShaderArtifactStore store(cache_root_os, ShaderCacheMode::BuildIfMissing);
                request.shader_artifact_store = &store;

                InstallDomainShaderBuildBackend();
                ShaderProgramBuildBackend *backend = GetShaderProgramBuildBackend();
                if (!backend)
                {
                    InstallDomainShaderBuildBackend();
                    result.diagnostics.emplace_back(label + ": domain backend slot empty (warm)");
                    continue;
                }

                warm_ctx.reset(backend->CreateBuildContext(nullptr, definition, request));
                if (!warm_ctx)
                {
                    const char *err = GetLastDomainShaderBuildBackendError();
                    InstallDomainShaderBuildBackend();
                    result.diagnostics.emplace_back(
                        label + ": warm build failed: "
                        + (err && err[0] ? err : "unknown error"));
                    continue;
                }
                if (!backend->FinalizeBuildContext(warm_ctx.get()))
                {
                    InstallDomainShaderBuildBackend();
                    result.diagnostics.emplace_back(label + ": warm finalize failed");
                    continue;
                }
                InstallDomainShaderBuildBackend();
            }

            const ShaderCreateInfo *warm_fs =
                warm_ctx->GetStageShader(ShaderStage::Fragment);
            const ShaderCreateInfo *warm_ms =
                warm_ctx->GetStageShader(ShaderStage::Mesh);
            if (!warm_fs || !warm_fs->HasSPVData() || !warm_ms || !warm_ms->HasSPVData())
            {
                result.diagnostics.emplace_back(label + ": warm build produced no SPIR-V");
                continue;
            }
            if (!warm_ctx->UsedCachedFragmentStage())
            {
                result.diagnostics.emplace_back(
                    label + ": fragment stage missed on a warm store"
                    " (domain master shader is not being baked)");
                continue;
            }
            if (!warm_ctx->UsedCachedMeshStage())
            {
                result.diagnostics.emplace_back(label + ": mesh stage missed on a warm store");
                continue;
            }
            if (warm_fs->GetSPVSize() != cold_fs->GetSPVSize()
             || std::memcmp(warm_fs->GetSPVData(), cold_fs->GetSPVData(),
                            cold_fs->GetSPVSize()) != 0)
            {
                char detail[192];
                std::snprintf(detail, sizeof(detail),
                              ": warm fragment SPIR-V differs from cold (cold=%zu warm=%zu)",
                              cold_fs->GetSPVSize(), warm_fs->GetSPVSize());
                result.diagnostics.emplace_back(label + detail);
                continue;
            }

            // ── ③ 磁盘上的 stage digest / 元数据摘要必须自洽 ──────────────
            {
                ShaderArtifactStore verify_store(cache_root_os, ShaderCacheMode::ReadOnly);

                hgl::ValueArray<uint8_t> stored_spv;
                hgl::uint64 stored_digest = 0;
                if (!verify_store.LoadStageSPV(
                        cold_ctx->GetProgramLink().fragment_stage,
                        stored_spv, &stored_digest))
                {
                    result.diagnostics.emplace_back(
                        label + ": fragment stage SPV not present on disk");
                    continue;
                }
                if (stored_digest != cold_frag_digest)
                {
                    result.diagnostics.emplace_back(
                        label + ": on-disk fragment source digest does not describe"
                        " the domain master shader that was compiled");
                    continue;
                }

                ShaderProgramArtifactMetadata metadata{};
                if (!verify_store.LoadProgramMetadata(
                        cold_ctx->GetProgramLink(), metadata))
                {
                    result.diagnostics.emplace_back(
                        label + ": program metadata not present on disk");
                    continue;
                }

                // 换源后不重算元数据 ⇒ 这里比的就是生成器那份没参与编译的文本。
                const hgl::uint64 expected_source_digest = ComputeGeneratedSourceDigest(
                    cold_mesh_glsl.data(), cold_mesh_glsl.size(),
                    cold_frag_glsl.data(), cold_frag_glsl.size());
                if (metadata.generated_source_digest != expected_source_digest)
                {
                    char detail[224];
                    std::snprintf(detail, sizeof(detail),
                                  ": program metadata generated_source_digest=%llu"
                                  " does not match the compiled sources (%llu)",
                                  static_cast<unsigned long long>(
                                      metadata.generated_source_digest),
                                  static_cast<unsigned long long>(
                                      expected_source_digest));
                    result.diagnostics.emplace_back(label + detail);
                    continue;
                }

                // ── ④ 落盘的 .frag 就是固定主着色器 ──────────────────────
                // `GetStagePath()` 是私有的，所以按契约（stage 与 .frag 同目录、
                // 同主文件名、只换扩展名）去 `shader-cache/stage/` 里找。每个
                // 形状独占一个目录，故这里恰好只有一个 `.frag`。
                bool found_frag_source = false;
                bool has_options_include = false;
                {
                    std::filesystem::path stage_dir(cache_root);
                    stage_dir /= ShaderArtifactCacheDirectory;
                    stage_dir /= ShaderArtifactStageDirectory;

                    std::error_code iter_ec;
                    for (std::filesystem::directory_iterator it(stage_dir, iter_ec), end;
                         !iter_ec && it != end; it.increment(iter_ec))
                    {
                        if (it->path().extension()
                            != GetShaderArtifactStageSourceExtension(ShaderStage::Fragment))
                            continue;

                        found_frag_source = true;

                        std::ifstream source_stream(it->path(), std::ios::binary);
                        if (!source_stream)
                            continue;
                        const std::string text(
                            (std::istreambuf_iterator<char>(source_stream)),
                            std::istreambuf_iterator<char>());
                        has_options_include =
                            text.find("generated/frag_options.glsl") != std::string::npos;
                    }
                }

                if (!found_frag_source)
                {
                    result.diagnostics.emplace_back(
                        label + ": no cached .frag source next to the stage SPV");
                    continue;
                }
                if (!has_options_include)
                {
                    result.diagnostics.emplace_back(
                        label + ": cached .frag source does not include"
                        " generated/frag_options.glsl (a generated shader was baked"
                        " instead of the fixed domain master)");
                    continue;
                }
            }

            char ok[160];
            std::snprintf(ok, sizeof(ok),
                          "%s: cold miss -> warm hit, spv=%zu bytes",
                          label.c_str(), warm_fs->GetSPVSize());
            std::fprintf(stdout, "[INFO] DH.domain-backend-cache %s\n", ok);

            if (digest_count < kCacheShapeCount)
            {
                digest_labels[digest_count] = label;
                // 片段阶段主着色器文本对同形状是逐字节相同的（选项住在
                // generated/frag_options.glsl），所以「选项有没有进身份」只能看
                // 文本之外的 compile_inputs_hash。这也是阶段 4 之后唯一的口径：
                // 文本摘要 = definition_hash，选项/依赖 = compile_inputs_hash。
                digest_values[digest_count] =
                    cold_ctx->GetProgramLink().fragment_stage.compile_inputs_hash;
                ++digest_count;
            }
        }

        for (size_t i = 0; i < digest_count; ++i)
            for (size_t j = i + 1; j < digest_count; ++j)
                if (digest_values[i] == digest_values[j])
                    result.diagnostics.emplace_back(
                        digest_labels[i] + " and " + digest_labels[j]
                        + ": identical fragment stage compile-inputs digest — the"
                          " domain options are not covered by the key, so two"
                          " different option sets can silently reuse one SPV");

        result.passed = result.diagnostics.empty();
        return result;
    }

    struct CacheIdentityEmitShape
    {
        const char *label;
        const char *definition_id;
        const GeometryVertexFormat *geometry;
        PrimitiveType primitive_type = PrimitiveType::Triangles;
    };

    static const CacheIdentityEmitShape kCacheIdentityEmitShapes[] =
    {
        { "cache-lit-abi-a",   "Lit",         &kCacheIdentityLitGeometryA },
        { "cache-lit-abi-b",   "Lit",         &kCacheIdentityLitGeometryB },
        { "cache-lit-abi-std", "Lit",         &kCacheIdentityLitGeometryStandard },
        { "cache-color",       "VertexColor", &kCacheIdentityColorGeometry },
        { "load-geometry-wire", "builtin/pure_color",
          &kCacheIdentityPureColorGeometry, PrimitiveType::Lines },
        { "plane-grid-luminance-wire", "VertexLuminance",
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
                     "[missing] %s: 网格阶段文本在 ShaderLibrary/mesh/shape 下没有"
                     "对应 master（形状 master 需人工按形状新增）\n",
                     label);
        return false;
    }

    /// `emit-fixed-mesh` 模式：一次跑完三张矩阵，逐形状点名网格阶段是否已有
    /// 内容一致的形状 master。
    ///
    /// 为什么这个探针放在门禁里而不是生产回归工具里：形状由**请求**决定
    /// （几何语义集合、位置格式、节点配置、图元类型），而矩阵只在这里维护一份。
    /// 在别处复现同一张矩阵，等于把「哪个形状需要覆盖」抄两份，抄歪了不会报错，
    /// 只会静默少覆盖一个形状。
    ///
    /// 三张矩阵都要走：
    ///   · `kReflectionCoverageVariants`——反射门逐 stage 编译的变体；
    ///   · `kDomainBackendEquivShapes`——领域后端等价门逐域的形状。
    ///     后者的几何是**由领域契约推导**的，与前者手写的几何不是同一份请求，
    ///     形状自然也不同（`builtin/error_checkerboard_3d` 就是这么漏掉的：
    ///     它只出现在领域后端矩阵里，第一次跑「缺源即失败」才现形）；
    ///   · `kCacheIdentityEmitShapes`——缓存身份 ABI 与 LoadGeometry 线框形状。
    ///
    /// 本探针**只读**。七-2 之后形状 master 是按形状命名、人工维护的物理源，
    /// 不再有「按哈希键把生成结果落盘」这件事；留着写盘分支只会把
    /// `mesh/fixed/` 那套哈希命名重新养出来。
    static int EmitFixedMeshCatalog()
    {
        std::string preset_diagnostic;
        if (!EnsureSamplerPresetsLoaded(preset_diagnostic))
        {
            std::fprintf(stdout, "[emit-fixed-mesh] %s\n", preset_diagnostic.c_str());
            return 1;
        }

        std::vector<std::string> catalog;
        std::string catalog_diagnostic;
        if (!LoadFixedMeshCatalog(catalog, catalog_diagnostic))
        {
            std::fprintf(stdout, "[emit-fixed-mesh] %s\n", catalog_diagnostic.c_str());
            return 1;
        }

        int covered = 0;
        int missing = 0;
        int failed = 0;

        for (const ReflectionCoverageVariant &variant : kReflectionCoverageVariants)
        {
            std::string build_diagnostic;
            std::unique_ptr<ShaderBuildContext> ctx =
                BuildReflectionCoverageVariant(
                    variant, build_diagnostic, nullptr, nullptr);
            if (!ctx)
            {
                std::fprintf(stdout, "[emit-fixed-mesh] %s: %s\n",
                          variant.variant_slug, build_diagnostic.c_str());
                ++failed;
                continue;
            }

            if (!CheckOneFixedMeshSource(variant.variant_slug, *ctx, catalog,
                                        covered, missing))
                ++failed;
        }

        // ── 领域后端矩阵 ────────────────────────────────────────────────
        //
        // 这条路径不吃 `fixed_fragment_source`（片段由领域后端自己解析主着色器），
        // 所以这里只取网格阶段。`AssembleMaterialProgram` 在这里的作用等价于
        // 「按请求生成网格阶段」，片段侧产出什么无关紧要。
        for (const DomainBackendEquivShape &shape : kDomainBackendEquivShapes)
        {
            MaterialDefinition definition{};
            if (!TryGetMaterialDefinitionByID(shape.definition_id, definition))
            {
                std::fprintf(stdout, "[emit-fixed-mesh] %s: definition unavailable\n",
                             shape.label);
                ++failed;
                continue;
            }

            GeometryVertexFormat geometry;
            MaterialDefinitionBuildRequest request{};
            std::string build_diagnostic;
            if (!BuildDomainEquivRequest(
                    shape, definition, geometry, request, build_diagnostic))
            {
                std::fprintf(stdout, "[emit-fixed-mesh] %s: %s\n", shape.label,
                          build_diagnostic.c_str());
                ++failed;
                continue;
            }

            // 只需要网格阶段文本，所以**不** finalize：领域后端在 finalize 阶段
            // 才编译固定片段（include 根要按 options 域就位），捕获用不到。
            request.defer_finalize = true;

            // 片段侧同样是物理固定的：`AssembleMaterialProgram` 在没有
            // `fixed_fragment_source` 时会直接拒绝。这里照抄领域后端的做法先解析
            // 固定片段源——否则捕获路径根本走不到网格阶段。
            FixedFragmentSource fixed;
            if (!ResolveFixedFragmentSource(definition, request, fixed))
            {
                std::fprintf(stdout, "[emit-fixed-mesh] %s: %s\n", shape.label,
                             GetLastDomainShaderBuildBackendError());
                ++failed;
                continue;
            }
            FixedStageInputs fixed_stage;
            fixed_stage.fragment_source = fixed.source.c_str();
            fixed_stage.fragment_interface_hash = fixed.interface_hash;
            fixed_stage.fragment_options_digest = fixed.options_digest;

            MaterialProgramBuildCapture capture{};
            std::unique_ptr<ShaderBuildContext> ctx(
                AssembleMaterialProgram(nullptr, request, definition, &capture, fixed_stage));
            if (!ctx)
            {
                const hgl::AnsiString &build_error =
                    GetLastMaterialProgramAssemblyError();
                std::fprintf(stdout, "[emit-fixed-mesh] %s: %s\n", shape.label,
                          build_error.IsEmpty()
                              ? "(no diagnostic)" : build_error.c_str());
                ++failed;
                continue;
            }

            if (!CheckOneFixedMeshSource(shape.label, *ctx, catalog,
                                        covered, missing))
                ++failed;
        }

        // ── Q0 缓存身份矩阵 ────────────────────────────────────────────
        //
        // 形状键与模板/profile 无关（只吃顶点节点配置、varying、位置格式、
        // mesh 模式、invocation 上限），所以这里让路由器选模板即可，不必复现
        // 那条用例手选的模板 ID；几何则**必须**复用同一批常量。
        for (const CacheIdentityEmitShape &shape : kCacheIdentityEmitShapes)
        {
            MaterialDefinition definition{};
            if (!TryGetMaterialDefinitionByID(shape.definition_id, definition))
            {
                std::fprintf(stdout, "[emit-fixed-mesh] %s: definition unavailable\n",
                             shape.label);
                ++failed;
                continue;
            }

            MaterialDefinitionBuildRequest request{};
            request.recipe.mtl_def_id = definition.definition_id;
            request.geometry_vertex_format = shape.geometry;
            request.primitive_type = shape.primitive_type;
            request.shader_program_purpose = ShaderProgramPurpose::ForwardColor;
            request.defer_finalize = true;

            if (!SelectSceneRenderTemplateRequest(
                    definition, ShaderProgramPurpose::ForwardColor, request.recipe,
                    request.render_template_request, nullptr)
             || !AppendMaterialRenderTemplateRoots(
                    definition, request.render_template_request))
            {
                std::fprintf(stdout, "[emit-fixed-mesh] %s: template request failed\n",
                             shape.label);
                ++failed;
                continue;
            }

            FixedFragmentSource fixed;
            if (!ResolveFixedFragmentSource(definition, request, fixed))
            {
                std::fprintf(stdout, "[emit-fixed-mesh] %s: %s\n", shape.label,
                             GetLastDomainShaderBuildBackendError());
                ++failed;
                continue;
            }
            FixedStageInputs fixed_stage;
            fixed_stage.fragment_source = fixed.source.c_str();
            fixed_stage.fragment_interface_hash = fixed.interface_hash;
            fixed_stage.fragment_options_digest = fixed.options_digest;

            MaterialProgramBuildCapture capture{};
            std::unique_ptr<ShaderBuildContext> ctx(
                AssembleMaterialProgram(nullptr, request, definition, &capture, fixed_stage));
            if (!ctx)
            {
                const hgl::AnsiString &build_error =
                    GetLastMaterialProgramAssemblyError();
                std::fprintf(stdout, "[emit-fixed-mesh] %s: %s\n", shape.label,
                             build_error.IsEmpty()
                                 ? "(no diagnostic)" : build_error.c_str());
                ++failed;
                continue;
            }

            if (!CheckOneFixedMeshSource(shape.label, *ctx, catalog,
                                        covered, missing))
                ++failed;
        }

        std::fprintf(stdout,
                     "[emit-fixed-mesh] covered=%d missing=%d failed=%d\n",
                     covered, missing, failed);
        return (failed == 0 && missing == 0) ? 0 : 1;
    }

}

int main(const int argc, char **argv)
{
    // 发行形态夹具烘焙模式：argc==3 且 argv[1]=="emit-release-cache"。
    // 产物（manifest + 两 stage SPV + expectation.txt）留给 RuntimeFormProbe
    // 在 ULRE_ENABLE_SHADERGEN=0 构建里消费。
    if (argc == 3 && std::strcmp(argv[1], "emit-release-cache") == 0)
    {
        if (!InitShaderCompiler())
        {
            GLogError(
                "[ShaderResourceSchemaRegressionGate] Failed to initialize GLSL compiler.");
            return 3;
        }

        const std::filesystem::path cache_root =
            std::filesystem::absolute(std::filesystem::path(argv[2]));
        GateResult manifest_result = RunProgramManifestLookupCase(cache_root, true);

        std::fprintf(manifest_result.passed ? stdout : stderr,
                     "[%s] %s\n",
                     manifest_result.passed ? "PASS" : "FAIL",
                     manifest_result.name.c_str());

        for (const auto &diag : manifest_result.diagnostics)
            std::fprintf(stderr, "  - %s\n", diag.c_str());

        // compute 夹具烘焙进同一个根：材质清单与 compute 清单是两份文件，
        // 互不覆盖，发行形态一次打包全部带走。
        if (manifest_result.passed)
        {
            GateResult compute_result = RunComputeProgramCacheCase(cache_root, true);

            std::fprintf(compute_result.passed ? stdout : stderr,
                         "[%s] %s\n",
                         compute_result.passed ? "PASS" : "FAIL",
                         compute_result.name.c_str());

            for (const auto &diag : compute_result.diagnostics)
                std::fprintf(stderr, "  - %s\n", diag.c_str());

            if (!compute_result.passed)
                return 1;
        }

        if (manifest_result.passed)
        {
            hgl::OSString cache_dir;
            if (ResolveEmittedCacheDir(cache_root, cache_dir))
                std::fprintf(stdout, "[INFO] release cache emitted to %s\n",
                             std::filesystem::path(cache_dir.c_str())
                                 .string().c_str());
            else
                std::fprintf(stdout, "[INFO] release cache emitted under %s\n",
                             cache_root.string().c_str());
        }

        return manifest_result.passed ? 0 : 1;
    }

    if (argc > 2)
    {
        GLogError(
            "[ShaderResourceSchemaRegressionGate] Expected zero or one group argument.");
        return 2;
    }

    const char *selected_group = argc == 2 ? argv[1] : "all";

    // `emit-fixed-mesh` 不是回归组，而是覆盖率探针：一次跑完三张矩阵，
    // 逐形状点名 `ShaderLibrary/mesh/shape` 下有没有对应的形状 master。
    // 它与门禁共用同一张变体表，形状定义只有一份——覆盖检查与验证不可能互相漂移。
    if (std::strcmp(selected_group, "emit-fixed-mesh") == 0)
    {
        if (!InitShaderCompiler())
        {
            GLogError(
                "[ShaderResourceSchemaRegressionGate] Failed to initialize GLSL compiler.");
            return 3;
        }
        return EmitFixedMeshCatalog();
    }

    if (!IsKnownRegressionGroup(selected_group))
    {
        GLogError(
            "[ShaderResourceSchemaRegressionGate] Unknown regression group: %s",
            selected_group);
        return 2;
    }

    if (!InitShaderCompiler())
    {
        GLogError(
            "[ShaderResourceSchemaRegressionGate] Failed to initialize GLSL compiler.");
        return 3;
    }

    const bool run_glsl = IsRegressionGroupSelected(selected_group, "glsl");
    const bool run_interface = IsRegressionGroupSelected(selected_group, "interface");
    const bool run_descriptor = IsRegressionGroupSelected(selected_group, "descriptor");
    const bool run_cache = IsRegressionGroupSelected(selected_group, "cache");
    const bool run_materialization =
        IsRegressionGroupSelected(selected_group, "materialization");
    const bool run_pipeline = IsRegressionGroupSelected(selected_group, "pipeline");
    const bool run_reflection = IsRegressionGroupSelected(selected_group, "reflection");
    const bool run_module_invariants =
        IsRegressionGroupSelected(selected_group, "module-invariants");

    std::vector<GateResult> results;

    if (run_descriptor)
    {
    }

    if (run_materialization) results.push_back(RunMaterializationSharedInstanceCase());
    if (run_materialization) results.push_back(RunMaterialRegistryCoverageCase());
    if (run_materialization) results.push_back(RunMaterialDefinitionIdentityCase());
    if (run_materialization) results.push_back(RunMaterialFallbackDefinitionCase());
    if (run_pipeline) results.push_back(RunUnifiedMaterialBaselineCase());
    if (run_pipeline) results.push_back(RunMaterialOutputContractCase());
    if (run_pipeline) results.push_back(RunPrimitiveVariantPurposeCase());
    if (run_pipeline) results.push_back(RunUnifiedForwardSkeletonCase());
    if (run_pipeline) results.push_back(RunUnifiedPureColorFragmentCase());
    if (run_descriptor) results.push_back(RunUnifiedMaterialContractCase());
    if (run_materialization) results.push_back(RunTransformGraphModelCase());
    if (run_materialization) results.push_back(RunTransformGraphCompositionCase());
    if (run_cache) results.push_back(RunAuthoritativeMaterialCacheIdentityCase());
    if (run_descriptor) results.push_back(RunGlobalSSBOBindingKeyCase());
    if (run_materialization) results.push_back(RunResolvedMaterialRenderStateCase());
    if (run_materialization) results.push_back(RunMaterialDefinitionFileSchemaCase());
    if (run_materialization) results.push_back(RunMaterialDefinitionParseDetailCase());
    if (run_materialization) results.push_back(RunMaterialDomainContractCase());
    if (run_materialization) results.push_back(RunMaterialLibraryContractCase());
    if (run_materialization) results.push_back(RunFallbackInferenceCase());
    if (run_glsl) results.push_back(RunProviderResourceManifestCase());
    if (run_glsl) results.push_back(RunShaderCodeModuleFileCase());
    if (run_glsl) results.push_back(RunShaderCodeModuleMetadataValidationCase());
    if (run_interface) results.push_back(RunShaderSemanticRegistryCase());
    if (run_interface) results.push_back(RunDepthVaryingMaskParityCase());
    if (run_interface) results.push_back(RunCharQuadWorkgroupContractCase());
    if (run_interface) results.push_back(RunDrawTriangleVertexColor2DCase());
    if (run_interface) results.push_back(RunMaterialVertexABICharacterizationCase());
    if (run_interface) results.push_back(RunMaterialSemanticABIParityCase());
    if (run_interface) results.push_back(RunGlobalAddressesStructParityCase());
    if (run_interface) results.push_back(RunWorldAddressesStructParityCase());
    if (run_interface) results.push_back(RunEntityGPUStructParityCase());
    if (run_interface) results.push_back(RunLightRowStructParityCase());
    if (run_interface) results.push_back(RunBatchInfoStructParityCase());
    if (run_interface) results.push_back(RunMeshDrawParamsHeadParityCase());
    if (run_interface) results.push_back(RunAddressOwnershipCase(false));
    if (run_interface) results.push_back(RunAddressOwnershipCase(true));
    if (run_glsl) results.push_back(RunVkEnumTextCase());
    if (run_cache) results.push_back(RunComposedStageInterfaceCase());
    if (run_cache) results.push_back(RunResolvedStageCacheIdentityCase());
    if (run_cache) results.push_back(RunCanonicalShaderContractCase());
    if (run_cache) results.push_back(RunProgramManifestCase());
    if (run_cache) results.push_back(
        RunProgramManifestLookupCase(
            std::filesystem::path(RepoRootOSPath("build/cache-manifest-lookup").c_str())));
    if (run_cache) results.push_back(
        RunComputeProgramCacheCase(
            std::filesystem::path(RepoRootOSPath("build/cache-compute-lookup").c_str())));
    if (run_pipeline) results.push_back(RunMaterialBDAFixedBindingsCase());
    if (run_pipeline) results.push_back(RunBindingMacroSingleSourceCase());
    if (run_pipeline) results.push_back(RunShaderLibraryPathCase());
    if (run_reflection) results.push_back(RunSPVReflectionCase());
    if (run_reflection) results.push_back(RunSPVBufferReferenceReflectionCase());
    if (run_reflection) results.push_back(RunGeneratedInterfaceReflectionCase());
    if (run_materialization) results.push_back(RunSamplerPresetLibraryCase());
    if (run_interface) results.push_back(RunFixedMeshVaryingInvariantsCase());
    if (run_module_invariants) results.push_back(RunModuleInvariantsCase());
    if (run_module_invariants) results.push_back(RunSamplerReferenceContractCase());
    if (run_module_invariants) results.push_back(RunModuleIncludeDeclarationContractCase());
    if (run_pipeline) results.push_back(RunDomainBackendEquivalenceCase());
    if (run_pipeline) results.push_back(RunDomainBackendCacheCase());

    bool all_passed = true;
    for (const auto &result : results)
    {
        std::fprintf(result.passed ? stdout : stderr,
                     "[%s] %s\n",
                     result.passed ? "PASS" : "FAIL",
                     result.name.c_str());

        if (!result.passed)
        {
            all_passed = false;
            for (const auto &diag : result.diagnostics)
                std::fprintf(stderr, "  - %s\n", diag.c_str());
        }
    }

    return all_passed ? 0 : 1;
}
