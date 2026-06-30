#include<hgl/ecs/systems/tick/TransformSystem.h>
#include<hgl/ecs/support/TransformAssignmentBuffer.h>
#include<hgl/graph/render/RenderContext.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/vk/VKRenderTarget.h>
#include<algorithm>

namespace hgl::ecs
{
    TransformSystem::TransformSystem(const std::string& name)
        : System(name)
    {
        // Set system type and properties
        SetSystemType(SystemType::Transform);
        SetExecutionOrder(ExecutionPhase::TickTransform);

        // No dependencies - Transform is fundamental
    }

    TransformSystem::~TransformSystem()
    {
        GLogInfo("[TransformSystem] Destructor called, clearing transform_buffer...");
        SAFE_CLEAR(transform_buffer);
        GLogInfo("[TransformSystem] Destructor complete");
    }

    TransformAssignmentBuffer* TransformSystem::GetTransformBuffer() const
    {
        return transform_buffer;
    }


    void TransformSystem::Update(float deltaTime)
    {
        (void)deltaTime;

        if (!world || !updateMovable)
            return;

        const auto& movable_transforms = world->GetMovableTransforms();
        uint32_t total_movable = 0;
        uint32_t dirty_movable = 0;
        uint32_t updated_movable = 0;
        uint32_t skipped_by_mask = 0;
        uint32_t skipped_by_version = 0;

        const uint32_t update_mask = TransformComponent::ToChangeMask(TransformComponent::TransformChange::LocalTRS) |
                                     TransformComponent::ToChangeMask(TransformComponent::TransformChange::Position) |
                                     TransformComponent::ToChangeMask(TransformComponent::TransformChange::Rotation) |
                                     TransformComponent::ToChangeMask(TransformComponent::TransformChange::Scale) |
                                     TransformComponent::ToChangeMask(TransformComponent::TransformChange::Parent) |
                                     TransformComponent::ToChangeMask(TransformComponent::TransformChange::WorldMatrix) |
                                     TransformComponent::ToChangeMask(TransformComponent::TransformChange::Mobility);

        for (const auto& weak_comp : movable_transforms)
        {
            if (auto comp = weak_comp.lock())
            {
                Entity* owner = comp->GetOwner();
                if (owner && !world->IsEntityTickEnabled(owner))
                    continue;

                ++total_movable;
                if (comp->IsDirty())
                    ++dirty_movable;

                bool allow_by_mask = (comp->GetChangeMask() & update_mask) != 0;
                if (ShouldUpdateTransform(comp, update_mask))
                {
                    comp->UpdateIfDirty();
                    MarkTransformSeen(comp);
                    ++updated_movable;
                }
                else
                {
                    if (comp->IsDirty())
                    {
                        if (!allow_by_mask)
                            ++skipped_by_mask;
                        else
                            ++skipped_by_version;
                    }
                    MarkTransformSeen(comp);
                }
            }
        }

        static uint32_t s_update_log_tick = 0;
        ++s_update_log_tick;
        if ((s_update_log_tick % 60u) == 1u)
        {
            GLogDebug("[TransformSystem] Update: movable_total=%u dirty=%u updated=%u skip_mask=%u skip_ver=%u",
                      total_movable,
                      dirty_movable,
                      updated_movable,
                      skipped_by_mask,
                      skipped_by_version);
        }
    }

    void TransformSystem::UpdateStaticDirty()
    {
        if (!world)
            return;

        const auto& static_transforms = world->GetStaticTransforms();

        for (const auto& weak_comp : static_transforms)
        {
            if (auto comp = weak_comp.lock())
            {
                UpdateStaticTransformRecursive(comp);
            }
        }

        static_dirty = true;
    }

    void TransformSystem::UpdateStaticTransformRecursive(const std::shared_ptr<TransformComponent>& comp)
    {
        if (!comp)
            return;

        Entity* owner = comp->GetOwner();
        if (owner && !world->IsEntityTickEnabled(owner))
            return;

        auto parent = comp->GetParent();
        if (parent)
        {
            auto parentTransform = parent->GetComponent<TransformComponent>();
            if (parentTransform && parentTransform->IsDirty())
            {
                UpdateStaticTransformRecursive(parentTransform);
            }
        }

        const uint32_t update_mask = TransformComponent::ToChangeMask(TransformComponent::TransformChange::LocalTRS) |
                                     TransformComponent::ToChangeMask(TransformComponent::TransformChange::Position) |
                                     TransformComponent::ToChangeMask(TransformComponent::TransformChange::Rotation) |
                                     TransformComponent::ToChangeMask(TransformComponent::TransformChange::Scale) |
                                     TransformComponent::ToChangeMask(TransformComponent::TransformChange::Parent) |
                                     TransformComponent::ToChangeMask(TransformComponent::TransformChange::WorldMatrix) |
                                     TransformComponent::ToChangeMask(TransformComponent::TransformChange::Mobility);

        if (ShouldUpdateTransform(comp, update_mask))
        {
            comp->UpdateIfDirty();
            MarkTransformSeen(comp);
        }
        else
        {
            MarkTransformSeen(comp);
        }
    }

    void TransformSystem::SubmitTransformUpdates()
    {
        if (!world)
            return;

        {
            const auto& static_transforms = world->GetStaticTransforms();

            const uint32_t update_mask = TransformComponent::ToChangeMask(TransformComponent::TransformChange::LocalTRS) |
                                         TransformComponent::ToChangeMask(TransformComponent::TransformChange::Position) |
                                         TransformComponent::ToChangeMask(TransformComponent::TransformChange::Rotation) |
                                         TransformComponent::ToChangeMask(TransformComponent::TransformChange::Scale) |
                                         TransformComponent::ToChangeMask(TransformComponent::TransformChange::Parent) |
                                         TransformComponent::ToChangeMask(TransformComponent::TransformChange::WorldMatrix) |
                                         TransformComponent::ToChangeMask(TransformComponent::TransformChange::Mobility);

            bool has_dirty_static = false;

            for (const auto& weak_comp : static_transforms)
            {
                if (auto comp = weak_comp.lock())
                {
                    if (ShouldUpdateTransform(comp, update_mask))
                    {
                        has_dirty_static = true;
                        break;
                    }
                }
            }

            if (has_dirty_static)
            {
                UpdateStaticDirty();
            }
        }

        EnsureTransformBuffer();
        TransformAssignmentBuffer* transform_buffer = GetTransformBuffer();
        if (!transform_buffer)
            return;

        const auto prev_static_handles = static_handles;
        const auto prev_dynamic_handles = dynamic_handles;

        RefreshHandleOrder();

        auto storage = TransformComponent::GetSharedStorage();
        const uint32_t static_count = GetStaticCount();
        const uint32_t dynamic_count = GetDynamicCount();
        const bool static_layout_changed = prev_static_handles != static_handles;
        const bool dynamic_layout_changed = prev_dynamic_handles != dynamic_handles;

        if (static_count != last_static_count || static_layout_changed)
            static_dirty = true;

        // Dynamic transforms are written into a per-frame ring segment.
        // Even when transform values are unchanged, current frame segment must be populated.
        const bool dynamic_force_full = (dynamic_count > 0);

        transform_buffer->EnsureCapacity(static_count, dynamic_count, graph::BufferAllocPolicy::Auto);

        if (storage)
        {
            std::vector<uint32_t> dirty_static_indices;
            std::vector<uint32_t> dirty_dynamic_indices;
            dirty_static_indices.reserve(static_count);
            dirty_dynamic_indices.reserve(dynamic_count);

            const auto& static_transforms = world->GetStaticTransforms();
            const auto& movable_transforms = world->GetMovableTransforms();

            if (static_dirty)
            {
                for (uint32_t i = 0; i < static_count; ++i)
                    dirty_static_indices.push_back(i);
            }
            else
            {
                for (const auto& weak_comp : static_transforms)
                {
                    auto comp = weak_comp.lock();
                    if (!comp)
                        continue;

                    const auto handle = comp->GetStorageHandle();
                    if (handle == TransformDataStorage::INVALID_HANDLE)
                        continue;

                    const uint32_t *idx = static_index_map.GetValuePointer(handle);
                    if (!idx)
                        continue;

                    const uint64_t version = comp->GetVersion();
                    const uint64_t *last_uploaded = last_uploaded_version.GetValuePointer(handle);
                    if (!last_uploaded || *last_uploaded != version)
                    {
                        dirty_static_indices.push_back(*idx);
                        last_uploaded_version[handle] = version;
                    }
                }
            }

            if (dynamic_force_full)
            {
                for (uint32_t i = 0; i < dynamic_count; ++i)
                    dirty_dynamic_indices.push_back(i);
            }
            else
            {
                for (const auto& weak_comp : movable_transforms)
                {
                    auto comp = weak_comp.lock();
                    if (!comp)
                        continue;

                    const auto handle = comp->GetStorageHandle();
                    if (handle == TransformDataStorage::INVALID_HANDLE)
                        continue;

                    const uint32_t *idx = dynamic_index_map.GetValuePointer(handle);
                    if (!idx)
                        continue;

                    const uint64_t version = comp->GetVersion();
                    const uint64_t *last_uploaded = last_uploaded_version.GetValuePointer(handle);
                    if (!last_uploaded || *last_uploaded != version)
                    {
                        dirty_dynamic_indices.push_back(*idx);
                        last_uploaded_version[handle] = version;
                    }
                }
            }

            if (static_dirty)
            {
                const auto& static_transforms_for_version = world->GetStaticTransforms();
                for (const auto& weak_comp : static_transforms_for_version)
                {
                    auto comp = weak_comp.lock();
                    if (!comp)
                        continue;
                    const auto handle = comp->GetStorageHandle();
                    if (handle == TransformDataStorage::INVALID_HANDLE)
                        continue;
                    last_uploaded_version[handle] = comp->GetVersion();
                }
            }

            if (dynamic_force_full)
            {
                const auto& movable_transforms_for_version = world->GetMovableTransforms();
                for (const auto& weak_comp : movable_transforms_for_version)
                {
                    auto comp = weak_comp.lock();
                    if (!comp)
                        continue;
                    const auto handle = comp->GetStorageHandle();
                    if (handle == TransformDataStorage::INVALID_HANDLE)
                        continue;
                    last_uploaded_version[handle] = comp->GetVersion();
                }
            }

            if (!dirty_static_indices.empty())
            {
                std::sort(dirty_static_indices.begin(), dirty_static_indices.end());
                dirty_static_indices.erase(std::unique(dirty_static_indices.begin(), dirty_static_indices.end()), dirty_static_indices.end());
                transform_buffer->WriteStaticDirtyIndices(*storage, static_handles, dirty_static_indices);
            }

            if (!dirty_dynamic_indices.empty())
            {
                std::sort(dirty_dynamic_indices.begin(), dirty_dynamic_indices.end());
                dirty_dynamic_indices.erase(std::unique(dirty_dynamic_indices.begin(), dirty_dynamic_indices.end()), dirty_dynamic_indices.end());
                transform_buffer->WriteDynamicDirtyIndices(*storage, static_count, dynamic_handles, dirty_dynamic_indices);
            }

            static_dirty = false;
        }

        last_static_count = static_count;
        last_dynamic_count = dynamic_count;

        static uint32_t s_submit_log_tick = 0;
        ++s_submit_log_tick;
        if ((s_submit_log_tick % 60u) == 1u)
        {
            uint32_t dynamic_base = transform_buffer ? transform_buffer->GetDynamicBaseIndex(static_count, dynamic_count) : 0u;

            if (storage && dynamic_count > 0 && !dynamic_handles.empty())
            {
                const auto handle = dynamic_handles.front();
                const glm::mat4 world = storage->GetWorldMatrix(handle);
                const glm::vec3 pos(world[3]);

                GLogDebug("[TransformSystem] SubmitTransformUpdates: static=%u dynamic=%u dynamic_base=%u first_dynamic_handle=%u world_pos=(%.3f, %.3f, %.3f)",
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
                GLogDebug("[TransformSystem] SubmitTransformUpdates: static=%u dynamic=%u dynamic_base=%u (no dynamic sample)",
                          static_count,
                          dynamic_count,
                          dynamic_base);
            }
        }
    }

    void TransformSystem::EnsureTransformBuffer()
    {
        if (GetTransformBuffer())
            return;

        if (!world)
            return;

        auto render_ctx = world->GetRenderContext();
        auto graphics_context = render_ctx ? render_ctx->GetGraphicsContext() : nullptr;
        if (!graphics_context)
            graphics_context = world->GetGraphicsContext();

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

        transform_buffer = new TransformAssignmentBuffer(buffer_manager,
                                                         TransformAssignmentBuffer::Mode::MovableOnly,
                                                         ring_frames);

        static_dirty = true;
    }

    uint32_t TransformSystem::GetDynamicBaseIndex(const uint32_t static_count,const uint32_t dynamic_count) const
    {
        TransformAssignmentBuffer* transform_buffer = GetTransformBuffer();
        if (!transform_buffer)
            return static_count;

        return transform_buffer->GetDynamicBaseIndex(static_count, dynamic_count);
    }

    bool TransformSystem::TryGetTransformGroupIndex(TransformDataStorage::HandleID handle, bool movable, uint32_t& out_index) const
    {
        if (handle == TransformDataStorage::INVALID_HANDLE)
            return false;

        if (movable)
        {
            const uint32_t *index = dynamic_index_map.GetValuePointer(handle);
            if (!index)
                return false;
            out_index = *index;
            return true;
        }

        const uint32_t *index = static_index_map.GetValuePointer(handle);
        if (!index)
            return false;
        out_index = *index;
        return true;
    }

    void TransformSystem::RefreshHandleOrder()
    {
        static_handles.clear();
        dynamic_handles.clear();
        static_index_map.Clear();
        dynamic_index_map.Clear();

        if (!world)
            return;

        const auto& static_transforms = world->GetStaticTransforms();
        const auto& movable_transforms = world->GetMovableTransforms();

        static_handles.reserve(static_transforms.size());
        dynamic_handles.reserve(movable_transforms.size());

        for (const auto& weak_comp : static_transforms)
        {
            if (auto comp = weak_comp.lock())
            {
                const auto handle = comp->GetStorageHandle();
                if (handle == TransformDataStorage::INVALID_HANDLE)
                    continue;
                const uint32_t index = static_cast<uint32_t>(static_handles.size());
                static_handles.push_back(handle);
                static_index_map[handle] = index;
            }
        }

        for (const auto& weak_comp : movable_transforms)
        {
            if (auto comp = weak_comp.lock())
            {
                const auto handle = comp->GetStorageHandle();
                if (handle == TransformDataStorage::INVALID_HANDLE)
                    continue;
                const uint32_t index = static_cast<uint32_t>(dynamic_handles.size());
                dynamic_handles.push_back(handle);
                dynamic_index_map[handle] = index;
            }
        }
    }

    bool TransformSystem::ShouldUpdateTransform(const std::shared_ptr<TransformComponent>& comp, uint32_t update_mask)
    {
        if (!comp || !comp->IsDirty())
            return false;

        if ((comp->GetChangeMask() & update_mask) == 0)
            return false;

        const auto handle = comp->GetStorageHandle();
        if (handle == TransformDataStorage::INVALID_HANDLE)
            return true;

        const uint64_t version = comp->GetVersion();
        const uint64_t *last_version = last_seen_version.GetValuePointer(handle);
        if (last_version && *last_version == version)
            return false;

        return true;
    }

    void TransformSystem::MarkTransformSeen(const std::shared_ptr<TransformComponent>& comp)
    {
        if (!comp)
            return;

        const auto handle = comp->GetStorageHandle();
        if (handle == TransformDataStorage::INVALID_HANDLE)
            return;

        last_seen_version[handle] = comp->GetVersion();
    }
}//namespace hgl::ecs

