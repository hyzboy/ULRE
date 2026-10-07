#pragma once

#include<hgl/math/Vector.h>
#include<hgl/color/Color4f.h>
#include<cmath>
#include<cstdint>
#include<cstddef>

namespace hgl::graph
{
    // 与 ShadowInfo.h 同款：本头会被大量 TU（含无 `using namespace math` 的）直接包含，
    // 裸写 `Vector4f` 会因查找不到而报 C3861 ⇒ 在本命名空间内显式引入 math 名（保证自足）。
    using namespace math;

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

    // ── 太阳模型（时刻 → 主方向光参数）──────────────────────────────────────────
    // 唯一出处：默认主光（`MakeDefaultMainLight`）取它，时刻驱动的作者调用（LightSystem）也取它 ——
    // 算术与「光源寄居 `SkyInfo`」时代的 `SkyInfo::SetTime` 逐式相同
    //（仰角：6→12 点线性 0→90°、12→18 点线性 90→0°，其余时刻 -10°；方位角固定 180°；
    // 白天色温由地平线（暖）到正顶（近白）；夜间强度 0）。
    // ⚠ 两处与旧式不同，均**不改变现有画面**（旧调用点全是白天）：
    //   ① 夜间只表达「主光熄灭」——旧模型此时打开 `moon_*`（第二光源），而 D3 已删 `moon_*`；
    //   ② 采样是**纯函数**（旧实现夜里保留上一份 sun_color 不动；夜里强度为 0 ⇒ 颜色不可观测）。
    struct SunLightSample
    {
        Vector4f direction = Vector4f(0.0f, 0.0f, 1.0f, 0.0f);  ///< 指向光源（.w = 0，与 sky 时代同约定）
        Color4f  color     = Color4f(1.0f, 0.95f, 0.9f, 1.0f);
        float    intensity = 1.0f;
    };

    inline SunLightSample EvaluateSunLight(float hour, float minute, float second)
    {
        const float total_hours = hour + minute / 60.0f + second / 3600.0f;

        // 简单模型：太阳在正南方（180 度），12 点最高（仰角 90 度），6 / 18 点在地平线（0 度）
        float elevation_deg = -10.0f;

        if (total_hours >= 6.0f && total_hours <= 18.0f)
        {
            if (total_hours <= 12.0f)
                elevation_deg = (total_hours - 6.0f) * (90.0f / 6.0f);
            else
                elevation_deg = (18.0f - total_hours) * (90.0f / 6.0f);
        }

        const float azimuth_deg = 180.0f;

        const float azimuth_rad   = azimuth_deg * (3.14159265f / 180.0f);
        const float elevation_rad = elevation_deg * (3.14159265f / 180.0f);

        SunLightSample sample;

        sample.direction.x = cos(elevation_rad) * sin(azimuth_rad);
        sample.direction.y = cos(elevation_rad) * cos(azimuth_rad);
        sample.direction.z = sin(elevation_rad);
        sample.direction.w = 0.0f; // 方向向量

        // 色温随仰角（地平线最暖，正顶最冷）
        const float elev_norm = elevation_deg >= 90.0f ? 1.0f
                              : (elevation_deg <= 0.0f ? 0.0f : elevation_deg / 90.0f);
        const float warm = powf(1.0f - elev_norm, 1.2f);

        const Color4f day_col (1.00f, 0.98f, 0.95f, 1.0f);
        const Color4f dusk_col(1.00f, 0.62f, 0.32f, 1.0f);

        sample.color.r = day_col.r + (dusk_col.r - day_col.r) * warm;
        sample.color.g = day_col.g + (dusk_col.g - day_col.g) * warm;
        sample.color.b = day_col.b + (dusk_col.b - day_col.b) * warm;
        sample.color.a = 1.0f;

        // 白天主光可见；夜里主光熄灭（不由本模型点亮第二光源）
        sample.intensity = (elevation_deg > 0.0f) ? 1.0f : 0.0f;

        return sample;
    }

    /// 引擎默认时刻（默认主光与默认天空光晕**同源同参**：改这里 = 两处同改）
    inline constexpr float kDefaultSunHour   = 10.0f;
    inline constexpr float kDefaultSunMinute =  0.0f;
    inline constexpr float kDefaultSunSecond =  0.0f;

    /**
     * 默认主方向光（**行 0 的初值**）= `EvaluateSunLight(默认时刻)` + 启用位。
     *
     * 只置 `kLightFlagEnabled`（不投阴影）：阴影机制由 `MainLightShadowSystem` 的
     * Enable / Disable 在光行上落 `cast_shadow` / `shadow_kind`，本函数不预置机制。
     */
    inline LightRow MakeDefaultMainLight()
    {
        const SunLightSample sun = EvaluateSunLight(kDefaultSunHour, kDefaultSunMinute, kDefaultSunSecond);

        LightRow row;

        row.type                = uint32_t(LightType::Direction);
        row.flags               = kLightFlagEnabled;
        row.direction_intensity = Vector4f(sun.direction.x, sun.direction.y, sun.direction.z, sun.intensity);
        row.color_angle         = Vector4f(sun.color.r, sun.color.g, sun.color.b, 16.0f);
        row.shadow_kind         = uint32_t(LightShadowKind::None);
        row.shadow_index        = 0;

        return row;
    }

}//namespace hgl::graph
