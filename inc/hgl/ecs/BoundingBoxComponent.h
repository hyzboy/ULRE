#pragma once

#include<hgl/ecs/Component.h>
#include<hgl/ecs/Entity.h>
#include<hgl/ecs/BoundingBoxDataStorage.h>
#include<glm/glm.hpp>
#include<memory>

namespace hgl
{
    namespace ecs
    {
        /**
         * Bounding box component for spatial queries and culling
         * Uses SOA (Structure of Arrays) storage for better cache performance
         * and efficient SSBO uploads
         * 
         * Based on CMMath's BoundingVolumes.h and AIECS design patterns
         */
        class BoundingBoxComponent : public Component
        {
        private:
            // SOA storage handle
            BoundingBoxDataStorage::HandleID storageHandle = BoundingBoxDataStorage::INVALID_HANDLE;

            // Shared SOA storage (singleton)
            static std::shared_ptr<BoundingBoxDataStorage> sharedStorage;

        public:
            explicit BoundingBoxComponent(const std::string& name = "BoundingBox")
                : Component(name)
            {
                // Ensure shared storage exists
                if (!sharedStorage)
                {
                    sharedStorage = std::make_shared<BoundingBoxDataStorage>();
                }

                // Allocate storage handle
                storageHandle = sharedStorage->Allocate();
            }

            ~BoundingBoxComponent() override
            {
                // Deallocate storage handle
                if (sharedStorage && storageHandle != BoundingBoxDataStorage::INVALID_HANDLE)
                {
                    sharedStorage->Deallocate(storageHandle);
                }
            }

            /**
             * Set AABB using CMMath's AABB type (or minimal fallback)
             * @param aabb AABB from CMMath
             */
            void SetAABB(const AABB& aabb)
            {
                if (sharedStorage && storageHandle != BoundingBoxDataStorage::INVALID_HANDLE)
                {
                    sharedStorage->SetAABB(storageHandle, aabb);
                }
            }

            /**
             * Set AABB using min/max points
             * @param minPoint Minimum corner
             * @param maxPoint Maximum corner
             */
            void SetAABB(const glm::vec3& minPoint, const glm::vec3& maxPoint)
            {
                if (sharedStorage && storageHandle != BoundingBoxDataStorage::INVALID_HANDLE)
                {
                    sharedStorage->SetAABB(storageHandle, minPoint, maxPoint);
                }
            }

            /**
             * Get AABB as CMMath's AABB type (or minimal fallback)
             * @return AABB object
             */
            AABB GetAABB() const
            {
                if (sharedStorage && storageHandle != BoundingBoxDataStorage::INVALID_HANDLE)
                {
                    return sharedStorage->GetAABB(storageHandle);
                }
                return AABB();
            }

            /**
             * Get minimum point
             * @return Minimum corner
             */
            glm::vec3 GetMinPoint() const
            {
                if (sharedStorage && storageHandle != BoundingBoxDataStorage::INVALID_HANDLE)
                {
                    return sharedStorage->GetMinPoint(storageHandle);
                }
                return glm::vec3(0.0f);
            }

            /**
             * Get maximum point
             * @return Maximum corner
             */
            glm::vec3 GetMaxPoint() const
            {
                if (sharedStorage && storageHandle != BoundingBoxDataStorage::INVALID_HANDLE)
                {
                    return sharedStorage->GetMaxPoint(storageHandle);
                }
                return glm::vec3(0.0f);
            }

            /**
             * Get center point
             * @return Center of AABB
             */
            glm::vec3 GetCenter() const
            {
                if (sharedStorage && storageHandle != BoundingBoxDataStorage::INVALID_HANDLE)
                {
                    return sharedStorage->GetCenter(storageHandle);
                }
                return glm::vec3(0.0f);
            }

            /**
             * Get half-extents
             * @return Half-extents of AABB
             */
            glm::vec3 GetExtents() const
            {
                if (sharedStorage && storageHandle != BoundingBoxDataStorage::INVALID_HANDLE)
                {
                    return sharedStorage->GetExtents(storageHandle);
                }
                return glm::vec3(0.0f);
            }

            /**
             * Check if bounding box is dirty
             * @return True if modified since last clear
             */
            bool IsDirty() const
            {
                if (sharedStorage && storageHandle != BoundingBoxDataStorage::INVALID_HANDLE)
                {
                    return sharedStorage->IsDirty(storageHandle);
                }
                return false;
            }

            /**
             * Clear dirty flag
             */
            void ClearDirty()
            {
                if (sharedStorage && storageHandle != BoundingBoxDataStorage::INVALID_HANDLE)
                {
                    sharedStorage->ClearDirty(storageHandle);
                }
            }

            /**
             * Get storage handle (for advanced usage)
             * @return Handle ID in SOA storage
             */
            BoundingBoxDataStorage::HandleID GetStorageHandle() const
            {
                return storageHandle;
            }

            /**
             * Get shared SOA storage (static method)
             * @return Shared storage instance
             */
            static std::shared_ptr<BoundingBoxDataStorage> GetSharedStorage()
            {
                if (!sharedStorage)
                {
                    sharedStorage = std::make_shared<BoundingBoxDataStorage>();
                }
                return sharedStorage;
            }

            // Component lifecycle hooks
            void OnAttach() override {}
            void OnUpdate(float deltaTime) override {}
            void OnDetach() override {}
        };

        // Static member definition (needs to be in .cpp file)
        // std::shared_ptr<BoundingBoxDataStorage> BoundingBoxComponent::sharedStorage = nullptr;
    }//namespace ecs
}//namespace hgl
