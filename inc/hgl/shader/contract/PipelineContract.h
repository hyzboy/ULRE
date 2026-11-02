/// @file PipelineContract.h
/// 渲染管线物理布局契约（描述符集序号 / 阶段间插值 location）的 C++ 真源。
///
/// 这两组数字此前分别由两个单功能 C++ 工具生成（`DescriptorMacroGen` /
/// `InterStageLocationMacroGen`），各自的真源还都不在这里：前者是一张只为
/// "能生成"而存在的 `kDescriptorBindingMacros` 规范表，后者是**运行期**注册表
/// `ShaderSemanticRegistry` —— 都不是编译器可静态断言的形态，GLSL 侧的漂移
/// 只能靠生成器逐字节对拍发现。
///
/// 现在数值本身提升为本文件的编译期枚举：GLSL 生成物、运行期注册表的
/// `stable_location`、以及编译期对拍断言全部指向它。两个旧工具已删除。

#pragma once

#include <cstdint>

#include <hgl/shader/contract/ShaderContractMacros.h>

namespace hgl::shader::contract
{
    /// 描述符集序号（GLSL 宏 `BINDLESS_SET`）。
    ///
    /// 引擎已无 Scene 集：相机 / 调色板 / 全局地址表 / sky / viewport / shadow 全部
    /// 改走 BDA（地址进 `global_addresses`，基址经 `pc_root.addr_global_addresses` 下发）
    /// ⇒ 唯一集合就是 Bindless，集号收敛为 0。
    ///
    /// 必须与 `hgl::graph::DescriptorSetType::Bindless` 相等
    ///（由 `ShaderConstantsParity.gen.h` 的 `static_assert` 守住）。
    enum class HGL_SHADER_ENUM DescriptorSetId : int
    {
        Bindless = 0,   ///< 全局 Bindless 纹理数组集合（一帧绑一次）
    };

    /// 双 Stage（Mesh Shader ↔ Fragment Shader）交接语义的**稳定 location**。
    ///
    /// GLSL 宏名 = `HGL_LOCATION_` + 上划线的枚举名（如 `WorldPos` → `HGL_LOCATION_WORLD_POS`）。
    /// 规则：`.mesh` / `.frag` 中严禁硬编码 location 数字，一律使用该宏。
    ///
    /// ⚠ 这些数字进 program 缓存键：改动 = 全量着色器缓存失效，且两侧都用宏时
    ///   单边漂移**不会**编译失败（各自仍自洽），只能靠生成器对拍发现。
    enum class HGL_SHADER_ENUM ShaderStageLocationId : uint32_t
    {
        DataIndexID    = 0,   ///< 实体/图元索引（打断批用）
        WorldPos       = 1,   ///< 世界坐标
        WorldNormal    = 2,   ///< 世界法线
        UV0            = 3,   ///< 主 UV
        StyleID        = 4,   ///< 样式槽号（2D 批量描边/图集用）
        VertexColor    = 5,   ///< 顶点色
        FragDirection  = 6,   ///< 视线方向
        Luminance      = 7,   ///< 亮度（vertex_luminance 域）
        WorldTangent   = 8,   ///< 世界切线（法线贴图）
        WorldBinormal  = 9,   ///< 世界副切线
    };
}//namespace hgl::shader::contract
