#pragma once

#include <hgl/TickObject.h>
#include <hgl/ecs/core/Context.h>
#include <memory>

namespace hgl {

/**
 * 工作对象（轻量化版本 - Phase 1 完成）
 * 
 * 职责：
 * - 作为应用层的入口点
 * - 创建和管理 ECS 实体
 * - 驱动 ECS 的 Tick/Render 循环
 * 
 * 不再负责：
 * - 资源管理（转移到 ECSContext）
 * - 渲染协调（转移到 RenderSystemCore）
 * - 场景管理（转移到 ECS）
 * 
 * 迁移指南：
 * 
 * ❌ 旧方式（已废弃）：
 *   auto mat = obj->CreateMaterial("mat");
 *   auto tex = obj->CreateTexture("tex", 512, 512);
 * 
 * ✅ 新方式（推荐）：
 *   auto mat = obj->GetWorld()->CreateMaterial("mat", "shader.glsl");
 *   auto tex = obj->GetWorld()->LoadTexture("texture.png");
 * 
 * ✅ 或使用便捷方法：
 *   auto mat = obj->CreateMaterial("mat", "shader.glsl");
 *   auto tex = obj->LoadTexture("texture.png");
 */
class WorkObject : public TickObject {
private:
    /// ECS 世界（唯一持有）
    std::shared_ptr<ecs::ECSContext> world;
    
    /// 工作流状态标志
    bool destroy_flag = false;
    bool render_dirty = true;
    
protected:
    /**
     * 获取 ECS 世界
     * 用于子类中访问 ECS（如 world()->CreateEntity()）
     */
    ecs::ECSContext* GetECSContextPtr() {
        return world.get();
    }

public:
    /**
     * 构造函数
     * @param ctx ECS 世界（通常由应用程序创建）
     * 
     * 示例：
     *   auto world = std::make_shared<ecs::ECSContext>("game_world");
     *   auto game = std::make_unique<MyGame>(world);
     */
    explicit WorkObject(std::shared_ptr<ecs::ECSContext> ctx)
        : world(std::move(ctx)) {
        if (!world) {
            LOG_WARNING("WorkObject: ECSContext is null");
        }
    }
    
    virtual ~WorkObject() = default;
    
    // ========== 工作流生命周期 ==========
    
    /**
     * 初始化工作对象
     * 在此方法中创建游戏实体和组件
     */
    virtual bool Init() = 0;
    
    /**
     * 主循环（每帧调用）
     * 自动驱动所有 Tick System 的更新
     */
    virtual void Tick(double dt) override {
        if (world) {
            world->Tick(dt);
        }
    }
    
    /**
     * 渲染循环（在 RenderPass 中调用）
     * 子类可重写此方法做自定义渲染逻辑
     */
    virtual void Render(double dt) {
        // 子类可重写此方法
        // 通常 ECS 的渲染已通过 RenderSystemCore 完成
    }
    
    // ========== 便捷 API ==========
    
    /**
     * 创建实体（便捷方法）
     * 
     * 示例：
     *   auto entity = CreateEntity("player");
     *   entity->AddComponent<TransformComponent>();
     *   entity->AddComponent<PrimitiveComponent>();
     */
    ecs::Entity* CreateEntity(const std::string& name = "") {
        if (!world) return nullptr;
        return world->CreateEntity(name);
    }
    
    /**
     * 获取 ECS 世界
     * 用于高级操作如 GetSystem、RegisterSystem 等
     * 
     * 示例：
     *   auto transform_sys = GetWorld()->GetSystem<TransformSystem>();
     */
    ecs::ECSContext* GetWorld() {
        return world.get();
    }
    
    /**
     * 获取 GPU 设备（高级用法）
     * 
     * 示例：
     *   auto device = GetGPUDevice();
     *   if (device) {
     *       // 做一些底层 Vulkan 操作
     *   }
     */
    hgl::vk::VulkanDevice* GetGPUDevice() {
        if (!world) return nullptr;
        return world->GetGPUDevice();
    }
    
    /**
     * 获取渲染目标（高级用法）
     */
    hgl::graph::IRenderTarget* GetRenderTarget() {
        if (!world) return nullptr;
        return world->GetRenderTarget();
    }
    
    // ========== 工作流标志 ==========
    
    /// 标记工作对象需要销毁
    void MarkForDestroy() {
        destroy_flag = true;
    }
    
    /// 检查是否标记为销毁
    bool IsMarkedForDestroy() const {
        return destroy_flag;
    }
    
    /// 标记渲染需要更新
    void MarkRenderDirty() {
        render_dirty = true;
    }
    
    /// 检查渲染是否为脏状态
    bool IsRenderDirty() const {
        return render_dirty;
    }
    
    /// 清除渲染脏标志
    void ClearRenderDirty() {
        render_dirty = false;
    }
};

} // namespace hgl
