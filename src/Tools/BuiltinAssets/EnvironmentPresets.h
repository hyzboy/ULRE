#pragma once

/**
 * @file EnvironmentPresets.h
 * @brief 内置环境预设的定义（**工具侧**，不属于运行时库）。
 *
 * 这里只放「数据」——每份预设就是一个 `EnvironmentSpec` 的赋值。落盘一律交给
 * `PresetWriter`（→ `hgl::serialize::SaveAssetObjectToFile`），因此本文件既不知道
 * TOML 长什么样，也不知道 FlatBuffers 长什么样。
 *
 * 为什么必须在运行时库之外：
 *  - 这些预设是**作者侧产物**，引擎只读它们，从不现场生成；
 *  - 产物甚至可能不进编辑器安装包（取决于发行策略），把生成器塞进运行时库等于
 *    让发行产物背着一份永远不会执行的代码。
 *
 * 对应的产物目录：`ShaderLibrary/asset/environment/`
 * （`sky_atmosphere` / `sky34_cube_ibl` / `sky34_oct_ibl` / `sky34_lambert_dome` / `sky34_sh9`）。
 */

#include "PresetWriter.h"

namespace hgl::graph::builtin
{
    /// 内置环境预设的数量（`ExportEnvironmentPresets()` 成功时应等于它）
    inline constexpr int kEnvironmentPresetCount = 5;

    /**
     * @brief 导出全部内置环境预设。
     *
     * @param writer 落盘器（输出根目录 + 格式选择）
     * @return 成功导出的预设数量；任何一份失败都立即返回已成功的计数（< 5）
     */
    int ExportEnvironmentPresets(PresetWriter &writer);
}//namespace hgl::graph::builtin
