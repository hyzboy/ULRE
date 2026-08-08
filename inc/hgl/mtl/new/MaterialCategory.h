#pragma once

#include <hgl/type/EnumUtil.h>

namespace hgl::graph
{
    // 引擎内建材质分类 (0-3)
    // 注意：2D 渲染路径均为 Unlit，故仅有 Unlit2D 而无 Lit2D。
    // 项目扩展材质分类从 64 开始
    enum class MaterialCategory : uint8
    {
        // 引擎内建
        Unlit2D     = 0,    // 2D 无光照
        Unlit3D     = 1,    // 3D 无光照
        Lit3D       = 2,    // 3D 光照表面 (Lit Surface)
        Special3D   = 3,    // 3D 表面 (reserved: Skin/Hair/Cloth/Eye/Foliage/ClearCoat/Water → Lit)

        // 项目扩展从此开始
        ProjectBase = 64,
    };

    constexpr bool IsBuiltinCategory(MaterialCategory cat)
    {
        return static_cast<uint8>(cat) < static_cast<uint8>(MaterialCategory::ProjectBase);
    }
}
