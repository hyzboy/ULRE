#include <hgl/ecs/support/spatial/WorldSpatialState.h>
#include <hgl/ecs/core/ECSCoreContext.h>
#include <hgl/log/Log.h>

namespace hgl::ecs
{
    WorldSpatialState::WorldSpatialState()
        : transform_storage(new TransformDataStorage())
        , bounding_box_storage(new BoundingBoxDataStorage())
        , visibility_storage(new VisibilityDataStorage())
    {
    }

    WorldSpatialState::~WorldSpatialState() = default;

    void WorldSpatialState::Initialize(ECSCoreContext* core_ctx, IWorldSpatialObserver* obs)
    {
        core_context = core_ctx;
        observer = obs;

        if (!transform_storage)
            transform_storage = new TransformDataStorage();
        if (!bounding_box_storage)
            bounding_box_storage = new BoundingBoxDataStorage();
        if (!visibility_storage)
            visibility_storage = new VisibilityDataStorage();

        if (transform_storage)
            transform_storage->SetSpatialState(this);
        if (visibility_storage)
            visibility_storage->SetSpatialSource(core_ctx, transform_storage);
    }

    void WorldSpatialState::Clear()
    {
        transform_slots.Clear();

        if (TransformDataStorage* storage = transform_storage)
            storage->Clear();

        if (BoundingBoxDataStorage* storage = bounding_box_storage)
            storage->Clear();

        if (VisibilityDataStorage* storage = visibility_storage)
            storage->Clear();
    }

    void WorldSpatialState::OnEntityDestroy(EntityID id)
    {
        const TransformID transform = GetTransformID(id);
        if (IsValidTransformID(transform))
            DestroyTransform(transform);

        if (BoundingBoxDataStorage* bbox_storage = bounding_box_storage)
        {
            const BoundingBoxDataStorage::HandleID bbox = bbox_storage->FindByOwner(id);
            if (bbox != BoundingBoxDataStorage::INVALID_HANDLE)
                DestroyBoundingBox(bbox);
        }

        if (VisibilityDataStorage* vis_storage = visibility_storage)
            vis_storage->SetVisible(id);
    }

    void WorldSpatialState::UpdateTransforms()
    {
        if (TransformDataStorage* storage = transform_storage)
            storage->UpdateDirtyWorldMatricesFlat();
    }

    uint32_t WorldSpatialState::GetTransformSegmentBudget(const bool movable) const
    {
        const TransformDataStorage* storage = static_cast<const TransformDataStorage*>(transform_storage);
        if (!storage)
            return 0;

        return movable ? storage->GetDynamicRowBudget() : storage->GetStaticRowBudget();
    }

    uint32_t WorldSpatialState::AcquireTransformSlot(const bool movable)
    {
        return transform_slots.Acquire(movable, GetTransformSegmentBudget(movable));
    }

    void WorldSpatialState::ReleaseTransformSlot(const bool movable, const uint32_t slot)
    {
        transform_slots.Release(movable, slot);
    }

    bool WorldSpatialState::TryGetTransformSlot(TransformID id, bool movable, uint32_t &out_slot) const
    {
        return transform_slots.Find(id, movable, out_slot);
    }

    uint32_t WorldSpatialState::GetLiveTransformCount(const bool movable) const
    {
        return transform_slots.LiveCount(movable);
    }

    bool WorldSpatialState::RegisterTransform(TransformID id, bool isMovable)
    {
        if (!IsValidTransformID(id))
            return false;

        if (observer)
            observer->OnTransformRegistered(id, isMovable);

        uint32_t existing_slot = 0;
        if (TryGetTransformSlot(id, isMovable, existing_slot))
            return true;

        uint32_t other_slot = 0;
        const bool has_other_slot = TryGetTransformSlot(id, !isMovable, other_slot);

        const uint32_t slot = AcquireTransformSlot(isMovable);

        if (slot == INVALID_TRANSFORM_SLOT)
        {
            if (!transform_slot_overflow_warned)
            {
                transform_slot_overflow_warned = true;
                GLogError("[WorldSpatialState] %s变换槽预算用尽（budget=%u）—— 该变换**没有 L2W 行**"
                          "（拒绝，不扩容；请调大预算后重载）。",
                          isMovable ? "动态" : "静态",
                          GetTransformSegmentBudget(isMovable));
            }
            return false;
        }

        if (!transform_slots.Assign(isMovable, slot, id))
            return false;

        if (has_other_slot)
            ReleaseTransformSlot(!isMovable, other_slot);

        return true;
    }

    bool WorldSpatialState::MigrateTransform(TransformID id, bool toMovable)
    {
        if (!IsValidTransformID(id))
            return false;

        uint32_t old_slot = 0;
        if (!TryGetTransformSlot(id, !toMovable, old_slot))
            return true;

        const uint32_t slot = AcquireTransformSlot(toMovable);

        if (slot == INVALID_TRANSFORM_SLOT)
        {
            if (!transform_slot_overflow_warned)
            {
                transform_slot_overflow_warned = true;
                GLogError("[WorldSpatialState] %s变换槽预算用尽（budget=%u）—— Mobility 换边被拒绝，"
                          "该变换留在原段（不扩容）。",
                          toMovable ? "动态" : "静态",
                          GetTransformSegmentBudget(toMovable));
            }
            return false;
        }

        if (!transform_slots.Assign(toMovable, slot, id))
            return false;

        ReleaseTransformSlot(!toMovable, old_slot);

        ++transform_row_move_count;

        if (!transform_row_move_warned)
        {
            transform_row_move_warned = true;
            GLogWarning("[WorldSpatialState] Mobility 换边 ⇒ L2W 行号移动（transform=%u 旧槽=%u → 新段槽=%u）；"
                        "固化 ID / 预烘焙数据需重新取号。",
                        id, old_slot, slot);
        }

        if (TransformDataStorage* storage = transform_storage)
            storage->SetMobility(id, toMovable ? 1 : 0);

        return true;
    }

    void WorldSpatialState::UnregisterTransform(TransformID id)
    {
        if (!IsValidTransformID(id))
            return;

        uint32_t slot = 0;

        if (TryGetTransformSlot(id, false, slot))
            ReleaseTransformSlot(false, slot);

        if (TryGetTransformSlot(id, true, slot))
            ReleaseTransformSlot(true, slot);
    }

    TransformID WorldSpatialState::CreateTransform(EntityID owner, Mobility mobility)
    {
        TransformDataStorage* storage = transform_storage;
        if (!storage)
            return INVALID_TRANSFORM_ID;

        const TransformID id = storage->Allocate();
        if (!IsValidTransformID(id))
            return INVALID_TRANSFORM_ID;

        storage->SetOwner(id, owner);
        storage->SetMobility(id, (mobility == Mobility::Movable) ? 1 : 0);

        if (!RegisterTransform(id, mobility == Mobility::Movable))
        {
            storage->Deallocate(id);
            return INVALID_TRANSFORM_ID;
        }

        if (observer)
            observer->OnTransformMembershipChanged(id);

        return id;
    }

    void WorldSpatialState::DestroyTransform(TransformID id)
    {
        if (!IsValidTransformID(id))
            return;

        TransformDataStorage* storage = transform_storage;
        if (!storage || storage->GetGeneration(id) == 0)
            return;

        UnregisterTransform(id);

        const TransformID parent = storage->GetParent(id);
        if (IsValidTransformID(parent))
            storage->RemoveChild(parent, id);

        storage->Deallocate(id);

        if (observer)
            observer->OnTransformMembershipChanged(id);
    }

    void WorldSpatialState::NotifyTransformChanged(TransformID id, uint32_t change_mask)
    {
        if (observer)
            observer->OnTransformChanged(id, change_mask);
    }

    TransformID WorldSpatialState::GetTransformID(EntityID owner) const
    {
        const TransformDataStorage* storage = static_cast<const TransformDataStorage*>(transform_storage);
        return storage ? storage->FindByOwner(owner) : INVALID_TRANSFORM_ID;
    }

    TransformAccessor WorldSpatialState::GetTransform(TransformID id)
    {
        return TransformAccessor(transform_storage, id);
    }

    TransformAccessor WorldSpatialState::GetTransformByEntity(EntityID owner)
    {
        return GetTransform(GetTransformID(owner));
    }

    BoundingBoxAccessor WorldSpatialState::GetBoundingBoxByEntity(EntityID owner) const
    {
        // const 成员里仍要交出可变 storage 句柄（行数据由世界拥有；句柄只做定位）
        BoundingBoxDataStorage* storage = const_cast<BoundingBoxDataStorage*>(static_cast<const BoundingBoxDataStorage*>(bounding_box_storage));
        if (!storage)
            return BoundingBoxAccessor();

        return BoundingBoxAccessor(storage, storage->FindByOwner(owner));
    }

    BoundingBoxAccessor WorldSpatialState::GetOrCreateBoundingBox(EntityID owner)
    {
        BoundingBoxDataStorage* storage = bounding_box_storage;
        if (!storage)
            return BoundingBoxAccessor();

        auto id = storage->FindByOwner(owner);
        if (id == BoundingBoxDataStorage::INVALID_HANDLE)
        {
            id = storage->Allocate();
            storage->SetOwner(id, owner);
        }

        return BoundingBoxAccessor(storage, id);
    }

    void WorldSpatialState::DestroyBoundingBox(BoundingBoxDataStorage::HandleID id)
    {
        if (id == BoundingBoxDataStorage::INVALID_HANDLE)
            return;

        if (BoundingBoxDataStorage* storage = bounding_box_storage)
            storage->Deallocate(id);
    }

    void WorldSpatialState::SetEntityVisible(EntityID id, bool visible)
    {
        if (!visibility_storage || !core_context || !core_context->GetEntity(id))
            return;

        const bool changed = visibility_storage->IsDirectlyInvisible(id) == visible;
        if (visible)
            visibility_storage->SetVisible(id);
        else
            visibility_storage->SetInvisible(id);

        if (changed && observer)
            observer->OnEntityVisibilityChanged(id, visible);
    }

    bool WorldSpatialState::IsEntityVisible(EntityID id) const
    {
        const VisibilityDataStorage* storage = static_cast<const VisibilityDataStorage*>(visibility_storage);
        return storage ? !storage->IsInvisible(id) : true;
    }
}
