#pragma once

#include<hgl/ecs/core/EntityHandle.h>
#include <ankerl/unordered_dense.h>
#include<hgl/sync/Mutex.h>

namespace hgl::ecs
{
    class ECSContext;

    /**
     * VisibilityDataStorage - Fast lookup for invisible entities with hierarchical support
     *
     * Maintains a set of invisible entity IDs for O(1) query during rendering.
     * **唯一真值**：由世界通过 `ECSContext::SetEntityVisible` 写入（A5a 起组件级可见性副本已删除）。
     *
     * **极性 = 负向**（集合成员 = 不可见；默认可见）；`IsInvisible` **含祖先链继承**，
     * `IsDirectlyInvisible` 只查自己 —— 后者**不得**冒充前者做可见判定（漏继承 ⇒ 后代被误判可见）。
     * 另两种表示：`EntityGPU` 行的 `Visible` 位 = **扁平派生位**（不继承）；策略事实
     * `StrategyFact::EntityVisible` = 每帧现算含继承。权威链 / 传播规则 / 已知失步见
     * **doc/visibility-authority-chain.md**。
     *
     * Supports hierarchical visibility: if an ancestor is invisible, all descendants are invisible.
     */
    class VisibilityDataStorage
    {
    private:
        ankerl::unordered_dense::set<EntityID> invisible_entities;
        mutable hgl::Mutex mutex;
        ECSContext* context = nullptr;

    public:
        VisibilityDataStorage(ECSContext* ctx = nullptr) : context(ctx) {}
        ~VisibilityDataStorage() = default;

        /// Set the context for hierarchical visibility checks
        void SetContext(ECSContext* ctx) { context = ctx; }

        /// Mark entity as invisible
        void SetInvisible(EntityID entity_id);

        /// Mark entity as visible
        void SetVisible(EntityID entity_id);

        /// Check if entity is invisible (O(1) lookup)
        /// Also checks ancestor chain for hierarchical visibility
        bool IsInvisible(EntityID entity_id) const;

        /// Query only direct visibility (doesn't check ancestors)
        bool IsDirectlyInvisible(EntityID entity_id) const;

        /// Get count of invisible entities
        size_t GetInvisibleCount() const;

        /// Clear all data
        void Clear();
    };
}//namespace hgl::ecs

