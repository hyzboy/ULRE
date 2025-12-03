#pragma once

#include<hgl/type/DataType.h>
#include<hgl/type/SortedSet.h>
#include<hgl/log/Log.h>

/**
 * 重构后的组件系统 (Refactored Component System)
 * 
 * 设计目标 (Design Goals):
 * 1. 简化类型系统 - 移除 ComponentData 层，数据直接嵌入 Component
 * 2. 明确所有权 - ComponentManager 拥有 Component 的生命周期
 * 3. 解除循环依赖 - Component 不直接引用容器，通过回调通知
 * 4. 独立于渲染框架 - 通过 ComponentRegistry 管理，不依赖 RenderFramework
 * 
 * 与旧系统对比 (Comparison with Legacy):
 * - 旧系统: Component -> ComponentData (三层结构，3个类型哈希)
 * - 新系统: Component (单层结构，1个类型哈希)
 * - 旧系统: Component.OwnerNode 直接引用
 * - 新系统: Component 通过事件回调与容器交互
 */

#define COMPONENT_V2_NAMESPACE         hgl::graph_v2::component
#define COMPONENT_V2_NAMESPACE_BEGIN   namespace COMPONENT_V2_NAMESPACE {
#define COMPONENT_V2_NAMESPACE_END     }

#define USING_COMPONENT_V2_NAMESPACE   using namespace COMPONENT_V2_NAMESPACE;

namespace hgl::graph_v2
{
    class IComponentContainer;  // 前向声明
}

COMPONENT_V2_NAMESPACE_BEGIN

using namespace hgl::graph_v2;

class ComponentManager;

/**
 * 组件基类 (Component Base Class)
 * 
 * 简化设计 (Simplified Design):
 * - 数据直接作为成员，不需要 ComponentData 层
 * - 不持有容器引用，通过事件回调通知
 * - 只有一个类型哈希，简化类型系统
 * 
 * 生命周期 (Lifecycle):
 * - 由 ComponentManager 创建
 * - 由 ComponentManager 销毁
 * - 可以被多个容器引用（但不拥有）
 */
class Component
{
    OBJECT_LOGGER

private:
    static uint unique_id_count;        ///< 全局唯一ID计数器

    uint unique_id;                     ///< 组件唯一ID
    ComponentManager* manager;          ///< 所属管理器（拥有者）

protected:
    friend class ComponentManager;

    /**
     * 管理器分离通知
     * @param mgr 被分离的管理器
     * 
     * 说明: 当组件从管理器中移除时调用
     */
    virtual void OnDetachFromManager(ComponentManager* mgr)
    {
        if (mgr == manager)
            manager = nullptr;
    }

public:
    /**
     * 构造函数
     * @param mgr 组件管理器
     * 
     * 说明: 组件必须通过管理器创建
     */
    Component(ComponentManager* mgr);

    /**
     * 析构函数
     * 
     * 说明: 
     * - 会从所有容器中分离（通过回调通知）
     * - 不删除数据（数据已嵌入）
     */
    virtual ~Component();

    /**
     * 获取组件类型哈希
     * 说明: 每个具体组件类型都要实现这个方法
     */
    virtual size_t GetTypeHash() const = 0;

    // ===== 访问器 =====

    uint GetUniqueID() const { return unique_id; }
    ComponentManager* GetManager() const { return manager; }

    // ===== 生命周期回调 =====

    /**
     * 附加到容器时的回调
     * @param container 容器接口
     * 
     * 说明: 当组件被添加到容器（如 TransformNode）时调用
     */
    virtual void OnAttach(IComponentContainer* container) {}

    /**
     * 从容器分离时的回调
     * @param container 容器接口
     * 
     * 说明: 当组件从容器中移除时调用
     */
    virtual void OnDetach(IComponentContainer* container) {}

    /**
     * 焦点丢失回调
     * 说明: 当组件所在场景失去焦点时调用
     */
    virtual void OnFocusLost() {}

    /**
     * 焦点获得回调
     * 说明: 当组件所在场景获得焦点时调用
     */
    virtual void OnFocusGained() {}

    /**
     * 克隆组件
     * @return 克隆的组件
     * 
     * 说明: 
     * - 克隆数据，但不克隆容器引用
     * - 克隆的组件由同一个 Manager 管理
     */
    virtual Component* Clone();
};

/**
 * 组件类型定义宏
 * 简化版本，只有一个类型哈希
 */
#define COMPONENT_V2_CLASS_BODY(name)                                           \
    static name##ComponentManager* GetDefaultManager();                         \
    name##ComponentManager* GetManager() const                                  \
    {                                                                           \
        return static_cast<name##ComponentManager*>(Component::GetManager());  \
    }                                                                           \
    static constexpr size_t StaticTypeHash()                                    \
    {                                                                           \
        return hgl::GetTypeHash<name##Component>();                             \
    }                                                                           \
    virtual size_t GetTypeHash() const override                                 \
    {                                                                           \
        return StaticTypeHash();                                                \
    }

using ComponentSet = hgl::SortedSet<Component*>;
using ComponentList = hgl::ArrayList<Component*>;

/**
 * 组件管理器基类 (Component Manager Base Class)
 * 
 * 职责 (Responsibilities):
 * - 创建和销毁组件（拥有生命周期）
 * - 管理组件集合
 * - 更新组件逻辑
 * 
 * 不依赖 (NOT Dependent on):
 * - RenderFramework（通过 ComponentRegistry 全局注册）
 * - 具体的容器实现
 */
class ComponentManager
{
    OBJECT_LOGGER

private:
    ComponentSet owned_components;      ///< 拥有的组件集合（负责销毁）

protected:
    friend class Component;

    /**
     * 附加组件到管理器
     * @param comp 组件
     * 
     * 说明: 由 Component 构造函数调用
     */
    virtual void AttachComponent(Component* comp)
    {
        if (comp)
            owned_components.Add(comp);
    }

    /**
     * 从管理器分离组件
     * @param comp 组件
     * 
     * 说明: 由 DestroyComponent 调用
     */
    virtual void DetachComponent(Component* comp)
    {
        if (comp)
            owned_components.Delete(comp);
    }

public:
    virtual ~ComponentManager();

    /**
     * 获取组件类型哈希
     */
    virtual size_t GetComponentTypeHash() const = 0;

    /**
     * 获取管理器类型哈希
     */
    virtual size_t GetTypeHash() const = 0;

    // ===== 组件创建和销毁 =====

    /**
     * 创建组件
     * @return 新创建的组件
     * 
     * 说明: 
     * - 子类实现具体创建逻辑
     * - 管理器拥有组件的生命周期
     */
    virtual Component* CreateComponent() = 0;

    /**
     * 销毁组件
     * @param comp 要销毁的组件
     * 
     * 说明:
     * - 从所有容器中分离
     * - 从管理器中移除
     * - 删除组件对象
     */
    virtual void DestroyComponent(Component* comp);

    // ===== 组件查询 =====

    size_t GetComponentCount() const { return owned_components.GetCount(); }
    ComponentSet& GetComponents() { return owned_components; }
    const ComponentSet& GetComponents() const { return owned_components; }

    /**
     * 获取指定容器中的组件
     * @param comp_list 输出列表
     * @param container 容器
     * @return 组件数量
     */
    int GetComponentsInContainer(ComponentList& comp_list, IComponentContainer* container);

    // ===== 更新 =====

    /**
     * 更新所有组件
     * @param delta_time 时间增量
     * 
     * 说明: 每帧调用，更新所有组件的逻辑
     */
    virtual void UpdateComponents(double delta_time);

    // ===== 事件 =====

    virtual void OnFocusLost() {}
    virtual void OnFocusGained() {}
};

/**
 * 组件管理器类型定义宏
 * 简化版本
 */
#define COMPONENT_V2_MANAGER_CLASS_BODY(name)                                   \
    static name##ComponentManager* GetDefaultManager();                         \
    static constexpr size_t StaticTypeHash()                                    \
    {                                                                           \
        return hgl::GetTypeHash<name##ComponentManager>();                      \
    }                                                                           \
    static constexpr size_t StaticComponentTypeHash()                           \
    {                                                                           \
        return hgl::GetTypeHash<name##Component>();                             \
    }                                                                           \
    virtual size_t GetComponentTypeHash() const override                        \
    {                                                                           \
        return StaticComponentTypeHash();                                       \
    }                                                                           \
    virtual size_t GetTypeHash() const override                                 \
    {                                                                           \
        return StaticTypeHash();                                                \
    }

COMPONENT_V2_NAMESPACE_END
