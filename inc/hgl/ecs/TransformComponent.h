#pragma once

#include<hgl/ecs/Component.h>
#include<hgl/ecs/Entity.h>
#include<hgl/ecs/TransformDataStorage.h>
#include<glm/glm.hpp>
#include<glm/gtc/quaternion.hpp>
#include<glm/gtc/matrix_transform.hpp>
#include<memory>
#include<vector>

namespace hgl
{
    namespace ecs
    {
        /**
         * Transform component for spatial transformation
         * Uses SOA (Structure of Arrays) storage for better cache performance
         * while maintaining OOP component interface
         */
        class TransformComponent : public Component
        {
        private:

            // SOA storage handle
            TransformDataStorage::HandleID storageHandle = TransformDataStorage::INVALID_HANDLE;

            // Hierarchy (using EntityID instead of shared_ptr)
            EntityID parent_id;
            std::vector<EntityID> child_ids;

            // Cached world transform (for static objects)
            glm::mat4 cachedWorldMatrix;
            bool matrixDirty;

            // Optimization settings
            bool movable;  // true = movable, false = static

        public:

            TransformComponent(const std::string& name = "Transform");
            ~TransformComponent() override;

        public:

            // Local transform accessors (using SOA backend)
            glm::vec3 GetLocalPosition() const;
            void SetLocalPosition(const glm::vec3& pos);

            glm::quat GetLocalRotation() const;
            void SetLocalRotation(const glm::quat& rot);

            glm::vec3 GetLocalScale() const;
            void SetLocalScale(const glm::vec3& scale);

            void SetLocalTRS(const glm::vec3& pos, const glm::quat& rot, const glm::vec3& scale);

        public:

            // World transform accessors
            glm::mat4 GetLocalMatrix() const;
            glm::mat4 GetWorldMatrix();

            glm::vec3 GetWorldPosition();
            void SetWorldPosition(const glm::vec3& pos);

            glm::quat GetWorldRotation();
            void SetWorldRotation(const glm::quat& rot);

            glm::vec3 GetWorldScale();
            void SetWorldScale(const glm::vec3& scale);

        public:

            // Parent/Child relationships
            void SetParent(EntityID parent);
            EntityID GetParentID() const { return parent_id; }
            Entity* GetParent() const;

            void AddChild(EntityID child);
            void RemoveChild(EntityID child);
            const std::vector<EntityID>& GetChildren() const { return child_ids; }
            
            // Helper function to get child entities as pointers
            void GetChildEntities(std::vector<Entity*>& out) const;

        public:

            // Mobility settings for optimization
            void SetMovable(bool isMovable);
            bool IsMovable() const { return movable; }
            bool IsStatic() const { return !movable; }
            bool IsDirty() const { return matrixDirty; }

        public:

            void OnUpdate(float deltaTime) override;
            void OnAttach() override;
            void OnDetach() override;

            /// Update world matrix if dirty
            void UpdateIfDirty();

            void MarkDirty();

        public:

            // Get the SOA storage handle for batch operations
            TransformDataStorage::HandleID GetStorageHandle() const { return storageHandle; }

            // Shared storage for all transforms
            static std::shared_ptr<TransformDataStorage>& GetSharedStorage()
            {
                static auto storage = std::make_shared<TransformDataStorage>();
                return storage;
            }

        private:

            void UpdateWorldMatrix();
            void MigrateStorage(bool toMovable);
            std::shared_ptr<TransformDataStorage> GetStorage() const;
        };
    }//namespace ecs
}//namespace hgl
