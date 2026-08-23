#pragma once

#include<hgl/graph/ubo/SkyInfo.h>
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
     */
    struct EnvironmentInfo
    {
        SkyInfo sky;

        // 预留：FogInfo fog; ...
    };
}//namespace hgl::graph
