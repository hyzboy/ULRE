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

            // SOA storage handle and bound storage
            TransformDataStorage::HandleID storageHandle = TransformDataStorage::INVALID_HANDLE;
            TransformDataStorage* bound_storage = nullptr;

            glm::vec3 local_pos{0.0f};
            glm::quat local_rot{1.0f, 0.0f, 0.0f, 0.0f};
            glm::vec3 local_scale{1.0f};

            // Hierarchy (using EntityID instead of shared_ptr)
            EntityID parent_id;
            std::vector<EntityID> child_ids;

            // Cached world transform (for static objects)
            glm::mat4 cachedWorldMatrix;
            bool matrixDirty;

            // Optimization settings
            Mobility mobility;

            // ── D4：静态写入语义化 ─────────────────────────────────────────────
            // "静态物体写一次就不动"是本引擎的硬约定（静态段写一次用很久；静态级联
            // 阴影缓存的正确性前提就是"静态物体不动"）。违反约定的**运行期**写入会
            // 整段重写静态矩阵并使全部静态级联缓存失效（当帧 4 级全量重绘），
            // 因此必须留下痕迹而不是静默生效。
            //   armed  —— 由 TransformSystem 在该组件已被渲染侧消费过后置位；
            //             场景搭建期（首次 SubmitTransformUpdates 之前）写入不告警。
            //   warned —— 每组件只报一次，避免每帧刷屏。
            bool static_runtime_write_armed;
            bool static_runtime_write_warned;

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

            // ── D4：静态写入诊断（A′：把"静态写完不动"做成 API 语义）────────────
            /// 组件已被渲染侧消费过（TransformSystem 在静态段同步时置位）。
            /// 置位之后对 Static 物体的任何写入都算"运行期写"，代价是整段静态矩阵
            /// 重写 + 全部静态级联缓存失效；会动的对象应当在创建期迁到 Movable。
            void ArmStaticRuntimeWriteWarning() { static_runtime_write_armed = true; }
            bool IsStaticRuntimeWriteArmed() const { return static_runtime_write_armed; }
            /// 已经就"运行期写静态"报过一次（每组件一次，不刷屏）
            bool HasWarnedStaticRuntimeWrite() const { return static_runtime_write_warned; }

        public:

            void OnAttach() override;
            void OnDetach() override;

            /// Update world matrix if dirty
            void UpdateIfDirty();

            void MarkDirty();
            void MarkDirty(uint32_t change_mask);

        public:

            // Get the SOA storage handle for batch operations
            TransformDataStorage::HandleID GetStorageHandle() const;

            // Shared storage for all transforms (fallback)
            static std::shared_ptr<TransformDataStorage>& GetSharedStorage()
            {
                static auto storage = std::make_shared<TransformDataStorage>();
                return storage;
            }

        private:

            void EnsureStorageAllocated();
            void UpdateWorldMatrix();
            void MigrateStorage(Mobility target_mobility);
            TransformDataStorage* GetStorage() const;

            /// D4：运行期写 Static 物体的一次性告警（what = 调用方 setter 名）。
            void WarnStaticRuntimeWrite(const char *what);
        };
    }//namespace ecs
}//namespace hgl

