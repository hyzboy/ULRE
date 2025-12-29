#include<hgl/ecs/TransformComponent.h>
#include<algorithm>

namespace hgl
{
    namespace ecs
    {
        TransformComponent::TransformComponent(const std::string& name)
            : Component(name)
            , localPosition(0.0f, 0.0f, 0.0f)
            , localRotation(1.0f, 0.0f, 0.0f, 0.0f)
            , localScale(1.0f, 1.0f, 1.0f)
            , cachedWorldMatrix(1.0f)
            , matrixDirty(true)
            , mobility(TransformMobility::Movable)
        {
        }

        TransformComponent::~TransformComponent()
        {
        }

        void TransformComponent::SetLocalPosition(const glm::vec3& pos)
        {
            localPosition = pos;
            MarkDirty();
        }

        void TransformComponent::SetLocalRotation(const glm::quat& rot)
        {
            localRotation = rot;
            MarkDirty();
        }

        void TransformComponent::SetLocalScale(const glm::vec3& scale)
        {
            localScale = scale;
            MarkDirty();
        }

        void TransformComponent::SetLocalTRS(const glm::vec3& pos, const glm::quat& rot, const glm::vec3& scale)
        {
            localPosition = pos;
            localRotation = rot;
            localScale = scale;
            MarkDirty();
        }

        glm::mat4 TransformComponent::GetLocalMatrix() const
        {
            glm::mat4 matrix(1.0f);
            matrix = glm::translate(matrix, localPosition);
            matrix = matrix * glm::mat4_cast(localRotation);
            matrix = glm::scale(matrix, localScale);
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
            auto parent = parentEntity.lock();
            if (parent)
            {
                auto parentTransform = parent->GetComponent<TransformComponent>();
                if (parentTransform)
                {
                    glm::mat4 parentWorld = parentTransform->GetWorldMatrix();
                    glm::mat4 parentInverse = glm::inverse(parentWorld);
                    glm::vec4 localPos = parentInverse * glm::vec4(pos, 1.0f);
                    localPosition = glm::vec3(localPos);
                }
                else
                {
                    localPosition = pos;
                }
            }
            else
            {
                localPosition = pos;
            }
            MarkDirty();
        }

        glm::quat TransformComponent::GetWorldRotation()
        {
            auto parent = parentEntity.lock();
            if (parent)
            {
                auto parentTransform = parent->GetComponent<TransformComponent>();
                if (parentTransform)
                {
                    return parentTransform->GetWorldRotation() * localRotation;
                }
            }
            return localRotation;
        }

        void TransformComponent::SetWorldRotation(const glm::quat& rot)
        {
            auto parent = parentEntity.lock();
            if (parent)
            {
                auto parentTransform = parent->GetComponent<TransformComponent>();
                if (parentTransform)
                {
                    glm::quat parentRot = parentTransform->GetWorldRotation();
                    localRotation = glm::inverse(parentRot) * rot;
                }
                else
                {
                    localRotation = rot;
                }
            }
            else
            {
                localRotation = rot;
            }
            MarkDirty();
        }

        glm::vec3 TransformComponent::GetWorldScale()
        {
            auto parent = parentEntity.lock();
            if (parent)
            {
                auto parentTransform = parent->GetComponent<TransformComponent>();
                if (parentTransform)
                {
                    return parentTransform->GetWorldScale() * localScale;
                }
            }
            return localScale;
        }

        void TransformComponent::SetWorldScale(const glm::vec3& scale)
        {
            auto parent = parentEntity.lock();
            if (parent)
            {
                auto parentTransform = parent->GetComponent<TransformComponent>();
                if (parentTransform)
                {
                    glm::vec3 parentScale = parentTransform->GetWorldScale();
                    localScale = scale / parentScale;
                }
                else
                {
                    localScale = scale;
                }
            }
            else
            {
                localScale = scale;
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
