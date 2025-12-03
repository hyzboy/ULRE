#pragma once

#include<hgl/type/DataType.h>
#include<hgl/type/SortedSet.h>

/**
 * 组件容器接口 (Component Container Interface)
 * 
 * 设计目标 (Design Goals):
 * 1. 接口隔离 - 将组件管理功能独立为接口
 * 2. 解除循环依赖 - Component 不需要知道 SceneNode 的具体实现
 * 3. 明确所有权 - 容器只持有引用，不拥有 Component 的生命周期
 * 
 * 与旧系统对比 (Comparison with Legacy):
 * - 旧系统: Component 直接引用 SceneNode* OwnerNode
 * - 新系统: Component 通过 IComponentContainer 接口与容器交互
 */

namespace hgl::graph_v2
{
    // 前向声明 - 使用新的命名空间
    namespace component
    {
        class Component;
        class ComponentManager;
    }

    using ComponentSet = hgl::SortedSet<component::Component*>;

    /**
     * 组件容器接口
     * 
     * 职责 (Responsibilities):
     * - 管理组件的附加和分离
     * - 提供组件查询功能
     * - 通知组件容器事件
     * 
     * 不负责 (NOT Responsible for):
     * - Component 的创建和销毁 (由 ComponentManager 负责)
     * - Component 的更新逻辑 (由 ComponentManager 负责)
     */
    class IComponentContainer
    {
    public:
        virtual ~IComponentContainer() = default;

        /**
         * 附加组件到容器
         * @param comp 组件指针
         * @return 附加成功返回 true
         * 
         * 说明: 
         * - 容器只持有引用，不拥有组件生命周期
         * - 会调用组件的 OnAttach 回调
         */
        virtual bool AttachComponent(component::Component* comp) = 0;

        /**
         * 从容器分离组件
         * @param comp 组件指针
         * 
         * 说明:
         * - 只是从容器中移除引用，不销毁组件
         * - 会调用组件的 OnDetach 回调
         */
        virtual void DetachComponent(component::Component* comp) = 0;

        /**
         * 检查是否包含指定组件
         * @param comp 组件指针
         * @return 包含返回 true
         */
        virtual bool ContainsComponent(component::Component* comp) const = 0;

        /**
         * 检查是否有指定管理器的组件
         * @param manager 组件管理器
         * @return 有该管理器的组件返回 true
         */
        virtual bool HasComponentOfManager(const component::ComponentManager* manager) const = 0;

        /**
         * 获取所有组件
         * @return 组件集合的常量引用
         */
        virtual const ComponentSet& GetComponents() const = 0;

        /**
         * 获取组件数量
         */
        virtual size_t GetComponentCount() const = 0;

        /**
         * 清空所有组件
         * 
         * 说明: 会调用所有组件的 OnDetach，但不销毁组件
         */
        virtual void ClearComponents() = 0;
    };

}//namespace hgl::graph_v2
