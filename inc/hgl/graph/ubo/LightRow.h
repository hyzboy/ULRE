#pragma once

#include<hgl/math/Vector.h>
#include<cstdint>
#include<cstddef>

namespace hgl::graph
{
    /**
     * 光源类型（加类型 = 加常量；**不得**为未实现类型写分支）。
     * ⚠ 新增枚举值必须同步 `kAllLightTypes`——那是覆盖性判定的唯一真源（漏更新 = 编译期失败）。
     */
    enum class LightType : uint32_t
    {
        Direction = 0,      ///< 方向光（当前唯一实现；主方向光 = 行 0）
        RANGE_SIZE          ///< 结束哨兵，无意义
    };

    constexpr LightType kAllLightTypes[] = { LightType::Direction };
    static_assert(sizeof(kAllLightTypes) / sizeof(kAllLightTypes[0]) == static_cast<size_t>(LightType::RANGE_SIZE),
                  "新增 LightType 必须同步 kAllLightTypes（枚举覆盖性判定）");

    /// 光源行的 flags 位
    constexpr uint32_t kLightFlagEnabled    = 1u << 0;   ///< 参与光照
    constexpr uint32_t kLightFlagCastShadow = 1u << 1;   ///< 该光投阴影（机制见 shadow_kind）

    /// 阴影机制种类（加机制 = 加常量 + 在光行里给索引；同样要同步 kAllLightShadowKinds）
    enum class LightShadowKind : uint32_t
    {
        None = 0,           ///< 不投阴影
        CSM,                ///< 级联阴影（主方向光当前唯一机制；shadow_index = 本世界影子数据行/句柄）
        RANGE_SIZE
    };

    constexpr LightShadowKind kAllLightShadowKinds[] = { LightShadowKind::None, LightShadowKind::CSM };
    static_assert(sizeof(kAllLightShadowKinds) / sizeof(kAllLightShadowKinds[0]) == static_cast<size_t>(LightShadowKind::RANGE_SIZE),
                  "新增 LightShadowKind 必须同步 kAllLightShadowKinds（枚举覆盖性判定）");

    /// 光源行容量（**冻结**：超限 = 项目 bug ⇒ 响亮报错，不扩容、不静默截断）
    constexpr uint32_t kLightRowCapacity = 16;

    /// 主方向光恒为 0 号行（阴影域与 shader 都按此假设取光；改这个值 = 全链改动）
    constexpr uint32_t kMainLightRowIndex = 0;

    /**
     * 光源行（世界私有表 `addr_lights` 的行；**POD + GLSL 镜像**）。
     *
     * ⚠ 布局要点：`alignas(64)` + **显式** `_pad`（scalar layout 不隐式补 alignas 填充）；
     *   `direction_intensity` 必须在 16B 边界（glm::vec4 要求）⇒ 前面显式 2 个 float 填充。
     *   C++ 与 GLSL 两份必须逐字段同序同型（门里结构对拍兜底：漂移 ⇒ 静默采到垃圾光照参数）。
     *
     * 约定：`direction_intensity.xyz` = **指向光源**的方向（与 `pcf_shadow.glsl` 既有约定一致）；
     *      `.w` = 强度。`color_angle.rgb` = 颜色；`.a` = 角径（度，软阴影/天空共用）。
     */
    struct alignas(64) LightRow
    {
        uint32_t type   = 0;            ///< LightType
        uint32_t flags  = 0;            ///< kLightFlag*
        float    _pad0[2] = { 0.0f, 0.0f };

        Vector4f direction_intensity = Vector4f(0.0f, 0.0f, 1.0f, 1.0f);
        Vector4f color_angle         = Vector4f(1.0f, 1.0f, 1.0f, 16.0f);

        uint32_t shadow_kind  = 0;      ///< LightShadowKind
        uint32_t shadow_index = 0;      ///< 该机制下的资源索引（0 = 无）
        uint32_t _pad1[2]     = { 0u, 0u };
    };

    static_assert(sizeof(LightRow) == 64, "LightRow 必须恰为 64 字节（alignas(64) 行）");
    static_assert(alignof(LightRow) == 64, "LightRow 必须 alignas(64)（与 EntityGPU 行同族）");
    static_assert(offsetof(LightRow, direction_intensity) == 16, "direction_intensity 必须落在 16B 边界");
    static_assert(offsetof(LightRow, color_angle) == 32, "color_angle 必须落在 32B");
    static_assert(offsetof(LightRow, shadow_kind) == 48, "shadow_kind 必须落在 48B");
}//namespace hgl::graph
