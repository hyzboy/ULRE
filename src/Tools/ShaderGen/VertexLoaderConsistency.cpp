/// VertexLoaderConsistency — s1_* 顶点 loader 宏一致性校验
///
/// 背景：`s1_position_{vec2,vec2i,vec3}.glsl` 用 `#ifdef HGL_X_LOADER` 守卫展开各数据
/// 模块提供的 loader 宏。**漏加一个 #ifdef 块 = 该属性静默不赋值**（几何/其它属性正常，
/// 只有该属性为 0），既无编译错误也无运行期报错——历史事故：AutoInstance 全黑 =
/// s1_position_vec2 漏 HGL_COLOR_LOADER。
///
/// 本工具把该纪律变成 CI 断言，规则**全部从 GLSL 文本推导，不维护任何平行清单**：
///   ① 每个被定义的 loader（某模块 `#define HGL_X_LOADER`）必须在**三个位置模块全部展开**
///   ② 每个被展开的 loader（位置模块 `#ifdef HGL_X_LOADER`）必须有模块定义它
///      （抓死残留——如 s1_joint 删除后遗留的 HGL_JOINT_LOADER）
///
/// 用法：VertexLoaderConsistency <ShaderLibrary/vertex 目录>
/// 已接入 ctest（VerifyVertexLoaderConsistency）。

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <algorithm>

namespace
{
    // 位置模块（loader 展开点）——文件名固定，新增位置模块须在此登记
    constexpr const char *POSITION_MODULES[] =
    {
        "s1_position_vec2.glsl",
        "s1_position_vec2i.glsl",
        "s1_position_vec3.glsl",
    };

    struct LoaderUse
    {
        std::string macro;       ///< HGL_XXX_LOADER
        std::string file;        ///< 出现的文件名
    };

    bool ReadTextFile(const std::filesystem::path &path, std::string &out)
    {
        std::ifstream in(path, std::ios::binary);
        if (!in)
            return false;

        out.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
        return true;
    }

    /// 提取形如 <prefix>HGL_XXX_LOADER 的宏名（prefix = "#define " 或 "#ifdef "）
    void CollectMacros(const std::string &text,
                       const char *prefix,
                       const std::string &filename,
                       std::vector<LoaderUse> &out)
    {
        const size_t prefix_len = std::strlen(prefix);
        size_t pos = 0;

        while ((pos = text.find(prefix, pos)) != std::string::npos)
        {
            size_t begin = pos + prefix_len;

            while (begin < text.size() && (text[begin] == ' ' || text[begin] == '\t'))
                ++begin;

            size_t end = begin;
            while (end < text.size()
                && (std::isalnum(static_cast<unsigned char>(text[end])) || text[end] == '_'))
                ++end;

            const std::string macro = text.substr(begin, end - begin);

            // 只关心 HGL_*_LOADER
            if (macro.rfind("HGL_", 0) == 0
             && macro.size() > 7
             && macro.compare(macro.size() - 7, 7, "_LOADER") == 0)
            {
                const bool duplicate =
                    std::any_of(out.begin(), out.end(),
                        [&](const LoaderUse &u)
                        { return u.macro == macro && u.file == filename; });

                if (!duplicate)
                    out.push_back({macro, filename});
            }

            pos = end;
        }
    }

    bool IsPositionModule(const std::string &filename)
    {
        for (const char *name : POSITION_MODULES)
            if (filename == name)
                return true;

        return false;
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

    std::vector<LoaderUse> defined;    // #define HGL_X_LOADER（数据模块提供）
    std::vector<LoaderUse> expanded;   // #ifdef  HGL_X_LOADER（位置模块展开）
    int position_module_seen = 0;

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

        CollectMacros(text, "#define ", filename, defined);

        if (IsPositionModule(filename))
        {
            ++position_module_seen;
            CollectMacros(text, "#ifdef ", filename, expanded);
        }
    }

    const int expected_position_modules =
        int(sizeof(POSITION_MODULES) / sizeof(POSITION_MODULES[0]));

    int fail = 0;

    if (position_module_seen != expected_position_modules)
    {
        std::fprintf(stderr,
            "[VertexLoaderConsistency] 位置模块数不符: 实测 %d，登记 %d\n"
            "  → 新增/删除 s1_position_* 后须同步本工具的 POSITION_MODULES\n",
            position_module_seen, expected_position_modules);
        ++fail;
    }

    // ① 已定义的 loader 必须在三个位置模块全部展开
    for (const LoaderUse &def : defined)
    {
        for (const char *pos_module : POSITION_MODULES)
        {
            const bool found =
                std::any_of(expanded.begin(), expanded.end(),
                    [&](const LoaderUse &u)
                    { return u.macro == def.macro && u.file == pos_module; });

            if (!found)
            {
                std::fprintf(stderr,
                    "[VertexLoaderConsistency] %s 定义于 %s，但 %s 未展开\n"
                    "  → 漏展开 = 该属性静默为 0（几何正常、属性错），"
                    "在 %s 的 LoadVertexData 补 #ifdef 块\n",
                    def.macro.c_str(), def.file.c_str(), pos_module, pos_module);
                ++fail;
            }
        }
    }

    // ② 被展开的 loader 必须有模块定义（抓删模块后的死残留）
    for (const LoaderUse &use : expanded)
    {
        const bool has_definition =
            std::any_of(defined.begin(), defined.end(),
                [&](const LoaderUse &d) { return d.macro == use.macro; });

        if (!has_definition)
        {
            std::fprintf(stderr,
                "[VertexLoaderConsistency] %s 在 %s 展开，但无任何模块定义它（死残留）\n"
                "  → 删除该 #ifdef 块，或补回提供该宏的数据模块\n",
                use.macro.c_str(), use.file.c_str());
            ++fail;
        }
    }

    if (fail > 0)
    {
        std::fprintf(stderr,
            "[VertexLoaderConsistency] 校验失败 %d 项\n", fail);
        return 1;
    }

    // 汇总（去重后的 loader 名单）
    std::vector<std::string> unique_macros;
    for (const LoaderUse &def : defined)
        if (std::find(unique_macros.begin(), unique_macros.end(), def.macro)
                == unique_macros.end())
            unique_macros.push_back(def.macro);

    std::printf(
        "[VertexLoaderConsistency] OK: %zu 个 loader × %d 个位置模块全部对称\n",
        unique_macros.size(), expected_position_modules);

    return 0;
}
