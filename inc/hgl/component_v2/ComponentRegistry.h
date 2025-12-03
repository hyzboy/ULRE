#pragma once

#include<hgl/component_v2/Component.h>
#include<hgl/type/Map.h>

/**
 * 组件注册表 (Component Registry)
 * 
 * 设计目标 (Design Goals):
 * 1. 全局单例 - 替代旧系统中通过 RenderFramework 管理的方式
 * 2. 独立管理 - ComponentManager 不依赖 RenderFramework
 * 3. 自动创建 - 支持按需自动创建默认管理器
 * 
 * 与旧系统对比 (Comparison with Legacy):
 * - 旧系统: RenderFramework 管理 ComponentManager
 * - 新系统: ComponentRegistry 全局单例管理
 * 
 * 迁移路径 (Migration Path):
 * - 阶段1: 创建 ComponentRegistry，与旧系统并存
 * - 阶段2: 新组件使用 ComponentRegistry
 * - 阶段3: 逐步迁移旧组件到新系统
 */

COMPONENT_V2_NAMESPACE_BEGIN

/**
 * 组件注册表
 * 
 * 实现细节 (Implementation Details):
 * - 单例模式
 * - 线程安全（如果需要）
 * - 按类型哈希管理 ComponentManager
 */
class ComponentRegistry
{
private:
    static ComponentRegistry* instance;     ///< 单例实例

    Map<size_t, ComponentManager*> manager_map;     ///< 类型哈希 -> 管理器

    ComponentRegistry() = default;
    ~ComponentRegistry();

public:
    // 禁用拷贝和移动
    ComponentRegistry(const ComponentRegistry&) = delete;
    ComponentRegistry& operator=(const ComponentRegistry&) = delete;

    /**
     * 获取单例实例
     * @return 注册表实例
     */
    static ComponentRegistry* Instance();

    /**
     * 销毁单例实例
     * 说明: 通常在程序退出时调用
     */
    static void DestroyInstance();

    /**
     * 注册组件管理器
     * @param manager 管理器
     * @return 注册成功返回 true
     * 
     * 说明:
     * - 如果已存在同类型管理器，返回 false
     * - 管理器的生命周期由注册表管理
     */
    bool RegisterManager(ComponentManager* manager);

    /**
     * 注销组件管理器
     * @param type_hash 类型哈希
     * @return 注销成功返回 true
     * 
     * 说明: 会销毁管理器及其所有组件
     */
    bool UnregisterManager(size_t type_hash);

    /**
     * 获取组件管理器
     * @param type_hash 类型哈希
     * @return 管理器指针，未找到返回 nullptr
     */
    ComponentManager* GetManager(size_t type_hash);

    /**
     * 获取组件管理器（模板版本）
     * @tparam T 管理器类型
     * @param create_if_not_exist 如果不存在是否自动创建
     * @return 管理器指针
     * 
     * 说明:
     * - 如果管理器不存在且 create_if_not_exist 为 true，自动创建并注册
     * - 这是最常用的获取管理器的方式
     */
    template<typename T>
    T* GetManager(bool create_if_not_exist = true)
    {
        T* mgr = static_cast<T*>(GetManager(T::StaticTypeHash()));

        if (!mgr && create_if_not_exist)
        {
            mgr = new T();
            RegisterManager(mgr);
        }

        return mgr;
    }

    /**
     * 检查是否包含指定类型的管理器
     * @param type_hash 类型哈希
     * @return 包含返回 true
     */
    bool Contains(size_t type_hash) const;

    /**
     * 检查是否包含指定类型的管理器（模板版本）
     */
    template<typename T>
    bool Contains() const
    {
        return Contains(T::StaticTypeHash());
    }

    /**
     * 获取所有管理器的数量
     */
    size_t GetManagerCount() const { return manager_map.GetCount(); }

    /**
     * 清空所有管理器
     * 说明: 销毁所有管理器及其组件
     */
    void Clear();

    /**
     * 更新所有组件
     * @param delta_time 时间增量
     * 
     * 说明: 调用所有管理器的 UpdateComponents
     */
    void UpdateAllComponents(double delta_time);
};

/**
 * 便捷函数：获取组件管理器
 * 
 * 说明: 全局函数，便于使用
 */
template<typename T>
inline T* GetComponentManager(bool create_if_not_exist = true)
{
    return ComponentRegistry::Instance()->GetManager<T>(create_if_not_exist);
}

COMPONENT_V2_NAMESPACE_END
