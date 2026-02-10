#include<hgl/ecs/TransformComponent.h>
#include<hgl/ecs/Context.h>
#include<hgl/ecs/ECSComponentRecords.h>
#include<algorithm>
#include<array>

namespace hgl
{
    namespace ecs
    {
        namespace
        {
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

        TransformComponent::TransformComponent(const std::string& name)
            : Component(name)
            , cachedWorldMatrix(1.0f)
            , matrixDirty(true)
            , movable(true)
        {
            // Allocate storage in the shared SOA storage by default
            storageHandle = GetSharedStorage()->Allocate();
            GetSharedStorage()->SetMobility(storageHandle, 1);
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
        }

        glm::quat TransformComponent::GetLocalRotation() const
        {
            return GetStorage()->GetRotation(storageHandle);
        }

        void TransformComponent::SetLocalRotation(const glm::quat& rot)
        {
            GetStorage()->SetRotation(storageHandle, rot);
            MarkDirty(ToChangeMask(TransformChange::Rotation));
        }

        glm::vec3 TransformComponent::GetLocalScale() const
        {
            return GetStorage()->GetScale(storageHandle);
        }

        void TransformComponent::SetLocalScale(const glm::vec3& scale)
        {
            GetStorage()->SetScale(storageHandle, scale);
            MarkDirty(ToChangeMask(TransformChange::Scale));
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

        void TransformComponent::SetMovable(bool isMovable)
        {
            if (movable == isMovable)
                return;

            TouchChange(ToChangeMask(TransformChange::Mobility));
            MigrateStorage(isMovable);
        }

        void TransformComponent::OnUpdate(float deltaTime)
        {
            (void)deltaTime;
        }

        void TransformComponent::OnAttach()
        {
            MarkDirty(ToChangeMask(TransformChange::LocalTRS));

            // Register with context
            if (auto owner = GetOwner())
            {
                if (auto ctx = owner->GetContext())
                {
                    ctx->RegisterTransformComponent(std::static_pointer_cast<TransformComponent>(shared_from_this()), movable);
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

        void TransformComponent::MigrateStorage(bool toMovable)
        {
            if (movable == toMovable)
                return;

            if (storageHandle == TransformDataStorage::INVALID_HANDLE)
            {
                movable = toMovable;
                return;
            }

            auto storage = GetStorage();
            storage->SetMobility(storageHandle, toMovable ? 1 : 0);
            movable = toMovable;

            // If transitioning to static and dirty, compute world matrix once
            if (!toMovable && matrixDirty)
            {
                UpdateWorldMatrix();
            }

            // Notify context of migration
            if (auto owner = GetOwner())
            {
                if (auto ctx = owner->GetContext())
                {
                    ctx->MigrateTransformComponent(this, toMovable);
                }
            }
        }

        const char* TransformComponent::GetSerializationType()
        {
            return "Transform";
        }

        bool TransformComponent::SerializeToRecord(const std::shared_ptr<Component>& component,
                                                    const std::unordered_map<EntityID, int32_t>& entity_index,
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
            auto it = entity_index.find(parent_id);
            if (parent_id.IsValid() && it != entity_index.end())
                data.parentIndex = it->second;

            out_record.type = GetSerializationType();
            out_record.payload = data;
            return true;
        }

        void TransformComponent::DeserializeFromRecord(const ComponentRecord& record,
                                                        Entity* entity,
                                                        std::vector<std::pair<std::shared_ptr<TransformComponent>, int32_t>>& pending_parents)
        {
            const auto& data = std::get<TransformRecord>(record.payload);
            auto transform = std::make_shared<TransformComponent>();
            transform->SetLocalTRS(ToVec3(data.position), ToQuat(data.rotation), ToVec3(data.scale));
            entity->AddComponentInstance(transform);

            if (data.movable != transform->IsMovable())
                transform->SetMovable(data.movable);

            pending_parents.emplace_back(transform, data.parentIndex);
        }
    }//namespace ecs
}//namespace hgl
