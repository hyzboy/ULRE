#pragma once

#include<hgl/graph/BufferAnnotation.h>
#include<hgl/graph/ubo/AmbientEnvInfo.h>
#include<cstdint>

namespace hgl::graph
{
    // 环境 Profile 句柄。0=无效，1=内置 default（EnvironmentManager 创建时生成）。
    using EnvProfileID = uint32_t;

    constexpr EnvProfileID kEnvProfileInvalid = 0;
    constexpr EnvProfileID kEnvProfileDefault = 1;

    /**
     * 环境综合信息（纯数据，一个 Profile 的全部环境状态）。
     * 数据唯一权威在 EnvironmentManager；RT/WORLD 只持有 EnvProfileID 引用。
     * 将来雾/环境光/IBL 等加在这里，shader 侧仍按段落各自物化为 UBO。
     *
     * ⚠ T6b/P4：**影子数据（`ShadowInfo`）已移出本结构** —— 它随**世界**（`ECSContext`）而不是
     * 随 profile（profile 可被多个世界共享 ⇒ 两个世界同帧会互相覆写同一份影子数据；CSM 的
     * RT/控制器本来就挂世界）。影子数据的 CPU 权威 = `ECSContext::EditShadowInfo()`，
     * 设备镜像 = 世界私有 SSBO（地址 `WorldAddresses.addr_shadow`，按帧槽各一份）。
     * 光源数据同理在**世界私有光源行**（`addr_lights`）。
     *
     * ⚠ **2026-10-07：`sky` 段整删**（原 `SkyInfo`）。理由：它的两个字段（`base_sky_color` /
     * `halo_intensity`）只服务"假大气天空"这一种方法，且**全仓从来没有写者**（唯一调用点只调了一次
     * `EditSkyInfo()` 未改字段）⇒ 取值搬成 `sky_atmosphere.glsl` 内常量后外观逐位不变；空的结构 +
     * 一条从没被写过的地址（`addr_sky`）没有保留价值（零兼容、不留空壳）。真做天空方法时按 R1 新建
     * 它**自己的**参数块。
     * 本结构现在只剩 **`ambient_env`**（`ambient_light_provider` 槽参数块）。
     */
    struct HGL_CPU_ONLY EnvironmentInfo
    {
        AmbientEnvInfo  ambient_env;    ///< `ambient_light_provider` 槽参数块（IBL 句柄）

        // 预留：FogInfo fog; ...
    };
}//namespace hgl::graph
