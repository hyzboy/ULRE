#pragma once

#include<glm/glm.hpp>
#include<glm/gtc/quaternion.hpp>
#include<vector>
#include<functional>
#include<cstdint>

namespace hgl
{
    namespace ecs
    {
        /**
         * Optimized SOA (Structure of Arrays) storage for transform data
         * Provides better cache performance for batch operations compared to AOS (Array of Structures)
         * Used by TransformComponent for data storage
         */
        class TransformDataStorage
        {
        public:

            using HandleID = uint32_t;
            static constexpr HandleID INVALID_HANDLE = UINT32_MAX;

        private:

            // SOA - Separate Arrays for each component
            // This layout is much more cache-friendly for batch operations
            std::vector<glm::vec3> positions;        // 12 bytes each, consecutive
            std::vector<glm::quat> rotations;        // 16 bytes each, consecutive
            std::vector<glm::vec3> scales;           // 12 bytes each, consecutive
            std::vector<glm::mat4> worldMatrices;    // 64 bytes each, consecutive
            std::vector<HandleID> parentHandles;     // 4 bytes each, consecutive
            std::vector<bool> matrixDirty;           // 1 byte each, consecutive
            std::vector<uint8_t> mobility;           // 1 byte each, consecutive (0=static, 1=movable)

        public:

            /// Allocate space for a new transform
            HandleID Allocate()
            {
                HandleID id = static_cast<HandleID>(positions.size());
                positions.emplace_back(0.0f);
                rotations.emplace_back(1.0f, 0.0f, 0.0f, 0.0f);
                scales.emplace_back(1.0f);
                worldMatrices.emplace_back(1.0f);
                parentHandles.push_back(INVALID_HANDLE);
                matrixDirty.push_back(true);
                mobility.push_back(1);  // Default to movable
                return id;
            }

            /// Free allocated space
            void Deallocate(HandleID id)
            {
                if (id >= positions.size())
                    return;

                // Mark as unused but keep data (can be reused)
                // For true removal, use compaction (defragmentation)
            }

        public: // Position accessors - SOA optimized

            glm::vec3 GetPosition(HandleID id) const
            {
                return positions[id];
            }

            void SetPosition(HandleID id, const glm::vec3& pos)
            {
                positions[id] = pos;
                matrixDirty[id] = true;
            }

        public: // Rotation accessors - SOA optimized

            glm::quat GetRotation(HandleID id) const
            {
                return rotations[id];
            }

            void SetRotation(HandleID id, const glm::quat& rot)
            {
                rotations[id] = rot;
                matrixDirty[id] = true;
            }

        public: // Scale accessors - SOA optimized

            glm::vec3 GetScale(HandleID id) const
            {
                return scales[id];
            }

            void SetScale(HandleID id, const glm::vec3& scale)
            {
                scales[id] = scale;
                matrixDirty[id] = true;
            }

        public: // Matrix accessors

            glm::mat4 GetWorldMatrix(HandleID id) const
            {
                return worldMatrices[id];
            }

            void SetWorldMatrix(HandleID id, const glm::mat4& matrix)
            {
                worldMatrices[id] = matrix;
                matrixDirty[id] = false;
            }

        public: // Parent relationship

            HandleID GetParent(HandleID id) const
            {
                return parentHandles[id];
            }

            void SetParent(HandleID id, HandleID parentId)
            {
                parentHandles[id] = parentId;
                matrixDirty[id] = true;
            }

        public: // Dirty flag

            bool IsDirty(HandleID id) const
            {
                return matrixDirty[id];
            }

            void SetDirty(HandleID id, bool dirty)
            {
                matrixDirty[id] = dirty;
            }

        public: // Mobility tracking (0 = static, 1 = movable)

            uint8_t GetMobility(HandleID id) const
            {
                return mobility[id];
            }

            void SetMobility(HandleID id, uint8_t mobilityValue)
            {
                mobility[id] = mobilityValue;
            }

        public: // Batch operations - these are much faster with SOA!

            /// Update only movable dirty transforms - optimized for static/movable separation
            /// Static objects (mobility == 0) are never updated after initialization
            void UpdateMovableDirtyMatrices(const std::function<void(HandleID, glm::vec3, glm::quat, glm::vec3)>& callback)
            {
                // Only iterate through movable objects
                for (size_t i = 0; i < matrixDirty.size(); ++i)
                {
                    // Skip static objects (mobility == 0)
                    if (mobility[i] == 0)
                        continue;

                    if (matrixDirty[i])
                    {
                        // Load from cache-friendly consecutive arrays
                        callback(static_cast<HandleID>(i), positions[i], rotations[i], scales[i]);
                        matrixDirty[i] = false;
                    }
                }
            }

            /// Update all dirty transforms - cache friendly
            /// Process all positions, then rotations, then scales
            /// NOTE: Prefer UpdateMovableDirtyMatrices() for better performance with static/movable separation
            void UpdateAllDirtyMatrices(const std::function<void(HandleID, glm::vec3, glm::quat, glm::vec3)>& callback)
            {
                // Iterate through all dirty flags
                for (size_t i = 0; i < matrixDirty.size(); ++i)
                {
                    if (matrixDirty[i])
                    {
                        // Load from cache-friendly consecutive arrays
                        callback(static_cast<HandleID>(i), positions[i], rotations[i], scales[i]);
                        matrixDirty[i] = false;
                    }
                }
            }

        public: // Batch data accessors

            /// Get all positions for batch processing
            const std::vector<glm::vec3>& GetAllPositions() const { return positions; }
            std::vector<glm::vec3>& GetAllPositions() { return positions; }

            /// Get all rotations for batch processing
            const std::vector<glm::quat>& GetAllRotations() const { return rotations; }
            std::vector<glm::quat>& GetAllRotations() { return rotations; }

            /// Get all scales for batch processing
            const std::vector<glm::vec3>& GetAllScales() const { return scales; }
            std::vector<glm::vec3>& GetAllScales() { return scales; }

            /// Get all world matrices for batch processing
            const std::vector<glm::mat4>& GetAllWorldMatrices() const { return worldMatrices; }
            std::vector<glm::mat4>& GetAllWorldMatrices() { return worldMatrices; }

        public:

            size_t GetSize() const { return positions.size(); }

            /// Clear all data
            void Clear()
            {
                positions.clear();
                rotations.clear();
                scales.clear();
                worldMatrices.clear();
                parentHandles.clear();
                matrixDirty.clear();
                mobility.clear();
            }
        };
    }//namespace ecs
}//namespace hgl
