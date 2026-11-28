#pragma once

#include <hgl/ecs/support/transform/TransformSlotTable.h>
#include <hgl/ecs/support/transform/TransformDataStorage.h>
#include <hgl/ecs/support/transform/TransformAccessor.h>
#include <hgl/ecs/support/cull/BoundingBoxDataStorage.h>
#include <hgl/ecs/support/cull/BoundingBoxAccessor.h>
#include <hgl/ecs/support/cull/VisibilityDataStorage.h>
#include <hgl/type/Smart.h>
#include <cstdint>

namespace hgl::ecs
{
    class ECSCoreContext;
    class ECSContext;

    /**
     * 空间事件观察者接口（如变换登记时触发对应系统挂载）
     */
    class IWorldSpatialObserver
    {
    public:
        virtual ~IWorldSpatialObserver() = default;
        virtual void OnTransformRegistered(TransformID id, bool isMovable) {}
        virtual void OnTransformMembershipChanged(TransformID id) {}
        virtual void OnTransformChanged(TransformID id, uint32_t change_mask) {}
        virtual void OnEntityVisibilityChanged(EntityID id, bool visible) {}
    };

    /**
     * WorldSpatialState
     *
     * 空间域聚合状态：
     * - 变换槽位表与行存储 (TransformSlotTable / TransformDataStorage)
     * - 包围盒行存储 (BoundingBoxDataStorage)
     * - 可见性存储 (VisibilityDataStorage)
     *
     * 负责 CPU 侧空间查询、槽位生命周期、包围盒与可见性集合管理。
     */
    class WorldSpatialState
    {
        friend class ECSContext;
        friend class TransformAccessor;
    private:
        ECSCoreContext* core_context = nullptr;
        IWorldSpatialObserver* observer = nullptr;

        TransformSlotTable transform_slots;
        uint64_t static_scene_revision = 0;
        bool transform_slot_overflow_warned = false;    ///< 段预算用尽：一次性告警闩
        bool transform_row_move_warned = false;        ///< Mobility 换边（行号移动）：一次性告警闩
        uint32_t transform_row_move_count = 0;         ///< 换边次数（可观测）

        AutoDelete<TransformDataStorage> transform_storage;
        AutoDelete<BoundingBoxDataStorage> bounding_box_storage;
        AutoDelete<VisibilityDataStorage> visibility_storage;

    public:
        WorldSpatialState();
        ~WorldSpatialState();

        WorldSpatialState(const WorldSpatialState&) = delete;
        WorldSpatialState& operator=(const WorldSpatialState&) = delete;

    private:
        void Initialize(ECSCoreContext* core_ctx, IWorldSpatialObserver* obs);
        void NotifyTransformChanged(TransformID id, uint32_t change_mask);
        ECSCoreContext *GetCoreContext() const { return core_context; }

    public:
        void Clear();

        /// 实体销毁时的空间数据统一回收入口（变换行、包围盒行、可见性状态）
        void OnEntityDestroy(EntityID id);

        /// 纯 CPU 变换层级与世界矩阵求值（供无头世界与渲染世界共用唯一实现）
        void UpdateTransforms();

        // ── 变换槽位与生命周期 ──
        uint32_t GetTransformSegmentBudget(bool movable) const;
        uint32_t AcquireTransformSlot(bool movable);
        void ReleaseTransformSlot(bool movable, uint32_t slot);
        bool TryGetTransformSlot(TransformID id, bool movable, uint32_t &out_slot) const;
        uint32_t GetLiveTransformCount(bool movable) const;
        uint32_t GetTransformRowMoveCount() const { return transform_row_move_count; }
        void BumpStaticSceneRevision() { ++static_scene_revision; }
        uint64_t GetStaticSceneRevision() const { return static_scene_revision; }

        bool RegisterTransform(TransformID id, bool isMovable);
        bool MigrateTransform(TransformID id, bool toMovable);
        void UnregisterTransform(TransformID id);

        TransformID CreateTransform(EntityID owner, Mobility mobility = Mobility::Static);
        void DestroyTransform(TransformID id);

        TransformID GetTransformID(EntityID owner) const;
        TransformAccessor GetTransform(TransformID id);
        TransformAccessor GetTransformByEntity(EntityID owner);

        const ValueArray<TransformID>& GetStaticTransforms() const { return transform_slots.Slots(false); }
        const ValueArray<TransformID>& GetMovableTransforms() const { return transform_slots.Slots(true); }

        TransformDataStorage *Transforms() { return transform_storage; }
        const TransformDataStorage *Transforms() const { return transform_storage; }

        BoundingBoxDataStorage *BoundingBoxes() { return bounding_box_storage; }
        const BoundingBoxDataStorage *BoundingBoxes() const { return bounding_box_storage; }

        VisibilityDataStorage *Visibility() { return visibility_storage; }
        const VisibilityDataStorage *Visibility() const { return visibility_storage; }

        template<typename Fn>
        void ForEachMovableTransform(Fn &&fn) const
        {
            for (const TransformID id : transform_slots.Slots(true))
                if (IsValidTransformID(id))
                    fn(id);
        }

        template<typename Fn>
        void ForEachStaticTransform(Fn &&fn) const
        {
            for (const TransformID id : transform_slots.Slots(false))
                if (IsValidTransformID(id))
                    fn(id);
        }

        // ── 包围盒 ──
        BoundingBoxAccessor GetBoundingBoxByEntity(EntityID owner) const;
        BoundingBoxAccessor GetOrCreateBoundingBox(EntityID owner);
        void DestroyBoundingBox(BoundingBoxDataStorage::HandleID id);

        // ── 可见性 ──
        void SetEntityVisible(EntityID id, bool visible);
        bool IsEntityVisible(EntityID id) const;

        TransformSlotTable& GetTransformSlotTable() { return transform_slots; }
        const TransformSlotTable& GetTransformSlotTable() const { return transform_slots; }
    };
}
