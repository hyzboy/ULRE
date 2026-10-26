/// VertexLoaderConsistency — 顶点流装载入口一致性校验
///
/// 背景：`s1_position_{vec2,vec2i,vec3}.glsl` 的 `LoadVertexData()` 无条件顺序调用
/// 各数据流入口 `LoadXxx()`；未装载该流的材质由 `vertex/s1_stream_defaults.glsl`
/// 用同名空函数补齐（空对象模式）。**漏调一个 LoadXxx() = 该属性静默不赋值**
/// （几何/其它属性正常，只有该属性为 0）——历史事故：AutoInstance 全黑 =
/// s1_position_vec2 漏了顶点色装载。
///
/// 旧实现在预处理期用 `#ifdef HGL_*_LOADER` 守卫展开装载宏，那批宏已全部删除；
/// 本工具随之改为校验新的三边关系，规则**全部从 GLSL 文本推导，不维护任何
/// 平行清单**（位置模块名单除外——它本就是「谁在调用」的登记处）：
///
///   ① 每个流模块（定义 `void LoadXxx()` 的文件）必须自述且仅自述一个
///      `#define S1_STREAM_<NAME>` 守卫；同守卫的多个提供者（如 s1_color 与
///      s1_palette_index 互斥二选一）必须提供**同名**入口函数。
///   ② `s1_stream_defaults.glsl` 必须为每个守卫提供 `#ifndef <守卫>` +
///      同名空函数；且不得留下无人提供的死 stub。
///   ③ 三个位置模块的 `LoadXxx();` 调用序列必须**同集合、同顺序**——
///      这条就是替代旧 `#ifdef` 对称性检查的那一条。
///   ④ 位置模块调用的每个入口都必须在 defaults 里有 stub（漏 stub 时缺该流的
///      材质会编译失败而不是静默为 0，但漏 stub 本身仍是错误）。
///
/// 用法：VertexLoaderConsistency <ShaderLibrary/vertex 目录>
/// 已接入 ctest（VerifyVertexLoaderConsistency）。

#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <algorithm>

namespace
{
    // 位置模块（流入口调用点）——文件名固定，新增位置模块须在此登记
    constexpr const char *POSITION_MODULES[] =
    {
        "s1_position_vec2.glsl",
        "s1_position_vec2i.glsl",
        "s1_position_vec3.glsl",
    };

    // 缺流空实现模块（stub 提供点）
    constexpr const char *DEFAULTS_MODULE = "s1_stream_defaults.glsl";

    // 流在位守卫前缀（数据模块自述，defaults 据此决定要不要补 stub）
    constexpr const char *STREAM_GUARD_PREFIX = "S1_STREAM_";

    // 入口函数名前缀（`void LoadXxx()` / `LoadXxx();`）
    constexpr const char *LOAD_ENTRY_PREFIX = "Load";

    struct StreamDecl
    {
        std::string guard;   ///< S1_STREAM_XXX
        std::string entry;   ///< LoadXxx
        std::string file;    ///< 提供者文件名
    };

    struct StreamStub
    {
        std::string guard;
        std::string entry;
    };

    /// 单文件扫描结果（四个桶全部由文本推导，不做正则）
    struct FileScan
    {
        std::vector<std::string> guards;   ///< `#define S1_STREAM_XXX`
        std::vector<std::string> entries;  ///< `void LoadXxx(` 定义
        std::vector<std::string> calls;    ///< `LoadXxx();` 语句（保持出现顺序）
        std::vector<StreamStub> stubs;     ///< `#ifndef S1_STREAM_XXX` 之后紧跟的入口
    };

    bool ReadTextFile(const std::filesystem::path &path, std::string &out)
    {
        std::ifstream in(path, std::ios::binary);
        if (!in)
            return false;

        out.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
        return true;
    }

    bool StartsWith(const std::string &text, const char *prefix)
    {
        const size_t length = std::strlen(prefix);
        return text.size() >= length && text.compare(0, length, prefix) == 0;
    }

    bool IsIdentChar(const char c)
    {
        return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
    }

    /// 取 `pos` 起的标识符（pos 必须指向标识符首字符）
    std::string IdentifierAt(const std::string &text, const size_t pos)
    {
        size_t end = pos;
        while (end < text.size() && IsIdentChar(text[end]))
            ++end;

        return text.substr(pos, end - pos);
    }

    /// 去掉行首尾空白（含 CR）
    std::string TrimLine(const std::string &line)
    {
        size_t begin = 0;
        size_t end = line.size();

        while (begin < end && (line[begin] == ' ' || line[begin] == '\t'
                            || line[begin] == '\r' || line[begin] == '\n'))
            ++begin;

        while (end > begin && (line[end - 1] == ' ' || line[end - 1] == '\t'
                            || line[end - 1] == '\r' || line[end - 1] == '\n'))
            --end;

        return line.substr(begin, end - begin);
    }

    /// 从 `#define ` / `#ifndef ` 这类行里取出 `S1_STREAM_XXX` 的 XXX 后缀
    bool ExtractStreamGuard(const std::string &trimmed, const char *directive,
                            std::string &out_guard)
    {
        if (!StartsWith(trimmed, directive))
            return false;

        size_t pos = std::strlen(directive);
        while (pos < trimmed.size() && (trimmed[pos] == ' ' || trimmed[pos] == '\t'))
            ++pos;

        if (!StartsWith(trimmed.substr(pos), STREAM_GUARD_PREFIX))
            return false;

        pos += std::strlen(STREAM_GUARD_PREFIX);
        out_guard = IdentifierAt(trimmed, pos);
        return !out_guard.empty();
    }

    void ScanFile(const std::string &text, FileScan &out)
    {
        std::string pending_guard;
        size_t cursor = 0;

        while (cursor <= text.size())
        {
            const size_t line_break = text.find('\n', cursor);
            const size_t line_end = line_break == std::string::npos ? text.size() : line_break;
            const std::string trimmed = TrimLine(text.substr(cursor, line_end - cursor));
            cursor = line_end + 1;

            if (!trimmed.empty())
            {
                std::string guard;
                if (ExtractStreamGuard(trimmed, "#define", guard))
                {
                    out.guards.push_back(guard);
                }
                else if (ExtractStreamGuard(trimmed, "#ifndef", guard))
                {
                    pending_guard = guard;
                }
                else if (StartsWith(trimmed, "void "))
                {
                    const std::string entry = IdentifierAt(trimmed, std::strlen("void "));
                    if (StartsWith(entry, LOAD_ENTRY_PREFIX)
                     && entry.size() > std::strlen(LOAD_ENTRY_PREFIX))
                    {
                        out.entries.push_back(entry);

                        // `#ifndef S1_STREAM_XXX` 之后紧跟的入口定义即该守卫的 stub
                        if (!pending_guard.empty())
                        {
                            out.stubs.push_back({pending_guard, entry});
                            pending_guard.clear();
                        }
                    }
                }
                else if (StartsWith(trimmed, LOAD_ENTRY_PREFIX)
                      && trimmed.back() == ';')
                {
                    // 只认整行调用 `LoadXxx();`，避免把别的表达式误判为流入口
                    const std::string entry = IdentifierAt(trimmed, 0);
                    if (trimmed.substr(entry.size()) == "();")
                        out.calls.push_back(entry);
                }
            }

            if (line_break == std::string::npos)
                break;
        }
    }

    bool IsPositionModule(const std::string &filename)
    {
        for (const char *name : POSITION_MODULES)
            if (filename == name)
                return true;

        return false;
    }

    std::string JoinList(const std::vector<std::string> &items)
    {
        std::string joined;
        for (size_t i = 0; i < items.size(); ++i)
        {
            if (i)
                joined += ", ";
            joined += items[i];
        }
        return joined;
    }
}//namespace

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        std::fprintf(stderr,
            "usage: VertexLoaderConsistency <ShaderLibrary/vertex 目录>\n");
        return 3;
    }

    const std::filesystem::path dir(argv[1]);

    if (!std::filesystem::is_directory(dir))
    {
        std::fprintf(stderr,
            "[VertexLoaderConsistency] 目录不存在: %s\n", argv[1]);
        return 2;
    }

    std::vector<StreamDecl> providers;                  // 流模块自述的守卫 + 入口
    std::vector<StreamStub> stubs;                      // defaults 里的缺流空实现
    std::vector<std::vector<std::string>> call_lists;   // 三个位置模块的调用序列
    std::vector<std::string> call_files;
    bool defaults_seen = false;
    int position_module_seen = 0;
    int fail = 0;

    for (const auto &entry : std::filesystem::directory_iterator(dir))
    {
        if (!entry.is_regular_file() || entry.path().extension() != ".glsl")
            continue;

        const std::string filename = entry.path().filename().string();

        std::string text;
        if (!ReadTextFile(entry.path(), text))
        {
            std::fprintf(stderr,
                "[VertexLoaderConsistency] 无法读取: %s\n", filename.c_str());
            return 2;
        }

        FileScan scan;
        ScanFile(text, scan);

        if (IsPositionModule(filename))
        {
            ++position_module_seen;
            call_lists.push_back(scan.calls);
            call_files.push_back(filename);
            continue;
        }

        if (filename == DEFAULTS_MODULE)
        {
            defaults_seen = true;

            // 文件自己的 include guard 也以 S1_STREAM_ 开头，但它同时出现在
            // `#define` 里；真正的流 stub 只有 `#ifndef`（对应 `#define` 在各
            // 流模块里）。用这条差异把 include guard 剔掉。
            for (const StreamStub &stub : scan.stubs)
            {
                const bool is_include_guard =
                    std::find(scan.guards.begin(), scan.guards.end(), stub.guard)
                        != scan.guards.end();
                if (!is_include_guard)
                    stubs.push_back(stub);
            }
            continue;
        }

        if (scan.entries.empty())
            continue;

        // ① 流模块必须自述「一个守卫 + 一个入口」——多守卫或多入口说明两个流
        //    挤在同一文件里，defaults 的 stub 条件会失去意义。
        if (scan.guards.size() != 1 || scan.entries.size() != 1)
        {
            std::fprintf(stderr,
                "[VertexLoaderConsistency] %s：流模块必须恰好 1 个 "
                "#define S1_STREAM_*（实测 %zu）与 1 个 void Load*()（实测 %zu）\n",
                filename.c_str(), scan.guards.size(), scan.entries.size());
            ++fail;
            continue;
        }

        providers.push_back({scan.guards[0], scan.entries[0], filename});
    }

    const int expected_position_modules =
        int(sizeof(POSITION_MODULES) / sizeof(POSITION_MODULES[0]));

    if (position_module_seen != expected_position_modules)
    {
        std::fprintf(stderr,
            "[VertexLoaderConsistency] 位置模块数不符: 实测 %d，登记 %d\n"
            "  → 新增/删除 s1_position_* 后须同步本工具的 POSITION_MODULES\n",
            position_module_seen, expected_position_modules);
        ++fail;
    }

    if (!defaults_seen)
    {
        std::fprintf(stderr,
            "[VertexLoaderConsistency] 缺少缺流空实现模块 %s\n", DEFAULTS_MODULE);
        ++fail;
    }

    // ① 同一守卫的多个提供者必须提供同名入口（s1_color / s1_palette_index 这类互斥二选一）
    for (size_t i = 0; i < providers.size(); ++i)
    {
        for (size_t j = i + 1; j < providers.size(); ++j)
        {
            if (providers[i].guard != providers[j].guard
             || providers[i].entry == providers[j].entry)
                continue;

            std::fprintf(stderr,
                "[VertexLoaderConsistency] 守卫 %s 的提供者提供了不同入口："
                "%s → %s()，%s → %s()\n"
                "  → 位置模块只调用一个入口名，同守卫的提供者必须同签名\n",
                providers[i].guard.c_str(),
                providers[i].file.c_str(), providers[i].entry.c_str(),
                providers[j].file.c_str(), providers[j].entry.c_str());
            ++fail;
        }
    }

    // ② defaults 必须为每个守卫提供同名 stub
    for (const StreamDecl &provider : providers)
    {
        const bool has_stub = std::any_of(stubs.begin(), stubs.end(),
            [&](const StreamStub &stub)
            {
                return stub.guard == provider.guard && stub.entry == provider.entry;
            });

        if (!has_stub)
        {
            std::fprintf(stderr,
                "[VertexLoaderConsistency] %s 自述守卫 %s + %s()，但 %s 没有对应 stub\n"
                "  → 缺该流的材质会编译失败（未定义 %s），在 %s 补 "
                "#ifndef S1_STREAM_%s + void %s() { }\n",
                provider.file.c_str(), provider.guard.c_str(), provider.entry.c_str(),
                DEFAULTS_MODULE, provider.entry.c_str(), DEFAULTS_MODULE,
                provider.guard.c_str(), provider.entry.c_str());
            ++fail;
        }
    }

    // ②b defaults 不得留下无人提供的死 stub
    for (const StreamStub &stub : stubs)
    {
        const bool has_provider = std::any_of(providers.begin(), providers.end(),
            [&](const StreamDecl &provider) { return provider.guard == stub.guard; });

        if (!has_provider)
        {
            std::fprintf(stderr,
                "[VertexLoaderConsistency] %s 的 stub %s（守卫 S1_STREAM_%s）无任何流模块提供\n"
                "  → 删除该 stub，或补回定义 #define S1_STREAM_%s 的数据模块\n",
                DEFAULTS_MODULE, stub.entry.c_str(), stub.guard.c_str(), stub.guard.c_str());
            ++fail;
        }
    }

    // ④ 位置模块调用的入口必须有 stub（缺 stub = 缺该流的材质在编译期炸）
    for (size_t module = 0; module < call_lists.size(); ++module)
    {
        for (const std::string &call : call_lists[module])
        {
            const bool has_stub = std::any_of(stubs.begin(), stubs.end(),
                [&](const StreamStub &stub) { return stub.entry == call; });

            if (!has_stub)
            {
                std::fprintf(stderr,
                    "[VertexLoaderConsistency] %s 调用了 %s()，但 %s 没有该 stub\n",
                    call_files[module].c_str(), call.c_str(), DEFAULTS_MODULE);
                ++fail;
            }
        }
    }

    // ③ 三个位置模块的调用序列必须同集合、同顺序——替代旧 #ifdef 对称性检查
    for (size_t module = 1; module < call_lists.size(); ++module)
    {
        if (call_lists[module] == call_lists[0])
            continue;

        std::fprintf(stderr,
            "[VertexLoaderConsistency] 位置模块调用序列不一致：\n"
            "  %s: %s\n"
            "  %s: %s\n"
            "  → 漏调 = 该属性静默为 0（几何正常、属性错）\n",
            call_files[0].c_str(), JoinList(call_lists[0]).c_str(),
            call_files[module].c_str(), JoinList(call_lists[module]).c_str());
        ++fail;
    }

    if (fail > 0)
    {
        std::fprintf(stderr,
            "[VertexLoaderConsistency] 校验失败 %d 项\n", fail);
        return 1;
    }

    std::vector<std::string> unique_guards;
    for (const StreamDecl &provider : providers)
        if (std::find(unique_guards.begin(), unique_guards.end(), provider.guard)
                == unique_guards.end())
            unique_guards.push_back(provider.guard);

    std::printf(
        "[VertexLoaderConsistency] OK: %zu 个流守卫 × %d 个位置模块，"
        "调用序列一致（%zu 个入口、%zu 个提供模块）\n",
        unique_guards.size(), expected_position_modules,
        stubs.size(), providers.size());

    return 0;
}
