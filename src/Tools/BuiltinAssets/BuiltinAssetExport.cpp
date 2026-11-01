/**
 * @file BuiltinAssetExport.cpp
 * @brief 内置元素（预设）资产的离线导出工具 —— **开发期生成器**，不进发行包。
 *
 * 为什么单独一个工程：
 *  - 这里定义的是**作者侧产物**（`ShaderLibrary/asset/...` 下的预设文件），引擎只读不写；
 *  - 生成器代码对运行时毫无用处，却会因为链进运行时库而出现在发行产物里；
 *  - 未来所有内置元素（几何预设、材质预设…）都往这里加一组，避免散落在各运行库里。
 *
 * 落盘一律走 `hgl::serialize` 统一通路：`AssetObject::Serialize()` →
 * `Archive` → `Value` → 按文件名后缀选 `TomlCodec` / `FlatCodec`（见 `ArchiveFile.h`）。
 * 本工具**不**手写任何 TOML 文本或 FlatBuffers 二进制。
 *
 * 用法：
 * ```
 * BuiltinAssetExport                       # 默认输出到 ShaderLibrary/asset
 * BuiltinAssetExport --out ShaderLibrary/asset
 * BuiltinAssetExport --toml-only           # 只写 .toml（作者侧改完再生成二进制时可分批）
 * BuiltinAssetExport --flat-only
 * ```
 *
 * 每个分组都会拿导出数量与声明的期望值比对：漏一份就算失败（CI 友好）。
 */

#include "EnvironmentPresets.h"
#include "PresetWriter.h"

#include <hgl/log/Log.h>
#include <hgl/log/Logger.h>
#include <hgl/type/String.h>
#include <hgl/utf.h>

namespace
{
    constexpr const os_char *kDefaultOutputDir = OS_TEXT("ShaderLibrary/asset");

    /**
     * 一个内置元素分组。
     *
     * 加新分组（几何 / 材质 / 场景…）只需：写一个 `int Export<X>Presets(PresetWriter&)`
     * 并在 `kGroups` 里加一行 —— `main()` 不需要改动。
     */
    struct PresetGroup
    {
        const char *id;                             ///<命令行/日志里的分组标识
        int         expected_count;                 ///<期望导出的预设数量（不符即失败）
        int       (*export_fn)(hgl::graph::builtin::PresetWriter &writer);
    };

    constexpr PresetGroup kGroups[] =
    {
        { "environment", hgl::graph::builtin::kEnvironmentPresetCount,
          &hgl::graph::builtin::ExportEnvironmentPresets },
    };

    void PrintUsage()
    {
        GLogInfo(u8"用法: BuiltinAssetExport [--out <dir>] [--toml-only | --flat-only]");
        GLogInfo(u8"  默认 --out %ls（两份格式都写）", kDefaultOutputDir);
    }
}//namespace

int main(int argc, char **argv)
{
    hgl::logger::InitLogger(OS_TEXT("BuiltinAssetExport"));

    hgl::OSString output_dir = kDefaultOutputDir;
    bool export_toml = true;
    bool export_flat = true;

    for (int i = 1; i < argc; ++i)
    {
        const hgl::AnsiString arg = argv[i];

        if (arg == "--help" || arg == "-h")
        {
            PrintUsage();
            return 0;
        }
        else if (arg == "--toml-only")
        {
            export_toml = true;
            export_flat = false;
        }
        else if (arg == "--flat-only")
        {
            export_toml = false;
            export_flat = true;
        }
        else if (arg == "--out")
        {
            if (i + 1 >= argc)
            {
                GLogError(u8"--out 缺少参数");
                return 2;
            }

            output_dir = hgl::ToOSString(argv[++i]);
        }
        else
        {
            GLogError(u8"未知参数: %s", arg.c_str());
            PrintUsage();
            return 2;
        }
    }

    if (output_dir.IsEmpty())
    {
        GLogError(u8"输出目录不能为空");
        return 2;
    }

    if (!export_toml && !export_flat)
    {
        GLogError(u8"--toml-only 与 --flat-only 不能同时使用");
        return 2;
    }

    GLogInfo(u8"内置资产导出 → %ls（文本=%d 二进制=%d）",
             output_dir.c_str(), export_toml ? 1 : 0, export_flat ? 1 : 0);

    hgl::graph::builtin::PresetWriter writer(output_dir, export_toml, export_flat);

    int failed_groups = 0;

    for (const PresetGroup &group : kGroups)
    {
        const int exported = group.export_fn(writer);

        if (exported != group.expected_count)
        {
            GLogError(u8"分组 %s 导出数量不符: 实际 %d, 预期 %d",
                      group.id, exported, group.expected_count);
            ++failed_groups;
            continue;
        }

        GLogInfo(u8"分组 %s 导出完成: %d 份", group.id, exported);
    }

    if (failed_groups > 0)
    {
        GLogError(u8"内置资产导出失败：%d 个分组未通过", failed_groups);
        return 1;
    }

    GLogInfo(u8"内置资产导出全部完成");
    return 0;
}
