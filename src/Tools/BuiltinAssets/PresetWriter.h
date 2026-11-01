#pragma once

/**
 * @file PresetWriter.h
 * @brief 内置预设资产的落盘器（**工具侧**，不进运行时库）。
 *
 * 一个内置预设（环境 / 几何 / 材质…）在仓库里的形态固定为「同为一份数据、两种物理格式」：
 *
 * ```
 * <out>/<subdir>/<stem><ext>          ← FlatBuffers（引擎运行时读的那份）
 * <out>/<subdir>/<stem><ext>.toml     ← TOML（作者侧可 diff / 可手改的那份）
 * ```
 *
 * 两份**必须字段完全一致**：它们都是同一个 `AssetObject::Serialize()` 走
 * `hgl::serialize::Archive` 的产物，物理格式只由文件名后缀决定（见 `ArchiveFile.h`）。
 * 因此本类只做「拼路径 + 调门面」，不认识任何具体资产类型，也不认识 TOML 与
 * FlatBuffers 的字节布局 —— 加新后端时本文件不用改。
 */

#include <hgl/asset/AssetObject.h>
#include <hgl/type/String.h>

namespace hgl::graph::builtin
{
    /**
     * 把内置预设写到固定输出目录。
     *
     * 线程约定：仅主线程离线使用（导出工具的 `main()`）。
     */
    class PresetWriter
    {
    private:

        OSString    output_dir;         ///<根输出目录（`--out`，通常是 `ShaderLibrary/asset`）
        bool        export_toml;        ///<是否写 `<stem><ext>.toml`
        bool        export_flat;        ///<是否写 `<stem><ext>`
        bool        dir_ready = false;  ///<子目录已确保存在（惰性，避免空跑时创建垃圾目录）

    private:

        OSString MakeFilePath(const os_char *subdir, const os_char *stem, const os_char *extension,
                              bool with_toml_suffix) const;

        bool EnsureDirectory(const os_char *subdir);

    public:

        /**
         * @param in_output_dir  根输出目录；空则一切 `Save()` 直接失败
         * @param in_export_toml 是否导出文本格式
         * @param in_export_flat 是否导出二进制格式
         */
        PresetWriter(const OSString &in_output_dir, bool in_export_toml, bool in_export_flat);

        PresetWriter(const PresetWriter &) = delete;
        PresetWriter &operator=(const PresetWriter &) = delete;

        /**
         * 落盘一份资产。
         *
         * @param asset      资产句柄（多态 → 走它自己的 `Serialize()`）
         * @param subdir     相对 `output_dir` 的子目录（如 `environment`）；可为 nullptr
         * @param stem       文件名主干（如 `sky34_cube_ibl`）
         * @param extension  资产类型后缀（如 `.env` / `.geo` / `.scene`；**不带** `.toml`）
         * @return 全部请求的格式都写成功
         */
        bool Save(hgl::asset::AssetObject &asset, const os_char *subdir,
                  const os_char *stem, const os_char *extension);

        const OSString &GetOutputDir() const { return output_dir; }
    };//class PresetWriter
}//namespace hgl::graph::builtin
