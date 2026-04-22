#pragma once

#include <cstdint>

namespace hgl::graph::mtl
{
    /// 材质配方 ID — 稳定整数句柄，用于 ECS 持有 Recipe 引用
    ///
    /// 0 保留为无效值，有效 ID 从 1 开始。
    using MaterialRecipeID = uint32_t;

    /// 无效 / 未初始化 ID
    inline constexpr MaterialRecipeID kInvalidMaterialRecipeID = 0u;

} // namespace hgl::graph::mtl
