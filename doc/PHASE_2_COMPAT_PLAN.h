/**
 * Phase 2 过渡方案：RenderFramework 兼容性适配器
 *
 * 目的：保留旧 API 接口，内部实现转发到新系统
 * 这允许现有代码继续工作，同时我们逐步迁移到 ECSContext
 *
 * 删除时间表：
 * - Phase 2 (现在): 保存兼容性 stub
 * - Week 3-4: 迁移主要应用代码
 * - Week 5: 删除兼容性层
 */

#ifndef _HGL_RENDER_FRAMEWORK_COMPAT_H
#define _HGL_RENDER_FRAMEWORK_COMPAT_H

#pragma once

// ============================================================================
// Phase 2 迁移说明
// ============================================================================
//
// 旧代码（不推荐）：
//   auto rf = new RenderFramework("app");
//   rf->Init(1920, 1080);
//
// 新代码（推荐）：
//   auto ecs = std::make_shared<ecs::ECSContext>("app");
//   ecs->Initialize();
//   ecs->InitializeGraphics(device, render_target);
//   auto core = std::make_unique<ecs::RenderSystemCore>(ecs.get());
//   core->Initialize();
//
// ============================================================================

#include <hgl/vk/VKDevice.h>
#include <hgl/ecs/core/Context.h>

namespace hgl::graph{

namespace graph
{
    class RenderPass;
    class IRenderTarget;
}

/**
 * RenderFramework 兼容层 (已弃用)
 *
 * ⚠️ 警告：这个类已过时，仅为向后兼容而保留。
 *           新代码应该直接使用 ECSContext。
 *
 * 迁移指南：
 * 1. 将 RenderFramework* rf 替换为 ECSContext* ecs
 * 2. 使用 ecs->GetGraphicsContext() 访问图形资源
 * 3. 使用 RenderSystemCore 管理帧生命周期
 */
class RenderFramework : public io::WindowEvent
{
    // Phase 2: 内部使用 ECSContext 存储实际实现
    std::shared_ptr<hgl::ecs::ECSContext> ecs_context;

    // 保留的最小成员以满足旧 API
    class vk::VulkanDevice* device = nullptr;
    graph::IRenderTarget* render_target = nullptr;

public:

    RenderFramework(const OSString& app_name);
    virtual ~RenderFramework();

    // 最小实现，用于兼容性
    bool Init(uint w, uint h);
    void Tick();
    void OnResize(uint w, uint h);
    void OnActive(bool active);
    void OnClose();

    // 新方式：返回 ECSContext
    hgl::ecs::ECSContext* GetECSContext() { return ecs_context.get(); }

    // 旧方式：保持这些方法但标记为已弃用
    class vk::VulkanDevice* GetDevice() const { return device; }
    VkDevice GetVkDevice() const;

    graph::IRenderTarget* GetRenderTarget() { return render_target; }

    // 其他访问器 - 指向 ECSContext 或设备
    const hgl::math::Vector2i& GetMouseCoord() const;

    // 声明但不实现（会在编译时提示需要迁移）
    // 这些方法应该通过 GetECSContext() 的图形接口访问

    #define DEPRECATED_METHOD(signature) \
        signature { \
            LOG_ERROR("DEPRECATED: Use ECSContext instead of RenderFramework"); \
            return nullptr; \
        }

    // 下面的方法都被标记为已弃用
    // 新代码应该通过 Graphics/Material/Buffer managers 的直接 API 使用这些功能
};

}//namespace hgl::graph

#endif // _HGL_RENDER_FRAMEWORK_COMPAT_H
