#pragma once

#include<glm/glm.hpp>
#include<vector>
#include<cstdint>
#include<hgl/math/geometry/AABB.h>

namespace hgl
{
    namespace ecs
    {
        using namespace hgl::math;

        /**
         * Structure of Arrays storage for bounding box data
         * Provides cache-friendly data layout for batch processing and SSBO uploads
         * 
         * Based on CMMath's BoundingVolumes.h design
         * Uses AABB (Axis-Aligned Bounding Box) from CMMath library
         */
        class BoundingBoxDataStorage
        {
        public:
            using HandleID = uint32_t;
            static constexpr HandleID INVALID_HANDLE = static_cast<HandleID>(-1);

        private:
            // SOA layout for AABB data
            std::vector<glm::vec3> minPoints;      // AABB minimum corners
            std::vector<glm::vec3> maxPoints;      // AABB maximum corners
            std::vector<glm::vec3> centers;        // AABB centers (for quick distance checks)
            std::vector<glm::vec3> extents;        // AABB half-extents (for quick size checks)
            std::vector<bool> dirtyFlags;          // Dirty flags for updates

            // Free list for handle reuse
            std::vector<HandleID> freeList;

        public:
            BoundingBoxDataStorage()
            {
                // Reserve initial capacity
                minPoints.reserve(100);
                maxPoints.reserve(100);
                centers.reserve(100);
                extents.reserve(100);
                dirtyFlags.reserve(100);
            }

            /**
             * Allocate a new bounding box slot
             * @return Handle ID for the allocated slot
             */
            HandleID Allocate()
            {
                HandleID handle;
                
                if (!freeList.empty())
                {
                    // Reuse from free list
                    handle = freeList.back();
                    freeList.pop_back();
                    
                    // Reset data
                    minPoints[handle] = glm::vec3(0.0f);
                    maxPoints[handle] = glm::vec3(0.0f);
                    centers[handle] = glm::vec3(0.0f);
                    extents[handle] = glm::vec3(0.0f);
                    dirtyFlags[handle] = true;
                }
                else
                {
                    // Allocate new slot
                    handle = static_cast<HandleID>(minPoints.size());
                    minPoints.push_back(glm::vec3(0.0f));
                    maxPoints.push_back(glm::vec3(0.0f));
                    centers.push_back(glm::vec3(0.0f));
                    extents.push_back(glm::vec3(0.0f));
                    dirtyFlags.push_back(true);
                }
                
                return handle;
            }

            /**
             * Deallocate a bounding box slot
             * @param handle Handle ID to deallocate
             */
            void Deallocate(HandleID handle)
            {
                if (handle >= minPoints.size())
                    return;
                
                freeList.push_back(handle);
            }

            /**
             * Set AABB using CMMath's AABB type (or minimal fallback)
             * @param handle Handle ID
             * @param aabb AABB from CMMath
             */
            void SetAABB(HandleID handle, const AABB& aabb)
            {
                if (handle >= minPoints.size())
                    return;
                
                minPoints[handle] = aabb.GetMin();
                maxPoints[handle] = aabb.GetMax();
                
                // Calculate and cache center and extents
                centers[handle] = (aabb.GetMin() + aabb.GetMax()) * 0.5f;
                extents[handle] = (aabb.GetMax() - aabb.GetMin()) * 0.5f;
                dirtyFlags[handle] = true;
            }

            /**
             * Set AABB using min/max points
             * @param handle Handle ID
             * @param minPoint Minimum corner
             * @param maxPoint Maximum corner
             */
            void SetAABB(HandleID handle, const glm::vec3& minPoint, const glm::vec3& maxPoint)
            {
                if (handle >= minPoints.size())
                    return;
                
                minPoints[handle] = minPoint;
                maxPoints[handle] = maxPoint;
                
                // Calculate and cache center and extents
                centers[handle] = (minPoint + maxPoint) * 0.5f;
                extents[handle] = (maxPoint - minPoint) * 0.5f;
                dirtyFlags[handle] = true;
            }

            /**
             * Get AABB as CMMath's AABB type (or minimal fallback)
             * @param handle Handle ID
             * @return AABB object
             */
            AABB GetAABB(HandleID handle) const
            {
                if (handle >= minPoints.size())
                    return AABB();
                
                return AABB(minPoints[handle], maxPoints[handle]);
            }

            /**
             * Get minimum point
             * @param handle Handle ID
             * @return Minimum corner
             */
            glm::vec3 GetMinPoint(HandleID handle) const
            {
                if (handle >= minPoints.size())
                    return glm::vec3(0.0f);
                return minPoints[handle];
            }

            /**
             * Get maximum point
             * @param handle Handle ID
             * @return Maximum corner
             */
            glm::vec3 GetMaxPoint(HandleID handle) const
            {
                if (handle >= maxPoints.size())
                    return glm::vec3(0.0f);
                return maxPoints[handle];
            }

            /**
             * Get center point
             * @param handle Handle ID
             * @return Center of AABB
             */
            glm::vec3 GetCenter(HandleID handle) const
            {
                if (handle >= centers.size())
                    return glm::vec3(0.0f);
                return centers[handle];
            }

            /**
             * Get half-extents
             * @param handle Handle ID
             * @return Half-extents of AABB
             */
            glm::vec3 GetExtents(HandleID handle) const
            {
                if (handle >= extents.size())
                    return glm::vec3(0.0f);
                return extents[handle];
            }

            /**
             * Get dirty flag
             * @param handle Handle ID
             * @return True if bounding box has been modified
             */
            bool IsDirty(HandleID handle) const
            {
                if (handle >= dirtyFlags.size())
                    return false;
                return dirtyFlags[handle];
            }

            /**
             * Clear dirty flag
             * @param handle Handle ID
             */
            void ClearDirty(HandleID handle)
            {
                if (handle >= dirtyFlags.size())
                    return;
                dirtyFlags[handle] = false;
            }

            /**
             * Get total number of allocated bounding boxes
             * @return Size of storage
             */
            size_t GetSize() const { return minPoints.size(); }

            /**
             * Direct access to min points array (for SSBO upload)
             * @return Reference to min points vector
             */
            const std::vector<glm::vec3>& GetAllMinPoints() const { return minPoints; }

            /**
             * Direct access to max points array (for SSBO upload)
             * @return Reference to max points vector
             */
            const std::vector<glm::vec3>& GetAllMaxPoints() const { return maxPoints; }

            /**
             * Direct access to centers array (for SSBO upload)
             * @return Reference to centers vector
             */
            const std::vector<glm::vec3>& GetAllCenters() const { return centers; }

            /**
             * Direct access to extents array (for SSBO upload)
             * @return Reference to extents vector
             */
            const std::vector<glm::vec3>& GetAllExtents() const { return extents; }

            /**
             * Clear all data
             */
            void Clear()
            {
                minPoints.clear();
                maxPoints.clear();
                centers.clear();
                extents.clear();
                dirtyFlags.clear();
                freeList.clear();
            }
        };
    }//namespace ecs
}//namespace hgl
