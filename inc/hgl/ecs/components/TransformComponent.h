#pragma once

#include<hgl/ecs/core/Component.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/support/TransformDataStorage.h>
#include<glm/glm.hpp>
#include<glm/gtc/quaternion.hpp>
#include<glm/gtc/matrix_transform.hpp>
#include<memory>
#include <hgl/type/UnorderedMap.h>
#include<utility>
#include<vector>
#include<cstdint>

namespace hgl::graph
{
    struct CameraInfo;
    class ViewportInfo;
}

namespace hgl
{
    namespace ecs
    {
        enum class Mobility : uint8_t
        {
            Static,
            Movable
        };

        struct ComponentRecord;

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
            Mobility mobility;

            // Fixed pixel-size mode (for gizmo/facing-quad-like controls)
            bool fixed_pixel_sizing_enabled;
            float fixed_pixel_diameter;
            float fixed_pixel_reference_world_diameter;
            float fixed_pixel_min_scale;
            const hgl::graph::CameraInfo* fixed_pixel_camera_info;
            const hgl::graph::ViewportInfo* fixed_pixel_viewport_info;

        public:

            enum class TransformChange : uint32_t
            {
                Position = 1u << 0,
                Rotation = 1u << 1,
                Scale = 1u << 2,
                Parent = 1u << 3,
                WorldMatrix = 1u << 4,
                Mobility = 1u << 5,
                LocalTRS = Position | Rotation | Scale,
            };

            static constexpr uint32_t ToChangeMask(TransformChange change)
            {
                return static_cast<uint32_t>(change);
            }

            TransformComponent(Mobility mobility, const std::string& name = "Transform");
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

            // Pixel-constant sizing helpers (useful for editor gizmos/facing quads)
            float ComputeWorldUnitsPerPixel(const hgl::graph::CameraInfo* camera_info,
                                            const hgl::graph::ViewportInfo* viewport_info);
            float ComputeFixedPixelUniformScale(const hgl::graph::CameraInfo* camera_info,
                                                const hgl::graph::ViewportInfo* viewport_info,
                                                float pixel_diameter,
                                                float reference_world_diameter);
            bool ApplyFixedPixelUniformScale(const hgl::graph::CameraInfo* camera_info,
                                             const hgl::graph::ViewportInfo* viewport_info,
                                             float pixel_diameter,
                                             float reference_world_diameter,
                                             float min_scale = 0.01f);

            void SetFixedPixelSizingEnabled(bool enabled);
            bool IsFixedPixelSizingEnabled() const { return fixed_pixel_sizing_enabled; }
            void SetFixedPixelSizingParameters(float pixel_diameter,
                                               float reference_world_diameter,
                                               float min_scale = 0.01f);
            void SetFixedPixelSizingContext(const hgl::graph::CameraInfo* camera_info,
                                            const hgl::graph::ViewportInfo* viewport_info);

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
            void SetMobility(Mobility new_mobility);
            Mobility GetMobility() const { return static_cast<Mobility>(mobility); }
            void SetMovable(bool isMovable);
            bool IsMovable() const { return mobility == Mobility::Movable; }
            bool IsStatic() const { return mobility == Mobility::Static; }
            bool IsDirty() const { return matrixDirty; }

        public:

            void OnUpdate(float deltaTime) override;
            void OnAttach() override;
            void OnDetach() override;

            /// Update world matrix if dirty
            void UpdateIfDirty();

            void MarkDirty();
            void MarkDirty(uint32_t change_mask);

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
            void MigrateStorage(Mobility target_mobility);
            std::shared_ptr<TransformDataStorage> GetStorage() const;
        };
    }//namespace ecs
}//namespace hgl

