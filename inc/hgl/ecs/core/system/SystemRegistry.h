#pragma once

#include<hgl/ecs/core/system/System.h>
#include<hgl/ecs/core/system/SystemScheduleState.h>
#include<hgl/ecs/support/storage/ComponentTypeTable.h>
#include<hgl/type/PtrArray.h>
#include<hgl/type/String.h>
#include<hgl/type/UnorderedMap.h>

namespace hgl::ecs
{
    class ECSCoreContext;
    class ECSContext;

    /**
     * 系统注册与执行顺序注册表（世界私有）。
     *
     * 持有：系统实例共享所有权（tick/render 分类）、渲染元素类型索引、
     * 按组系统快照、有序执行列表，以及依赖/脏标记/插入序号调度状态。
     * 世界仍是总入口：注册/查询/组开关经 ECSContext 转发，本注册表不提供绕过世界的旁路。
     *
     * 分类权威 = 系统自身 ExecutionPhase（以 RenderSwapchainNextImage 为界）；
     * 同 key 替换必须先把旧实例从两映射与两执行列表摘除并 Shutdown（见 AddOrUpdate）。
     */
    class SystemRegistry
    {
    public:
        struct OrderedSystem
        {
            size_t key = 0;
            int phase = 0;          // ExecutionPhase as int
            int group_id = 0;       // SystemGroupID as int（同相位内按组分层排序）
            uint64_t insertion_order = 0;  // 用于稳定排序
            std::shared_ptr<System> system;
        };

    private:
        ECSCoreContext *context = nullptr;    ///< 非拥有回指：世界以成员持有本注册表，生命周期更长

        // 分类存储：更新系统和渲染系统分开
        hgl::UnorderedMap<size_t, std::shared_ptr<System>> tick_systems;
        hgl::UnorderedMap<size_t, std::shared_ptr<System>> render_systems;

        // 按渲染元素类型存储的系统（**一次性注册路径**仍按名字：组定义/安装器/管线）。
        // 内部无序存储；唯一有序消费者（ECSContext::GetAllRenderElementTypes）在世界边界
        // 按名字排序，保持旧 std::map 的遍历（组注册）顺序。
        hgl::UnorderedMap<AnsiString, PtrArray<std::shared_ptr<System>>> systems_by_element_type;

        /// 按组系统快照（使启用/禁用走 O(1) 数组；注册系统时失效，下次启用时重建）
        PtrArray<std::shared_ptr<System>> group_systems[kSystemGroupCount];

        // 执行顺序（缓存）：更新系统和渲染系统分开
        PtrArray<OrderedSystem> tick_system_order;
        PtrArray<OrderedSystem> render_system_order;

        // 调度状态：依赖图 + 脏标记 + 插入序号（随执行列表一起迁移，避免跨对象同步）
        SystemScheduleState schedule;

        void SortSystemList(PtrArray<OrderedSystem>& order_list,
                            const SystemScheduleState::DependencyMap& dependencies,
                            bool& dirty_flag,
                            const char* label);
        static OrderedSystem* FindOrderedSystem(PtrArray<OrderedSystem>& list, size_t key);

    public:
        explicit SystemRegistry(ECSCoreContext *ctx = nullptr)
            : context(ctx)
        {
        }

        void SetContext(ECSCoreContext *ctx) { context = ctx; }
        ECSCoreContext *GetContext() const { return context; }

        /// 注册或替换（key = typeid hash）。同 key 替换：先从两映射+两执行列表摘旧并 Shutdown，
        /// 再让新实例进入映射/元素索引/执行列表并重建依赖；世界 active 时立即 Initialize。
        void AddOrUpdate(size_t key, const std::shared_ptr<System>& system);

        /// 按 key 查找（先 tick 表后 render 表）；找不到返回 nullptr
        std::shared_ptr<System> Find(size_t key) const;

        void SortTick();
        void SortRender();

        PtrArray<OrderedSystem>& TickOrder() { return tick_system_order; }
        PtrArray<OrderedSystem>& RenderOrder() { return render_system_order; }

        /// 元素类型索引查找（不插入）；不存在返回 nullptr
        const PtrArray<std::shared_ptr<System>>* FindByElementType(const AnsiString& element_type) const
        {
            return systems_by_element_type.GetValuePointer(element_type);
        }

        /// 遍历全部元素类型名（无序；需要有序输出的调用方自行排序）
        template<typename Fn>
        void ForEachElementType(Fn&& fn) const
        {
            for (const auto& pair : systems_by_element_type)
                fn(pair.first);
        }

        /// 启用/禁用组内所有系统：有快照走快照，否则从元素索引重建
        void SetGroupEnabled(SystemGroupID group, bool enabled);

        /// 清空全部组快照（世界 ResetComponentDrivenGroups 用）
        void ClearGroupSnapshots()
        {
            for (uint32_t g = 0; g < kSystemGroupCount; ++g)
                group_systems[g].Clear();
        }

        /// 清两分类映射 + 两执行列表（系统共享引用在此释放；元素索引/快照仍持引用）
        void ClearRegistrations()
        {
            tick_systems.Clear();
            render_systems.Clear();
            tick_system_order.Clear();
            render_system_order.Clear();
        }

        void ClearElementIndex() { systems_by_element_type.Clear(); }
        void ClearDependencies() { schedule.ClearDependencies(); }

        /// Shutdown 的 active 路径：排序 → 逐 tick 条目 Shutdown 并清列表 →
        /// 逐 render 条目 Shutdown 并清列表 → 清两映射。
        /// 不清元素索引/依赖/快照（与旧世界 Shutdown 行为一致，由 inactive 路径或成员析构释放）。
        void ShutdownSystems();
    };
}