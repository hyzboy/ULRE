#pragma once

#include<hgl/component_v2/Component.h>
#include<hgl/graph/mesh/Primitive.h>

/**
 * 渲染组件示例 (Render Component Example)
 * 
 * 这是一个具体组件的实现示例，展示如何使用新的组件系统。
 * This is a concrete component implementation example showing how to use the new component system.
 * 
 * 与旧系统对比 (Comparison with Legacy):
 * - 旧系统: RenderComponent + RenderComponentData (两个类)
 * - 新系统: RenderComponentV2 (一个类，数据嵌入)
 */

COMPONENT_V2_NAMESPACE_BEGIN

/**
 * 渲染组件 V2
 * 
 * 设计说明 (Design Notes):
 * 1. 数据直接作为成员，不需要独立的 ComponentData
 * 2. 使用宏简化类型定义
 * 3. 由 RenderComponentManager 管理生命周期
 */
class RenderComponentV2 : public Component
{
    COMPONENT_V2_CLASS_BODY(RenderComponentV2)

private:
    hgl::graph::Primitive* primitive = nullptr;     ///< 渲染图元（数据直接嵌入）
    hgl::graph::Material* material = nullptr;       ///< 材质
    bool visible = true;                            ///< 是否可见
    float alpha = 1.0f;                             ///< 透明度

public:
    /**
     * 构造函数
     * @param mgr 组件管理器
     * 
     * 说明: 必须通过管理器创建
     */
    RenderComponentV2(ComponentManager* mgr)
        : Component(mgr)
    {
    }

    /**
     * 析构函数
     * 
     * 说明: 
     * - 不负责删除 primitive 和 material
     * - 这些资源由各自的管理器负责
     */
    virtual ~RenderComponentV2()
    {
        primitive = nullptr;
        material = nullptr;
    }

    // ===== 数据访问 =====

    void SetPrimitive(hgl::graph::Primitive* prim) { primitive = prim; }
    hgl::graph::Primitive* GetPrimitive() const { return primitive; }

    void SetMaterial(hgl::graph::Material* mtl) { material = mtl; }
    hgl::graph::Material* GetMaterial() const { return material; }

    void SetVisible(bool v) { visible = v; }
    bool IsVisible() const { return visible; }

    void SetAlpha(float a) { alpha = a; }
    float GetAlpha() const { return alpha; }

    // ===== 生命周期回调 =====

    void OnAttach(IComponentContainer* container) override
    {
        // 组件被附加到节点时的逻辑
        // 例如：注册到渲染列表
    }

    void OnDetach(IComponentContainer* container) override
    {
        // 组件从节点分离时的逻辑
        // 例如：从渲染列表移除
    }

    // ===== 克隆 =====

    Component* Clone() override
    {
        auto* mgr = GetManager();
        auto* cloned = static_cast<RenderComponentV2*>(mgr->CreateComponent());
        
        // 复制数据（引用，不是深拷贝）
        cloned->primitive = primitive;
        cloned->material = material;
        cloned->visible = visible;
        cloned->alpha = alpha;
        
        return cloned;
    }
};

/**
 * 渲染组件管理器 V2
 * 
 * 职责 (Responsibilities):
 * - 创建和销毁 RenderComponentV2
 * - 管理所有渲染组件
 * - 提供批量更新功能
 */
class RenderComponentV2Manager : public ComponentManager
{
    COMPONENT_V2_MANAGER_CLASS_BODY(RenderComponentV2)

public:
    /**
     * 构造函数
     */
    RenderComponentV2Manager() = default;

    /**
     * 析构函数
     */
    virtual ~RenderComponentV2Manager() = default;

    /**
     * 创建组件
     * @return 新创建的渲染组件
     */
    Component* CreateComponent() override
    {
        return new RenderComponentV2(this);
    }

    /**
     * 更新所有组件
     * @param delta_time 时间增量
     * 
     * 说明: 如果有需要每帧更新的逻辑，在这里实现
     */
    void UpdateComponents(double delta_time) override
    {
        // 遍历所有组件，执行更新逻辑
        for (auto* comp : GetComponents())
        {
            auto* render_comp = static_cast<RenderComponentV2*>(comp);
            
            // 例如：处理渐变效果
            // if (render_comp->IsFading())
            //     render_comp->UpdateFade(delta_time);
        }
    }

    /**
     * 收集可见的渲染组件
     * @param out_list 输出列表
     * 
     * 说明: 便捷方法，用于渲染流程
     */
    void CollectVisibleComponents(ComponentList& out_list)
    {
        for (auto* comp : GetComponents())
        {
            auto* render_comp = static_cast<RenderComponentV2*>(comp);
            if (render_comp->IsVisible())
                out_list.Add(render_comp);
        }
    }
};

// ===== 静态函数实现 =====

// 实现 GetDefaultManager 静态方法
inline RenderComponentV2Manager* RenderComponentV2::GetDefaultManager()
{
    return GetComponentManager<RenderComponentV2Manager>(true);
}

inline RenderComponentV2Manager* RenderComponentV2Manager::GetDefaultManager()
{
    return GetComponentManager<RenderComponentV2Manager>(true);
}

COMPONENT_V2_NAMESPACE_END

/**
 * 使用示例 (Usage Example):
 * 
 * ```cpp
 * using namespace hgl::graph_v2::component;
 * 
 * // 1. 获取管理器（自动创建）
 * auto* manager = RenderComponentV2::GetDefaultManager();
 * 
 * // 2. 创建组件
 * auto* render_comp = static_cast<RenderComponentV2*>(manager->CreateComponent());
 * 
 * // 3. 设置数据
 * render_comp->SetPrimitive(my_primitive);
 * render_comp->SetMaterial(my_material);
 * 
 * // 4. 附加到节点
 * transform_node->AttachComponent(render_comp);
 * 
 * // 5. 使用完毕后销毁
 * manager->DestroyComponent(render_comp);
 * ```
 * 
 * 对比旧系统 (Comparison with Legacy):
 * 
 * 旧系统需要:
 * ```cpp
 * // 1. 创建 ComponentData
 * auto data = CreateSharedPtr<RenderComponentData>();
 * 
 * // 2. 创建 Component
 * auto comp = manager->CreateComponent(data);
 * 
 * // 3. 类型系统复杂
 * data->GetTypeHash();            // ComponentData 的哈希
 * comp->GetTypeHash();            // Component 的哈希
 * comp->GetDataTypeHash();        // ComponentData 的哈希
 * comp->GetManagerTypeHash();     // ComponentManager 的哈希
 * ```
 * 
 * 新系统更简单:
 * ```cpp
 * // 1. 直接创建 Component（数据嵌入）
 * auto comp = manager->CreateComponent();
 * 
 * // 2. 类型系统简单
 * comp->GetTypeHash();  // 只有一个哈希
 * ```
 */
