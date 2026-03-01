#include<hgl/ecs/components/TransformComponent.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/ComponentRecords.h>
#include<hgl/log/Log.h>
#include<hgl/graph/CameraInfo.h>
#include<hgl/graph/camera/ViewportInfo.h>
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
            // Allocate storage in the shared SOA storage with target mobility.
            storageHandle = GetSharedStorage()->Allocate();
            GetSharedStorage()->SetMobility(storageHandle, IsMovable() ? 1 : 0);
        }

        TransformComponent::~TransformComponent()
        {
            // Free storage in the corresponding SOA storage
            if (storageHandle != TransformDataStorage::INVALID_HANDLE)
            {
                GetStorage()->Deallocate(storageHandle);
            }
        }

        glm::vec3 TransformComponent::GetLocalPosition() const
        {
            return GetStorage()->GetPosition(storageHandle);
        }

        void TransformComponent::SetLocalPosition(const glm::vec3& pos)
        {
            GetStorage()->SetPosition(storageHandle, pos);
            MarkDirty(ToChangeMask(TransformChange::Position));

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
        }

        glm::quat TransformComponent::GetLocalRotation() const
        {
            return GetStorage()->GetRotation(storageHandle);
        }

        void TransformComponent::SetLocalRotation(const glm::quat& rot)
        {
            GetStorage()->SetRotation(storageHandle, rot);
            MarkDirty(ToChangeMask(TransformChange::Rotation));

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
        }

        glm::vec3 TransformComponent::GetLocalScale() const
        {
            return GetStorage()->GetScale(storageHandle);
        }

        void TransformComponent::SetLocalScale(const glm::vec3& scale)
        {
            GetStorage()->SetScale(storageHandle, scale);
            MarkDirty(ToChangeMask(TransformChange::Scale));

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
        }

        void TransformComponent::SetLocalTRS(const glm::vec3& pos, const glm::quat& rot, const glm::vec3& scale)
        {
            auto storage = GetStorage();
            storage->SetPosition(storageHandle, pos);
            storage->SetRotation(storageHandle, rot);
            storage->SetScale(storageHandle, scale);
            MarkDirty(ToChangeMask(TransformChange::LocalTRS));
        }

        glm::mat4 TransformComponent::GetLocalMatrix() const
        {
            auto storage = GetStorage();

            // 正确的TRS顺序：先构建各个矩阵，然后以正确的顺序相乘
            // T * R * S（向量应用顺序：先缩放，再旋转，最后平移）
            glm::mat4 scaleMatrix = glm::scale(glm::mat4(1.0f), storage->GetScale(storageHandle));
            glm::mat4 rotMatrix = glm::mat4_cast(storage->GetRotation(storageHandle));
            glm::mat4 transMatrix = glm::translate(glm::mat4(1.0f), storage->GetPosition(storageHandle));

            return transMatrix * rotMatrix * scaleMatrix;
        }

        glm::mat4 TransformComponent::GetWorldMatrix()
        {
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
            if (parent.IsValid() && owner_context)
            {
                Entity* parentEntity = owner_context->GetEntity(parent);
                if (parentEntity)
                {
                    auto parentTransform = parentEntity->GetComponent<TransformComponent>();
                    if (parentTransform)
                    {
                        parentTransform->AddChild(owner_id);
                    }
                }
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

        void TransformComponent::OnUpdate(float deltaTime)
        {
            (void)deltaTime;

            if (fixed_pixel_sizing_enabled && fixed_pixel_camera_info && fixed_pixel_viewport_info)
            {
                ApplyFixedPixelUniformScale(fixed_pixel_camera_info,
                                            fixed_pixel_viewport_info,
                                            fixed_pixel_diameter,
                                            fixed_pixel_reference_world_diameter,
                                            fixed_pixel_min_scale);
            }
        }

        void TransformComponent::OnAttach()
        {
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
                        parentTransform->RemoveChild(owner_entity->GetID());
                    }
                }
            }

            // Clear children
            child_ids.clear();
        }

        void TransformComponent::UpdateWorldMatrix()
        {
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

            auto storage = GetStorage();
            if (storage && storageHandle != TransformDataStorage::INVALID_HANDLE)
            {
                storage->SetWorldMatrix(storageHandle, cachedWorldMatrix);
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

        void TransformComponent::UpdateIfDirty()
        {
            if (!matrixDirty)
                return;

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

        std::shared_ptr<TransformDataStorage> TransformComponent::GetStorage() const
        {
            return GetSharedStorage();
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

        const char* TransformComponent::GetSerializationType()
        {
            return "Transform";
        }

        bool TransformComponent::SerializeToRecord(const std::shared_ptr<Component>& component,
                                                    const hgl::UnorderedMap<EntityID, int32_t>& entity_index,
                                                    ComponentRecord& out_record)
        {
            auto transform = std::dynamic_pointer_cast<TransformComponent>(component);
            if (!transform)
                return false;

            TransformRecord data{};
            data.position = ToArray3(transform->GetLocalPosition());
            data.rotation = ToArray4(transform->GetLocalRotation());
            data.scale = ToArray3(transform->GetLocalScale());
            data.movable = transform->IsMovable();

            const auto parent_id = transform->GetParentID();
            auto index = entity_index.GetValuePointer(parent_id);
            if (parent_id.IsValid() && index)
                data.parentIndex = *index;

            out_record.type = GetSerializationType();
            out_record.payload = data;
            return true;
        }

        void TransformComponent::DeserializeFromRecord(const ComponentRecord& record,
                                                        Entity* entity,
                                                        std::vector<std::pair<std::shared_ptr<TransformComponent>, int32_t>>& pending_parents)
        {
            const auto& data = std::any_cast<const TransformRecord&>(record.payload);
            auto transform = std::make_shared<TransformComponent>(data.movable ? Mobility::Movable : Mobility::Static);
            transform->SetLocalTRS(ToVec3(data.position), ToQuat(data.rotation), ToVec3(data.scale));
            entity->AddComponentInstance(transform);

            pending_parents.emplace_back(transform, data.parentIndex);
        }
    }//namespace ecs
}//namespace hgl


