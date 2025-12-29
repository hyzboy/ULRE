#include<hgl/ecs/TransformComponent.h>
#include<algorithm>

namespace hgl
{
    namespace ecs
    {
        TransformComponent::TransformComponent(const std::string& name)
            : Component(name)
            , cachedWorldMatrix(1.0f)
            , matrixDirty(true)
            , mobility(TransformMobility::Movable)
        {
            // Allocate storage in the shared SOA storage
            storageHandle = GetSharedStorage()->Allocate();
        }

        TransformComponent::~TransformComponent()
        {
            // Free storage in the shared SOA storage
            if (storageHandle != TransformDataStorage::INVALID_HANDLE)
            {
                GetSharedStorage()->Deallocate(storageHandle);
            }
        }

        glm::vec3 TransformComponent::GetLocalPosition() const
        {
            return GetSharedStorage()->GetPosition(storageHandle);
        }

        void TransformComponent::SetLocalPosition(const glm::vec3& pos)
        {
            GetSharedStorage()->SetPosition(storageHandle, pos);
            MarkDirty();
        }

        glm::quat TransformComponent::GetLocalRotation() const
        {
            return GetSharedStorage()->GetRotation(storageHandle);
        }

        void TransformComponent::SetLocalRotation(const glm::quat& rot)
        {
            GetSharedStorage()->SetRotation(storageHandle, rot);
            MarkDirty();
        }

        glm::vec3 TransformComponent::GetLocalScale() const
        {
            return GetSharedStorage()->GetScale(storageHandle);
        }

        void TransformComponent::SetLocalScale(const glm::vec3& scale)
        {
            GetSharedStorage()->SetScale(storageHandle, scale);
            MarkDirty();
        }

        void TransformComponent::SetLocalTRS(const glm::vec3& pos, const glm::quat& rot, const glm::vec3& scale)
        {
            auto storage = GetSharedStorage();
            storage->SetPosition(storageHandle, pos);
            storage->SetRotation(storageHandle, rot);
            storage->SetScale(storageHandle, scale);
            MarkDirty();
        }

        glm::mat4 TransformComponent::GetLocalMatrix() const
        {
            auto storage = GetSharedStorage();
            glm::mat4 matrix(1.0f);
            matrix = glm::translate(matrix, storage->GetPosition(storageHandle));
            matrix = matrix * glm::mat4_cast(storage->GetRotation(storageHandle));
            matrix = glm::scale(matrix, storage->GetScale(storageHandle));
            return matrix;
        }

        glm::mat4 TransformComponent::GetWorldMatrix()
        {
            if (matrixDirty || mobility == TransformMobility::Movable)
            {
                UpdateWorldMatrix();
            }
            return cachedWorldMatrix;
        }

        glm::vec3 TransformComponent::GetWorldPosition()
        {
            glm::mat4 worldMatrix = GetWorldMatrix();
            return glm::vec3(worldMatrix[3]);
        }

        void TransformComponent::SetWorldPosition(const glm::vec3& pos)
        {
            auto storage = GetSharedStorage();
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
            auto storage = GetSharedStorage();
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
            auto storage = GetSharedStorage();
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
            auto storage = GetSharedStorage();
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
            auto storage = GetSharedStorage();
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

        void TransformComponent::SetMobility(TransformMobility mob)
        {
            mobility = mob;
            // Update mobility in storage
            GetSharedStorage()->SetMobility(storageHandle, mob == TransformMobility::Static ? 0 : 1);
        }

        void TransformComponent::OnUpdate(float deltaTime)
        {
            // Update transform if needed
            if (matrixDirty && mobility == TransformMobility::Static)
            {
                UpdateWorldMatrix();
            }
        }

        void TransformComponent::OnAttach()
        {
            MarkDirty();
        }

        void TransformComponent::OnDetach()
        {
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
    }//namespace ecs
}//namespace hgl
