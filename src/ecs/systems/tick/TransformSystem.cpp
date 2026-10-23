#include<hgl/ecs/systems/tick/TransformSystem.h>
#include<hgl/common/RenderOptions.h>   // 编译期开关台帐（§2 校验面）
#include<hgl/common/DebugChannel.h>
#include<hgl/ecs/support/TransformAssignmentBuffer.h>
#include<hgl/graph/render/RenderContext.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/vk/VKRenderTarget.h>
#include<hgl/log/Log.h>
#include<algorithm>

namespace hgl::ecs
{
    TransformSystem::TransformSystem(const std::string& name)
        : System(name)
    {
        // Set system type and properties
        SetExecutionPhase(ExecutionPhase::TickTransform);

        // No dependencies - Transform is fundamental
    }

    TransformSystem::~TransformSystem()
    {
        if (ULRE_DEBUG_ON(Transform))
            GLogInfo("[TransformSystem] Destructor called, clearing transform_buffer...");

        // Clean up transform_buffer when system is destroyed
        // This is created by this system in EnsureTransformBuffer(), so this system owns it
        SAFE_CLEAR(transform_buffer);

        if (ULRE_DEBUG_ON(Transform))
            GLogInfo("[TransformSystem] Destructor complete");
    }


    /// T3c-2b（B1）：世界实体行表的 `row[Transform]` —— 生产者（变换侧）写自己的产物
    void TransformSystem::WriteEntityTransformRows()
    {
        if (!world)
            return;

        const uint32_t dynamic_base = GetDynamicBaseIndex();

        world->ForEachAliveEntity([&](Entity *entity)
        {
            if (!entity)
                return;

            const EntityID owner = entity->GetEntityID();
            const TransformID transform_id = world->GetTransformID(owner);

            if (!IsValidTransformID(transform_id))
            {
                world->GetEntityGPUTable().SetTransformRow(owner, 0);
                world->GetEntityGPUTable().VerifyAfterWrite(entity, "TransformSystem::WriteEntityTransformRows");   // §2.2 问题 2(a)
                return;
            }

            const TransformAccessor transform = world->GetTransformByEntity(owner);
            const bool movable = transform.IsMovable();
            uint32_t slot = 0;

            if (!world->TryGetTransformSlot(transform_id, movable, slot))
            {
                world->GetEntityGPUTable().SetTransformRow(owner, 0);
                world->GetEntityGPUTable().VerifyAfterWrite(entity, "TransformSystem::WriteEntityTransformRows");   // §2.2 问题 2(a)
                return;
            }

            // L2W 行号约定（具名权威 = `inc/hgl/ecs/support/TransformAssignmentBuffer.h`）：
            // 静态 = `kFirstObjectL2WSlot + slot`（行 0 恒为单位矩阵）/ 动态 = 基址(环布局) + slot
            const uint32_t row = movable ? (dynamic_base + slot) : (kFirstObjectL2WSlot + slot);

            world->GetEntityGPUTable().SetTransformRow(owner, row);
            world->GetEntityGPUTable().VerifyAfterWrite(entity, "TransformSystem::WriteEntityTransformRows");   // §2.2 问题 2(a)
        });
    }

    void TransformSystem::Update(float deltaTime)
    {
        (void)deltaTime;

        if (!world || !updateMovable)
            return;

        auto* storage = world->GetTransformStorage();
        if (storage)
        {
            storage->UpdateDirtyWorldMatricesFlat();
        }

        uint32_t total_movable = 0;
        uint32_t dirty_movable = 0;
        uint32_t updated_movable = 0;
        uint32_t skipped_by_mask = 0;
        uint32_t skipped_by_version = 0;

        const uint32_t update_mask = ToChangeMask(TransformChange::LocalTRS) |
                                     ToChangeMask(TransformChange::Position) |
                                     ToChangeMask(TransformChange::Rotation) |
                                     ToChangeMask(TransformChange::Scale) |
                                     ToChangeMask(TransformChange::Parent) |
                                     ToChangeMask(TransformChange::WorldMatrix) |
                                     ToChangeMask(TransformChange::Mobility);

        // 结构性遍历（内部跳空洞）：禁裸遍历 `movable_transforms` —— 空洞会让
        // IsDirty / GetChangeMask 以 (uint32)-1 索引平行数组（2026-10-02 实测崩溃）。
        if (storage)
            world->ForEachMovableTransform([&](TransformID id)
        {
            ++total_movable;

            const bool row_dirty = storage->IsDirty(id);
            if (row_dirty)
                ++dirty_movable;

            const bool allow_by_mask = (storage->GetChangeMask(id) & update_mask) != 0;

            if (ShouldUpdateTransform(id, update_mask))
            {
                UpdateTransformRow(id);
                MarkTransformSeen(id);
                ++updated_movable;
            }
            else
            {
                if (row_dirty)
                {
                    if (!allow_by_mask)
                        ++skipped_by_mask;
                    else
                        ++skipped_by_version;
                }
                MarkTransformSeen(id);
            }
        });

        if (ULRE_DEBUG_ON(Transform))
        {
            static uint32_t s_update_log_tick = 0;
            ++s_update_log_tick;
            if ((s_update_log_tick % 60u) == 1u)
            {
                GLogInfo("[TransformSystem] Update: movable_total=%u dirty=%u updated=%u skip_mask=%u skip_ver=%u",
                         total_movable,
                         dirty_movable,
                         updated_movable,
                         skipped_by_mask,
                         skipped_by_version);
            }
        }
    }

    void TransformSystem::UpdateStaticDirty()
    {
        if (!world)
            return;

        auto* storage = world->GetTransformStorage();
        if (storage)
        {
            storage->UpdateDirtyWorldMatricesFlat();
        }

        const uint32_t update_mask = ToChangeMask(TransformChange::LocalTRS) |
                                     ToChangeMask(TransformChange::Position) |
                                     ToChangeMask(TransformChange::Rotation) |
                                     ToChangeMask(TransformChange::Scale) |
                                     ToChangeMask(TransformChange::Parent) |
                                     ToChangeMask(TransformChange::WorldMatrix) |
                                     ToChangeMask(TransformChange::Mobility);

        // 同上：结构性遍历（内部跳空洞）
        world->ForEachStaticTransform([&](TransformID id)
        {
            if (ShouldUpdateTransform(id, update_mask))
            {
                UpdateTransformRow(id);
            }

            MarkTransformSeen(id);
        });

        static_dirty = true;
    }

    void TransformSystem::SubmitTransformUpdates()
    {
        if (!world)
            return;

        {
            const auto& static_transforms = world->GetStaticTransforms();

            const uint32_t update_mask = ToChangeMask(TransformChange::LocalTRS) |
                                         ToChangeMask(TransformChange::Position) |
                                         ToChangeMask(TransformChange::Rotation) |
                                         ToChangeMask(TransformChange::Scale) |
                                         ToChangeMask(TransformChange::Parent) |
                                         ToChangeMask(TransformChange::WorldMatrix) |
                                         ToChangeMask(TransformChange::Mobility);

            auto *static_storage = world ? world->GetTransformStorage() : nullptr;

            // 批 1.1 B4：静态变更的**独立信号**。为什么不能只看 has_dirty_static —— `Update()` 开头的
            // `UpdateDirtyWorldMatricesFlat()` 会先把 `matrixDirty` 清掉，于是正常帧序（Update → Submit）下
            // `ShouldUpdateTransform` 的第一道门 `!IsDirty(id)` 恒真 ⇒ has_dirty_static 恒假 ⇒
            // revision 永不递增（移动/删除静态物体都不会让静态级联阴影缓存失效）。
            // 存储侧的单调序号只增不减，与 per-row 脏位的生命周期解耦。
            const uint64_t static_change_seq = static_storage ? static_storage->GetStaticChangeSeq() : 0u;
            const bool static_scene_changed  = (static_change_seq != last_static_change_seq);

            bool has_dirty_static = false;

            // 结构性遍历（内部跳空洞）——裸遍历会把空洞（INVALID_TRANSFORM_ID）
            // 交给 ShouldUpdateTransform ⇒ 以 (uint32)-1 索引行存储平行数组。
            world->ForEachStaticTransform([&](TransformID id)
            {
                if (!has_dirty_static && ShouldUpdateTransform(id, update_mask))
                    has_dirty_static = true;
            });

            if (has_dirty_static || static_scene_changed)
            {
                UpdateStaticDirty();

                // D4：静态物体被判脏 ⇒ 它已被渲染侧消费（在静态列表里并被消费）。
                // 从"下一次"写入起算**运行期写**：本次（搭建/首次）写入不该告警，
                // 之后再写就会整段重写静态矩阵 + 让全部静态级联缓存失效，故此时 arm，
                // 由写入方一次性告警（WarnStaticRuntimeWrite / 存储行标记）。
                // 这个位置有两个刻意选择：①在下面的 transform_buffer 早退**之前**，
                // 无图形设备的路径（单元测试）同样成立；②只在真有静态变更时做
                // （O(N) 只在罕见事件上付，稳态每帧仍是早退）。
                if (static_storage)
                {
                    world->ForEachStaticTransform([&](TransformID id)
                    {
                        static_storage->ArmWrite(id);
                    });
                }

                // A3：静态场景 revision 递增。静态级联阴影滚动缓存的正确性前提
                // 是"缓存有效期内静态物体不动"，此检出点是其唯一权威信号源；
                // EnvironmentSystem 在 RenderPreBeginFrame 比对消费（同帧失效，
                // 当帧 prepass 即重建）。
                world->BumpStaticSceneRevision();

                // 批 1.1 B4：消费掉本次序号（重复提交不再递增 —— 静态级联不会每帧全量重建）
                last_static_change_seq = static_change_seq;
            }
        }

        EnsureTransformBuffer();
        if (!transform_buffer)
            return;

        const auto prev_static_handles = static_handles;
        const auto prev_dynamic_handles = dynamic_handles;

        RefreshHandleOrder();

        auto* storage = world ? world->GetTransformStorage() : nullptr;
        if (!storage)
            return;

        // 确保拓扑排序与世界矩阵在平铺模式下全量计算完毕
        storage->UpdateDirtyWorldMatricesFlat();

        const uint32_t static_count = GetStaticCount();
        const uint32_t dynamic_count = GetDynamicCount();
        const bool static_layout_changed = prev_static_handles != static_handles;
        const bool dynamic_layout_changed = prev_dynamic_handles != dynamic_handles;

        if (static_count != last_static_count || static_layout_changed)
            static_dirty = true;

        // Dynamic transforms are written into a per-frame ring segment.
        // Even when transform values are unchanged, current frame segment must be populated.
        const bool dynamic_force_full = (dynamic_count > 0);

        transform_buffer->EnsureBudget(static_count, dynamic_count);

        if (storage)
        {
            std::vector<uint32_t> dirty_static_indices;
            std::vector<uint32_t> dirty_dynamic_indices;
            dirty_static_indices.reserve(static_count);
            dirty_dynamic_indices.reserve(dynamic_count);

            if (static_dirty)
            {
                // 槽位表（含空洞）⇒ 只把**活着的槽位**记为脏（空洞无人引用）
                // 批 1.3 C6：统一走世界的 `ForEach*`（跳洞由世界侧保证，不再手写 `IsValidTransformID`）；
                // 槽号由 slot_map 反查（`static_slot_map[handle] = slot`，见 RefreshHandleOrder）——与旧写法同值。
                world->ForEachStaticTransform([&](TransformID id)
                {
                    const uint32_t *idx = static_slot_map.GetValuePointer(id);

                    if (idx)
                        dirty_static_indices.push_back(*idx);
                });
            }
            else
            {
                world->ForEachStaticTransform([&](TransformID id)
                {
                    const uint32_t *idx = static_slot_map.GetValuePointer(id);

                    if (!idx)
                        return;

                    const uint64_t version = storage->GetVersion(id);
                    const uint64_t *last_uploaded = last_uploaded_version.GetValuePointer(id);

                    if (!last_uploaded || *last_uploaded != version)
                    {
                        dirty_static_indices.push_back(*idx);
                        last_uploaded_version[id] = version;
                    }
                });
            }

            if (dynamic_force_full)
            {
                // 动态段每帧全量写本帧 ring 切片 ⇒ 活着的槽位全部记脏
                // 批 1.3 C6：同上，走世界的 `ForEach*`（槽号由 slot_map 反查）。
                world->ForEachMovableTransform([&](TransformID id)
                {
                    const uint32_t *idx = dynamic_slot_map.GetValuePointer(id);

                    if (idx)
                        dirty_dynamic_indices.push_back(*idx);
                });
            }
            else
            {
                world->ForEachMovableTransform([&](TransformID id)
                {
                    const uint32_t *idx = dynamic_slot_map.GetValuePointer(id);

                    if (!idx)
                        return;

                    const uint64_t version = storage->GetVersion(id);
                    const uint64_t *last_uploaded = last_uploaded_version.GetValuePointer(id);

                    if (!last_uploaded || *last_uploaded != version)
                    {
                        dirty_dynamic_indices.push_back(*idx);
                        last_uploaded_version[id] = version;
                    }
                });
            }

            if (static_dirty)
            {
                world->ForEachStaticTransform([&](TransformID id)
                {
                    last_uploaded_version[id] = storage->GetVersion(id);
                });
            }

            if (dynamic_force_full)
            {
                world->ForEachMovableTransform([&](TransformID id)
                {
                    last_uploaded_version[id] = storage->GetVersion(id);
                });
            }

            if (!dirty_static_indices.empty())
            {
                std::sort(dirty_static_indices.begin(), dirty_static_indices.end());
                dirty_static_indices.erase(std::unique(dirty_static_indices.begin(), dirty_static_indices.end()), dirty_static_indices.end());
                transform_buffer->WriteStaticDirtyIndices(*storage, world->GetStaticTransforms(), dirty_static_indices);
            }

            if (!dirty_dynamic_indices.empty())
            {
                std::sort(dirty_dynamic_indices.begin(), dirty_dynamic_indices.end());
                dirty_dynamic_indices.erase(std::unique(dirty_dynamic_indices.begin(), dirty_dynamic_indices.end()), dirty_dynamic_indices.end());
                transform_buffer->WriteDynamicDirtyIndices(*storage, world->GetMovableTransforms(), dirty_dynamic_indices);
            }

            // T3c-2b（B1）：L2W 行号下发（值变才标脏 ⇒ 本函数被同帧多次调用时幂等）
            WriteEntityTransformRows();

            static_dirty = false;
        }

        last_static_count = static_count;

        if (ULRE_DEBUG_ON(Transform))
        {
            static uint32_t s_submit_log_tick = 0;
            ++s_submit_log_tick;
            if ((s_submit_log_tick % 60u) == 1u)
            {
                uint32_t dynamic_base = transform_buffer ? transform_buffer->GetDynamicBaseIndex() : 0u;

                if (storage && dynamic_count > 0 && !dynamic_handles.empty())
                {
                    const auto handle = dynamic_handles.front();
                    const glm::mat4 world = storage->GetWorldMatrix(handle);
                    const glm::vec3 pos(world[3]);

                    GLogInfo("[TransformSystem] SubmitTransformUpdates: static=%u dynamic=%u dynamic_base=%u first_dynamic_handle=%u world_pos=(%.3f, %.3f, %.3f)",
                             static_count,
                             dynamic_count,
                             dynamic_base,
                             static_cast<uint32_t>(handle),
                             pos.x,
                             pos.y,
                             pos.z);
                }
                else
                {
                    GLogInfo("[TransformSystem] SubmitTransformUpdates: static=%u dynamic=%u dynamic_base=%u (no dynamic sample)",
                             static_count,
                             dynamic_count,
                             dynamic_base);
                }
            }
        }
    }

    void TransformSystem::EnsureTransformBuffer()
    {
        if (transform_buffer)
            return;

        if (!world)
            return;

        // W7：GetGraphicsContext() 已内置 render_ctx fallback
        auto *graphics_context = world->GetGraphicsContext();
        if (!graphics_context)
            return;

        auto buffer_manager = graphics_context->GetBufferManager();
        if (!buffer_manager)
            return;

        // Use the render target's actual frame count so the ring matches
        // the swapchain's in-flight frame count, avoiding over/under allocation.
        uint32_t ring_frames = HGL_L2W_RING_FRAMES;
        auto render_target = world->GetRenderTarget();
        if (render_target)
            ring_frames = render_target->GetFrameCount();

        // 行预算的唯一真源是世界的变换存储（T10/B1a：声明式预算 + 预分配）。
        const TransformDataStorage *storage = world->GetTransformStorage();

        const uint32_t max_static_rows  = storage ? storage->GetStaticRowBudget()  : TransformDataStorage::kDefaultMaxStaticRows;
        const uint32_t max_dynamic_rows = storage ? storage->GetDynamicRowBudget() : TransformDataStorage::kDefaultMaxDynamicRows;

        transform_buffer = new TransformAssignmentBuffer(buffer_manager,
                                                         ring_frames,
                                                         max_static_rows,
                                                         max_dynamic_rows);
        static_dirty = true;
    }

    uint32_t TransformSystem::GetDynamicBaseIndex() const
    {
        if (!transform_buffer)
            return 0;

        return transform_buffer->GetDynamicBaseIndex();
    }

    bool TransformSystem::TryGetTransformSlot(TransformDataStorage::HandleID handle, bool movable, uint32_t& out_slot) const
    {
        if (handle == TransformDataStorage::INVALID_HANDLE)
            return false;

        if (movable)
        {
            const uint32_t *index = dynamic_slot_map.GetValuePointer(handle);
            if (!index)
                return false;
            out_slot = *index;
            return true;
        }

        const uint32_t *index = static_slot_map.GetValuePointer(handle);
        if (!index)
            return false;
        out_slot = *index;
        return true;
    }

    void TransformSystem::RefreshHandleOrder()
    {
        static_handles.clear();
        dynamic_handles.clear();
        static_slot_map.Clear();
        dynamic_slot_map.Clear();

        if (!world)
            return;

        // B1b：世界的两个数组**就是槽位表**（下标 = 槽位，空洞 = INVALID）。
        // 这里只派生出"活着的 handle 列表"与 handle→槽位映射；槽位值跨帧稳定，
        // 不再像改前那样把"名单排名"当行号（那时删/加一个物体就会整体平移）。
        const auto& static_slots = world->GetStaticTransforms();
        const auto& movable_slots = world->GetMovableTransforms();

        static_handles.reserve(static_slots.size());
        dynamic_handles.reserve(movable_slots.size());

        for (uint32_t slot = 0; slot < static_cast<uint32_t>(static_slots.size()); ++slot)
        {
            const TransformID handle = static_slots[slot];

            if (!IsValidTransformID(handle))
                continue;

            static_handles.push_back(handle);
            static_slot_map[handle] = slot;
        }

        for (uint32_t slot = 0; slot < static_cast<uint32_t>(movable_slots.size()); ++slot)
        {
            const TransformID handle = movable_slots[slot];

            if (!IsValidTransformID(handle))
                continue;

            dynamic_handles.push_back(handle);
            dynamic_slot_map[handle] = slot;
        }
    }

    void TransformSystem::UpdateTransformRow(const TransformID id)
    {
        auto *storage = world ? world->GetTransformStorage() : nullptr;

        if (!storage || !IsValidTransformID(id))
            return;

        // 与旧组件路径同义：脏就做一次平铺求值，并记一次 WorldMatrix 变更
        if (storage->IsDirty(id) || storage->IsTopologyDirty())
            storage->UpdateDirtyWorldMatricesFlat();

        storage->AddChangeMask(id,ToChangeMask(TransformChange::WorldMatrix));
    }

    bool TransformSystem::ShouldUpdateTransform(const TransformID id,uint32_t update_mask)
    {
        auto *storage = world ? world->GetTransformStorage() : nullptr;

        if (!storage || !IsValidTransformID(id))
            return false;

        if (!storage->IsDirty(id))
            return false;

        if ((storage->GetChangeMask(id) & update_mask) == 0)
            return false;

        const uint64_t version = storage->GetVersion(id);
        const uint64_t *last_version = last_seen_version.GetValuePointer(id);
        if (last_version && *last_version == version)
            return false;

        return true;
    }

    void TransformSystem::MarkTransformSeen(const TransformID id)
    {
        auto *storage = world ? world->GetTransformStorage() : nullptr;

        if (!storage || !IsValidTransformID(id))
            return;

        last_seen_version[id] = storage->GetVersion(id);
    }
}//namespace hgl::ecs
