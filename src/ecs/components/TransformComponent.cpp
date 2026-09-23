#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/log/Log.h>
#include<hgl/graph/CameraInfo.h>
#include<hgl/graph/ubo/ViewportInfo.h>
#include<algorithm>
#include<array>
#include<cmath>

namespace hgl
{
    namespace ecs
    {
        namespace
        {
            struct TransformRecord
            {
                std::array<float, 3> position{};
                std::array<float, 4> rotation{};
                std::array<float, 3> scale{};
                bool movable = true;
                int32_t parentIndex = -1;
            };

            std::array<float, 3> ToArray3(const glm::vec3& value)
            {
                return {value.x, value.y, value.z};
            }

            std::array<float, 4> ToArray4(const glm::quat& value)
            {
                return {value.x, value.y, value.z, value.w};
            }

            glm::vec3 ToVec3(const std::array<float, 3>& value)
            {
                return glm::vec3(value[0], value[1], value[2]);
            }

            glm::quat ToQuat(const std::array<float, 4>& value)
            {
                return glm::quat(value[3], value[0], value[1], value[2]);
            }
        }

        TransformComponent::TransformComponent(Mobility initial_mobility, const std::string& name)
            : Component(name)
            , storageHandle(TransformDataStorage::INVALID_HANDLE)
            , bound_storage(nullptr)
            , local_pos(0.0f)
            , local_rot(1.0f, 0.0f, 0.0f, 0.0f)
            , local_scale(1.0f)
            , cachedWorldMatrix(1.0f)
            , matrixDirty(true)
            , mobility(initial_mobility)
            , fixed_pixel_sizing_enabled(false)
            , fixed_pixel_diameter(160.0f)
            , fixed_pixel_reference_world_diameter(1.0f)
            , fixed_pixel_min_scale(0.01f)
            , fixed_pixel_camera_info(nullptr)
            , fixed_pixel_viewport_info(nullptr)
        {
        }

        TransformComponent::~TransformComponent()
        {
            // Free storage in the corresponding SOA storage
            if (storageHandle != TransformDataStorage::INVALID_HANDLE && bound_storage)
            {
                bound_storage->Deallocate(storageHandle);
            }
        }

        TransformDataStorage::HandleID TransformComponent::GetStorageHandle() const
        {
            if (storageHandle == TransformDataStorage::INVALID_HANDLE)
            {
                const_cast<TransformComponent*>(this)->EnsureStorageAllocated();
            }
            return storageHandle;
        }

        void TransformComponent::EnsureStorageAllocated()
        {
            if (storageHandle != TransformDataStorage::INVALID_HANDLE)
                return;

            auto* storage = GetStorage();
            if (storage)
            {
                bound_storage = storage;
                storageHandle = storage->Allocate();
                storage->SetLocalTRS(storageHandle, local_pos, local_rot, local_scale);
                storage->SetMobility(storageHandle, IsMovable() ? 1 : 0);
            }
        }

        glm::vec3 TransformComponent::GetLocalPosition() const
        {
            if (storageHandle != TransformDataStorage::INVALID_HANDLE && bound_storage)
                return bound_storage->GetPosition(storageHandle);
            return local_pos;
        }

        void TransformComponent::SetLocalPosition(const glm::vec3& pos)
        {
            local_pos = pos;
            if (storageHandle != TransformDataStorage::INVALID_HANDLE || owner_context)
            {
                GetStorage()->SetPosition(GetStorageHandle(), pos);
            }
            MarkDirty(ToChangeMask(TransformChange::Position));

        #if HGL_TRANSFORM_DEBUG_LOGGING
            static uint32_t s_pos_log_tick = 0;
            ++s_pos_log_tick;
            if ((s_pos_log_tick % 120u) == 1u)
            {
                GLogInfo("[TransformComponent] SetLocalPosition: owner=%u handle=%u pos=(%.3f, %.3f, %.3f) version=%llu dirty=%d",
                         owner_id.index,
                         static_cast<uint32_t>(storageHandle),
                         pos.x,
                         pos.y,
                         pos.z,
                         static_cast<unsigned long long>(GetVersion()),
                         matrixDirty ? 1 : 0);
            }
        #endif//HGL_TRANSFORM_DEBUG_LOGGING
        }

        glm::quat TransformComponent::GetLocalRotation() const
        {
            if (storageHandle != TransformDataStorage::INVALID_HANDLE && bound_storage)
                return bound_storage->GetRotation(storageHandle);
            return local_rot;
        }

        void TransformComponent::SetLocalRotation(const glm::quat& rot)
        {
            local_rot = rot;
            if (storageHandle != TransformDataStorage::INVALID_HANDLE || owner_context)
            {
                GetStorage()->SetRotation(GetStorageHandle(), rot);
            }
            MarkDirty(ToChangeMask(TransformChange::Rotation));

        #if HGL_TRANSFORM_DEBUG_LOGGING
            static uint32_t s_rot_log_tick = 0;
            ++s_rot_log_tick;
            if ((s_rot_log_tick % 180u) == 1u)
            {
                GLogInfo("[TransformComponent] SetLocalRotation: owner=%u handle=%u rot=(%.3f, %.3f, %.3f, %.3f) version=%llu dirty=%d",
                         owner_id.index,
                         static_cast<uint32_t>(storageHandle),
                         rot.w,
                         rot.x,
                         rot.y,
                         rot.z,
                         static_cast<unsigned long long>(GetVersion()),
                         matrixDirty ? 1 : 0);
            }
        #endif//HGL_TRANSFORM_DEBUG_LOGGING
        }

        glm::vec3 TransformComponent::GetLocalScale() const
        {
            if (storageHandle != TransformDataStorage::INVALID_HANDLE && bound_storage)
                return bound_storage->GetScale(storageHandle);
            return local_scale;
        }

        void TransformComponent::SetLocalScale(const glm::vec3& scale)
        {
            local_scale = scale;
            if (storageHandle != TransformDataStorage::INVALID_HANDLE || owner_context)
            {
                GetStorage()->SetScale(GetStorageHandle(), scale);
            }
            MarkDirty(ToChangeMask(TransformChange::Scale));

        #if HGL_TRANSFORM_DEBUG_LOGGING
            static uint32_t s_scale_log_tick = 0;
            ++s_scale_log_tick;
            if ((s_scale_log_tick % 180u) == 1u)
            {
                GLogInfo("[TransformComponent] SetLocalScale: owner=%u handle=%u scale=(%.3f, %.3f, %.3f) version=%llu dirty=%d",
                         owner_id.index,
                         static_cast<uint32_t>(storageHandle),
                         scale.x,
                         scale.y,
                         scale.z,
                         static_cast<unsigned long long>(GetVersion()),
                         matrixDirty ? 1 : 0);
            }
        #endif//HGL_TRANSFORM_DEBUG_LOGGING
        }

        void TransformComponent::SetLocalTRS(const glm::vec3& pos, const glm::quat& rot, const glm::vec3& scale)
        {
            local_pos = pos;
            local_rot = rot;
            local_scale = scale;
            auto* storage = GetStorage();
            if (storage)
            {
                storage->SetLocalTRS(GetStorageHandle(), pos, rot, scale);
            }
            MarkDirty(ToChangeMask(TransformChange::LocalTRS));
        }

        glm::mat4 TransformComponent::GetLocalMatrix() const
        {
            auto* storage = GetStorage();
            if (storage && storageHandle != TransformDataStorage::INVALID_HANDLE)
            {
                return storage->GetLocalMatrix(storageHandle);
            }

            glm::mat4 scaleMatrix = glm::scale(glm::mat4(1.0f), local_scale);
            glm::mat4 rotMatrix = glm::mat4_cast(local_rot);
            glm::mat4 transMatrix = glm::translate(glm::mat4(1.0f), local_pos);
            return transMatrix * rotMatrix * scaleMatrix;
        }

        glm::mat4 TransformComponent::GetWorldMatrix()
        {
            auto* storage = GetStorage();
            if (storage && storageHandle != TransformDataStorage::INVALID_HANDLE)
            {
                if (matrixDirty || storage->IsDirty(storageHandle) || storage->IsTopologyDirty())
                {
                    storage->UpdateDirtyWorldMatricesFlat();
                    matrixDirty = false;
                }
                cachedWorldMatrix = storage->GetWorldMatrix(storageHandle);
                return cachedWorldMatrix;
            }

            if (matrixDirty)
                UpdateIfDirty();
            return cachedWorldMatrix;
        }

        glm::vec3 TransformComponent::GetWorldPosition()
        {
            glm::mat4 worldMatrix = GetWorldMatrix();
            return glm::vec3(worldMatrix[3]);
        }

        void TransformComponent::SetWorldPosition(const glm::vec3& pos)
        {
            auto storage = GetStorage();
            Entity* parent = owner_context ? owner_context->GetEntity(parent_id) : nullptr;
            if (parent)
            {
                auto parentTransform = parent->GetComponent<TransformComponent>();
                if (parentTransform)
                {
                    glm::mat4 parentWorld = parentTransform->GetWorldMatrix();
                    glm::mat4 parentInverse = glm::inverse(parentWorld);
                    glm::vec4 localPos = parentInverse * glm::vec4(pos, 1.0f);
                    storage->SetPosition(storageHandle, glm::vec3(localPos));
                }
                else
                {
                    storage->SetPosition(storageHandle, pos);
                }
            }
            else
            {
                storage->SetPosition(storageHandle, pos);
            }
            MarkDirty(ToChangeMask(TransformChange::Position));
        }

        glm::quat TransformComponent::GetWorldRotation()
        {
            auto storage = GetStorage();
            Entity* parent = owner_context ? owner_context->GetEntity(parent_id) : nullptr;
            if (parent)
            {
                auto parentTransform = parent->GetComponent<TransformComponent>();
                if (parentTransform)
                {
                    return parentTransform->GetWorldRotation() * storage->GetRotation(storageHandle);
                }
            }
            return storage->GetRotation(storageHandle);
        }

        void TransformComponent::SetWorldRotation(const glm::quat& rot)
        {
            auto storage = GetStorage();
            Entity* parent = owner_context ? owner_context->GetEntity(parent_id) : nullptr;
            if (parent)
            {
                auto parentTransform = parent->GetComponent<TransformComponent>();
                if (parentTransform)
                {
                    glm::quat parentRot = parentTransform->GetWorldRotation();
                    storage->SetRotation(storageHandle, glm::inverse(parentRot) * rot);
                }
                else
                {
                    storage->SetRotation(storageHandle, rot);
                }
            }
            else
            {
                storage->SetRotation(storageHandle, rot);
            }
            MarkDirty(ToChangeMask(TransformChange::Rotation));
        }

        glm::vec3 TransformComponent::GetWorldScale()
        {
            auto storage = GetStorage();
            Entity* parent = owner_context ? owner_context->GetEntity(parent_id) : nullptr;
            if (parent)
            {
                auto parentTransform = parent->GetComponent<TransformComponent>();
                if (parentTransform)
                {
                    return parentTransform->GetWorldScale() * storage->GetScale(storageHandle);
                }
            }
            return storage->GetScale(storageHandle);
        }

        void TransformComponent::SetWorldScale(const glm::vec3& scale)
        {
            auto storage = GetStorage();
            Entity* parent = owner_context ? owner_context->GetEntity(parent_id) : nullptr;
            if (parent)
            {
                auto parentTransform = parent->GetComponent<TransformComponent>();
                if (parentTransform)
                {
                    glm::vec3 parentScale = parentTransform->GetWorldScale();
                    storage->SetScale(storageHandle, scale / parentScale);
                }
                else
                {
                    storage->SetScale(storageHandle, scale);
                }
            }
            else
            {
                storage->SetScale(storageHandle, scale);
            }
            MarkDirty(ToChangeMask(TransformChange::Scale));
        }

        float TransformComponent::ComputeWorldUnitsPerPixel(const hgl::graph::CameraInfo* camera_info,
                                                            const hgl::graph::ViewportInfo* viewport_info)
        {
            if (!camera_info || !viewport_info)
                return 0.0f;

            const float viewport_height = float(viewport_info->GetViewportHeight() > 0 ? viewport_info->GetViewportHeight() : 1u);
            const float proj_11 = std::fabs(camera_info->projection[1][1]);
            if (proj_11 <= 1e-6f)
                return 0.0f;

            const bool is_ortho = std::fabs(camera_info->projection[3][3] - 1.0f) < 1e-6f;

            if (is_ortho)
            {
                const float world_height = 2.0f / proj_11;
                return world_height / viewport_height;
            }

            const float tan_half_fovy = 1.0f / proj_11;
            const glm::vec3 world_pos = GetWorldPosition();
            glm::vec3 to_object = world_pos - camera_info->pos;

            float depth = std::fabs(glm::dot(to_object, camera_info->view_line));
            if (depth <= 1e-4f)
                depth = glm::length(to_object);
            if (depth <= 1e-4f)
                depth = 1.0f;

            const float world_height = 2.0f * depth * tan_half_fovy;
            return world_height / viewport_height;
        }

        float TransformComponent::ComputeFixedPixelUniformScale(const hgl::graph::CameraInfo* camera_info,
                                                                const hgl::graph::ViewportInfo* viewport_info,
                                                                float pixel_diameter,
                                                                float reference_world_diameter)
        {
            if (pixel_diameter <= 0.0f || reference_world_diameter <= 1e-6f)
                return 0.0f;

            const float world_per_pixel = ComputeWorldUnitsPerPixel(camera_info, viewport_info);
            if (world_per_pixel <= 0.0f)
                return 0.0f;

            const float target_world_diameter = pixel_diameter * world_per_pixel;
            return target_world_diameter / reference_world_diameter;
        }

        bool TransformComponent::ApplyFixedPixelUniformScale(const hgl::graph::CameraInfo* camera_info,
                                                             const hgl::graph::ViewportInfo* viewport_info,
                                                             float pixel_diameter,
                                                             float reference_world_diameter,
                                                             float min_scale)
        {
            float scale = ComputeFixedPixelUniformScale(camera_info,
                                                        viewport_info,
                                                        pixel_diameter,
                                                        reference_world_diameter);
            if (scale <= 0.0f)
                return false;

            if (scale < min_scale)
                scale = min_scale;

            const glm::vec3 current_scale = GetLocalScale();
            if (std::fabs(current_scale.x - scale) > 1e-5f ||
                std::fabs(current_scale.y - scale) > 1e-5f ||
                std::fabs(current_scale.z - scale) > 1e-5f)
            {
                SetLocalScale(glm::vec3(scale));
            }

            return true;
        }

        void TransformComponent::SetFixedPixelSizingEnabled(bool enabled)
        {
            fixed_pixel_sizing_enabled = enabled;
        }

        void TransformComponent::SetFixedPixelSizingParameters(float pixel_diameter,
                                                               float reference_world_diameter,
                                                               float min_scale)
        {
            if (pixel_diameter > 0.0f)
                fixed_pixel_diameter = pixel_diameter;

            if (reference_world_diameter > 1e-6f)
                fixed_pixel_reference_world_diameter = reference_world_diameter;

            if (min_scale > 0.0f)
                fixed_pixel_min_scale = min_scale;
        }

        void TransformComponent::SetFixedPixelSizingContext(const hgl::graph::CameraInfo* camera_info,
                                                            const hgl::graph::ViewportInfo* viewport_info)
        {
            fixed_pixel_camera_info = camera_info;
            fixed_pixel_viewport_info = viewport_info;

            if (fixed_pixel_sizing_enabled && fixed_pixel_camera_info && fixed_pixel_viewport_info)
            {
                ApplyFixedPixelUniformScale(fixed_pixel_camera_info,
                                            fixed_pixel_viewport_info,
                                            fixed_pixel_diameter,
                                            fixed_pixel_reference_world_diameter,
                                            fixed_pixel_min_scale);
            }
        }

        void TransformComponent::SetParent(EntityID parent)
        {
            // Remove from old parent
            if (parent_id.IsValid() && owner_context)
            {
                Entity* oldParentEntity = owner_context->GetEntity(parent_id);
                if (oldParentEntity)
                {
                    auto oldParentTransform = oldParentEntity->GetComponent<TransformComponent>();
                    if (oldParentTransform)
                    {
                        oldParentTransform->RemoveChild(owner_id);
                    }
                }
            }

            // Set new parent
            parent_id = parent;
            TransformDataStorage::HandleID parent_storage_handle = TransformDataStorage::INVALID_HANDLE;
            if (parent.IsValid() && owner_context)
            {
                Entity* parentEntity = owner_context->GetEntity(parent);
                if (parentEntity)
                {
                    auto parentTransform = parentEntity->GetComponent<TransformComponent>();
                    if (parentTransform)
                    {
                        parentTransform->AddChild(owner_id);
                        parent_storage_handle = parentTransform->GetStorageHandle();
                    }
                }
            }

            if (auto* storage = GetStorage())
            {
                storage->SetParent(GetStorageHandle(), parent_storage_handle);
            }

            MarkDirty(ToChangeMask(TransformChange::Parent) | ToChangeMask(TransformChange::WorldMatrix));
        }

        Entity* TransformComponent::GetParent() const
        {
            if (!owner_context || !parent_id.IsValid())
                return nullptr;
            return owner_context->GetEntity(parent_id);
        }

        void TransformComponent::AddChild(EntityID child)
        {
            if (!child.IsValid())
                return;

            // Check if already a child
            auto it = std::find(child_ids.begin(), child_ids.end(), child);
            if (it == child_ids.end())
            {
                child_ids.push_back(child);
            }
        }

        void TransformComponent::RemoveChild(EntityID child)
        {
            if (!child.IsValid())
                return;

            auto it = std::find(child_ids.begin(), child_ids.end(), child);
            if (it != child_ids.end())
            {
                child_ids.erase(it);
            }
        }

        void TransformComponent::GetChildEntities(std::vector<Entity*>& out) const
        {
            out.clear();
            if (!owner_context)
                return;

            for (const EntityID& child_id : child_ids)
            {
                Entity* entity = owner_context->GetEntity(child_id);
                if (entity)
                    out.push_back(entity);
            }
        }

        void TransformComponent::SetMobility(Mobility new_mobility)
        {
            MigrateStorage(static_cast<Mobility>(new_mobility));
        }

        void TransformComponent::SetMovable(bool isMovable)
        {
            SetMobility(isMovable ? Mobility::Movable : Mobility::Static);
        }

        // 原 OnUpdate 每帧重算 fixed_pixel 的通道已删除：gizmo 的
        // TransformGizmoSystem 在 TickPostCamera 相位每帧调
        // SetFixedPixelSizingContext（设置时即时 Apply），是主通道。

        void TransformComponent::OnAttach()
        {
            if (owner_context)
            {
                auto* target_storage = owner_context->GetTransformStorage();
                if (target_storage)
                {
                    if (bound_storage != target_storage)
                    {
                        if (bound_storage && storageHandle != TransformDataStorage::INVALID_HANDLE)
                        {
                            bound_storage->Deallocate(storageHandle);
                        }
                        bound_storage = target_storage;
                        storageHandle = target_storage->Allocate();
                    }
                    target_storage->SetLocalTRS(storageHandle, local_pos, local_rot, local_scale);
                    target_storage->SetMobility(storageHandle, IsMovable() ? 1 : 0);

                    if (parent_id.IsValid())
                    {
                        if (Entity* parent_entity = owner_context->GetEntity(parent_id))
                        {
                            if (auto parent_tc = parent_entity->GetComponent<TransformComponent>())
                            {
                                target_storage->SetParent(storageHandle, parent_tc->GetStorageHandle());
                            }
                        }
                    }
                }
            }

            MarkDirty(ToChangeMask(TransformChange::LocalTRS));

            // Register with context
            if (auto owner = GetOwner())
            {
                if (auto ctx = owner->GetContext())
                {
                    ctx->RegisterTransformComponent(std::static_pointer_cast<TransformComponent>(shared_from_this()), IsMovable());
                }
            }
        }

        void TransformComponent::OnDetach()
        {
            // Unregister from context
            if (auto owner = GetOwner())
            {
                if (auto ctx = owner->GetContext())
                {
                    ctx->UnregisterTransformComponent(this);
                }
            }

            if (bound_storage && storageHandle != TransformDataStorage::INVALID_HANDLE)
            {
                bound_storage->Deallocate(storageHandle);
                storageHandle = TransformDataStorage::INVALID_HANDLE;
                bound_storage = nullptr;
            }

            // Remove from parent
            Entity* parent = owner_context ? owner_context->GetEntity(parent_id) : nullptr;
            if (parent)
            {
                auto parentTransform = parent->GetComponent<TransformComponent>();
                if (parentTransform)
                {
                    Entity* owner_entity = owner_context ? owner_context->GetEntity(owner_id) : nullptr;
                    if (owner_entity)
                    {
                        parentTransform->RemoveChild(owner_entity->GetEntityID());
                    }
                }
            }

            // Clear children
            child_ids.clear();
        }

        void TransformComponent::UpdateWorldMatrix()
        {
            auto* storage = GetStorage();
            if (storage && storageHandle != TransformDataStorage::INVALID_HANDLE)
            {
                if (matrixDirty || storage->IsDirty(storageHandle) || storage->IsTopologyDirty())
                {
                    storage->UpdateDirtyWorldMatricesFlat();
                }
                cachedWorldMatrix = storage->GetWorldMatrix(storageHandle);
                matrixDirty = false;
                AddChangeMask(ToChangeMask(TransformChange::WorldMatrix));
                return;
            }

            glm::mat4 localMatrix = GetLocalMatrix();

            Entity* parent = owner_context ? owner_context->GetEntity(parent_id) : nullptr;
            if (parent)
            {
                auto parentTransform = parent->GetComponent<TransformComponent>();
                if (parentTransform)
                {
                    cachedWorldMatrix = parentTransform->GetWorldMatrix() * localMatrix;
                }
                else
                {
                    cachedWorldMatrix = localMatrix;
                }
            }
            else
            {
                cachedWorldMatrix = localMatrix;
            }

            matrixDirty = false;

            AddChangeMask(ToChangeMask(TransformChange::WorldMatrix));

            // Mark children as dirty
            if (owner_context)
            {
                for (const EntityID& child_id : child_ids)
                {
                    Entity* child = owner_context->GetEntity(child_id);
                    if (child)
                    {
                        auto childTransform = child->GetComponent<TransformComponent>();
                        if (childTransform)
                        {
                            childTransform->MarkDirty();
                        }
                    }
                }
            }
        }

        void TransformComponent::UpdateIfDirty()
        {
            if (!matrixDirty)
                return;

            auto* storage = GetStorage();
            if (storage && storageHandle != TransformDataStorage::INVALID_HANDLE)
            {
                UpdateWorldMatrix();
                return;
            }

            Entity* parent = owner_context ? owner_context->GetEntity(parent_id) : nullptr;
            if (parent)
            {
                auto parentTransform = parent->GetComponent<TransformComponent>();
                if (parentTransform && parentTransform->IsMovable())
                {
                    parentTransform->UpdateIfDirty();
                }
            }

            UpdateWorldMatrix();
        }

        void TransformComponent::MarkDirty()
        {
            MarkDirty(ToChangeMask(TransformChange::WorldMatrix));
        }

        void TransformComponent::MarkDirty(uint32_t change_mask)
        {
            TouchChange(change_mask);
            matrixDirty = true;

            auto* storage = GetStorage();
            if (storage && storageHandle != TransformDataStorage::INVALID_HANDLE)
            {
                storage->SetDirty(storageHandle, true);
            }

            // Mark children as dirty
            if (owner_context)
            {
                for (const EntityID& child_id : child_ids)
                {
                    Entity* child = owner_context->GetEntity(child_id);
                    if (child)
                    {
                        auto childTransform = child->GetComponent<TransformComponent>();
                        if (childTransform)
                        {
                            childTransform->MarkDirty();
                        }
                    }
                }
            }
        }

        TransformDataStorage* TransformComponent::GetStorage() const
        {
            if (bound_storage)
                return bound_storage;
            if (owner_context)
            {
                if (auto* s = owner_context->GetTransformStorage())
                    return s;
            }
            return GetSharedStorage().get();
        }

        void TransformComponent::MigrateStorage(Mobility target_mobility)
        {
            if (mobility == target_mobility)
                return;

            const bool to_movable = (target_mobility == Mobility::Movable);

            TouchChange(ToChangeMask(TransformChange::Mobility));

            if (storageHandle == TransformDataStorage::INVALID_HANDLE)
            {
                mobility = target_mobility;
                return;
            }

            auto storage = GetStorage();
            storage->SetMobility(storageHandle, to_movable ? 1 : 0);
            mobility = target_mobility;

            // If transitioning to static and dirty, compute world matrix once
            if (!to_movable && matrixDirty)
            {
                UpdateWorldMatrix();
            }

            // Notify context of migration
            if (auto owner = GetOwner())
            {
                if (auto ctx = owner->GetContext())
                {
                    ctx->MigrateTransformComponent(this, to_movable);
                }
            }
        }
    }//namespace ecs
}//namespace hgl

