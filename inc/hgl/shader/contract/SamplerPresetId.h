/// @file SamplerPresetId.h
/// 全局 Sampler 预设序号的 C++ 真源。
///
/// 数值语义：**索引 = `ShaderLibrary/contract/sampler.toml` 里 `[[samplers]]` 的出现顺序**
/// （从 0 开始，文件里不写 index 字段）。运行期 `SamplerPresetLibrary::GetIndex(name)`
/// 按同一顺序注册 `vkCreateSampler`，`ShaderGen` 生成 `#define <Name>Sampler <idx>u` 宏。
///
/// 为什么必须在这里而不是在 .toml 里：着色器侧宏是**手写常量**，写错不会编译失败，
/// 只会静默按 0 号（Nearest）采样 —— 视觉错误且无任何报错。把顺序固化成 C++ 枚举后，
/// 生成器可以逐条校验 `sampler.toml` 与本枚举的一致性（顺序、条数、名字），
/// 并把 GLSL 宏变成生成物。

#pragma once

#include <cstdint>

#include <hgl/shader/contract/ShaderContractMacros.h>

namespace hgl::shader::contract
{
    /// 全局采样器预设（GLSL 宏 `<Name>Sampler`，如 `TrilinearClampSampler`）。
    ///
    /// ⚠ 追加预设**只能往末尾加**：已有的值被材质 `.toml` 的
    ///   `[resources].samplers` 与已提交的 GLSL 宏引用，插队会整体错位。
    enum class HGL_SHADER_ENUM SamplerPresetId : uint32_t
    {
        Nearest         = 0,   ///< 最近邻（无 mip）：像素风 / UI 铭牌
        Linear          = 1,   ///< 双线性，mip 仍为最近邻
        Trilinear       = 2,   ///< 三线性（mip 线性）
        TrilinearAniso  = 3,   ///< 三线性 + 各向异性 16x
        ShadowPCF       = 4,   ///< 阴影图专用：ClampToEdge + 比较采样（GreaterOrEqual）
        Terrain         = 5,   ///< 地形：运行期可 Rebuild，槽位不变
        UI              = 6,   ///< GUI：最近邻 + ClampToEdge
        TrilinearClamp  = 7,   ///< 三线性 + ClampToEdge（八面体 IBL 展开等越界图）
    };
}//namespace hgl::shader::contract
