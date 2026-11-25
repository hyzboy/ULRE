#pragma once

#include<cstdint>

/**
 * 环境 Profile 句柄 —— 后端无关契约。
 *
 * 为什么在契约层：渲染目标（Vulkan 层）只持有一个「用哪个环境 Profile」的
 * 整数引用，不需要也不应该看到 SceneGraph 的环境数据结构（IBL 句柄、天空参数等）。
 * 环境数据的唯一权威仍在 `graph::EnvironmentManager`。
 *
 * 命名空间保持 `hgl::graph`：这是既有公开 API 的一部分，迁移只改文件归属，
 * 不改类型名与命名空间，避免消费方连锁改动。
 */
namespace hgl::graph
{
    /// 环境 Profile 句柄。0=无效，1=内置 default（EnvironmentManager 创建时生成）。
    using EnvProfileID = uint32_t;

    constexpr EnvProfileID kEnvProfileInvalid = 0;
    constexpr EnvProfileID kEnvProfileDefault = 1;
}//namespace hgl::graph
