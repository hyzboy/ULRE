#include<hgl/ecs/TransformComponent.h>
#include<hgl/ecs/Context.h>
#include<algorithm>

namespace hgl
{
    namespace ecs
    {
        TransformComponent::TransformComponent(const std::string& name)
            : Component(name)
            , cachedWorldMatrix(1.0f)
            , matrixDirty(true)
            , movable(true)
        {
            // Allocate storage in the dynamic SOA storage by default
            storageHandle = GetDynamicStorage()->Allocate();
            GetDynamicStorage()->SetMobility(storageHandle, 1);
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
            MarkDirty();
        }

        glm::quat TransformComponent::GetLocalRotation() const
        {
            return GetStorage()->GetRotation(storageHandle);
        }

        void TransformComponent::SetLocalRotation(const glm::quat& rot)
        {
            GetStorage()->SetRotation(storageHandle, rot);
            MarkDirty();
        }

        glm::vec3 TransformComponent::GetLocalScale() const
        {
            return GetStorage()->GetScale(storageHandle);
        }

        void TransformComponent::SetLocalScale(const glm::vec3& scale)
        {
            GetStorage()->SetScale(storageHandle, scale);
            MarkDirty();
        }

        void TransformComponent::SetLocalTRS(const glm::vec3& pos, const glm::quat& rot, const glm::vec3& scale)
        {
            auto storage = GetStorage();
            storage->SetPosition(storageHandle, pos);
            storage->SetRotation(storageHandle, rot);
            storage->SetScale(storageHandle, scale);
            MarkDirty();
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
            auto parent = parentEntity.lock();
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
            MarkDirty();
        }

        glm::quat TransformComponent::GetWorldRotation()
        {
            auto storage = GetStorage();
            auto parent = parentEntity.lock();
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
            auto parent = parentEntity.lock();
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
            MarkDirty();
        }

        glm::vec3 TransformComponent::GetWorldScale()
        {
            auto storage = GetStorage();
            auto parent = parentEntity.lock();
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
            auto parent = parentEntity.lock();
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
            MarkDirty();
        }

        void TransformComponent::SetParent(std::shared_ptr<Entity> parent)
        {
            // Remove from old parent
            auto oldParent = parentEntity.lock();
            if (oldParent)
            {
                auto oldParentTransform = oldParent->GetComponent<TransformComponent>();
                if (oldParentTransform)
                {
                    oldParentTransform->RemoveChild(owner.lock());
                }
            }

            // Set new parent
            parentEntity = parent;
            if (parent)
            {
                auto parentTransform = parent->GetComponent<TransformComponent>();
                if (parentTransform)
                {
                    parentTransform->AddChild(owner.lock());
                }
            }

            MarkDirty();
        }

        void TransformComponent::AddChild(std::shared_ptr<Entity> child)
        {
            if (!child)
                return;

            // Check if already a child
            auto it = std::find(childEntities.begin(), childEntities.end(), child);
            if (it == childEntities.end())
            {
                childEntities.push_back(child);
            }
        }

        void TransformComponent::RemoveChild(std::shared_ptr<Entity> child)
        {
            if (!child)
                return;

            auto it = std::find(childEntities.begin(), childEntities.end(), child);
            if (it != childEntities.end())
            {
                childEntities.erase(it);
            }
        }

        void TransformComponent::SetMovable(bool isMovable)
        {
            if (movable == isMovable)
                return;

            MigrateStorage(isMovable);
        }

        void TransformComponent::OnUpdate(float deltaTime)
        {
            (void)deltaTime;
        }

        void TransformComponent::OnAttach()
        {
            MarkDirty();

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
            auto parent = parentEntity.lock();
            if (parent)
            {
                auto parentTransform = parent->GetComponent<TransformComponent>();
                if (parentTransform)
                {
                    parentTransform->RemoveChild(owner.lock());
                }
            }

            // Clear children
            childEntities.clear();
        }

        void TransformComponent::UpdateWorldMatrix()
        {
            glm::mat4 localMatrix = GetLocalMatrix();

            auto parent = parentEntity.lock();
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

            auto storage = GetStorage();
            if (storage && storageHandle != TransformDataStorage::INVALID_HANDLE)
            {
                storage->SetWorldMatrix(storageHandle, cachedWorldMatrix);
            }

            // Mark children as dirty
            for (auto& child : childEntities)
            {
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

        void TransformComponent::UpdateIfDirty()
        {
            if (!matrixDirty)
                return;

            auto parent = parentEntity.lock();
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
            matrixDirty = true;

            // Mark children as dirty
            for (auto& child : childEntities)
            {
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

        std::shared_ptr<TransformDataStorage> TransformComponent::GetStorage() const
        {
            return movable ? GetDynamicStorage() : GetStaticStorage();
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

            auto oldStorage = GetStorage();
            auto newStorage = toMovable ? GetDynamicStorage() : GetStaticStorage();

            TransformDataStorage::HandleID newHandle = newStorage->Allocate();

            // Copy transform data
            newStorage->SetPosition(newHandle, oldStorage->GetPosition(storageHandle));
            newStorage->SetRotation(newHandle, oldStorage->GetRotation(storageHandle));
            newStorage->SetScale(newHandle, oldStorage->GetScale(storageHandle));

            // Update mobility in new storage (0 = static, 1 = movable)
            newStorage->SetMobility(newHandle, toMovable ? 1 : 0);

            // Release old storage
            oldStorage->Deallocate(storageHandle);

            storageHandle = newHandle;
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
    }//namespace ecs
}//namespace hgl
