#pragma once

#include<hgl/ecs/core/Component.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/ecs/support/TransformDataStorage.h>
#include<hgl/ecs/support/TransformID.h>
#include<hgl/ecs/support/TransformAccessor.h>
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

            // 局部 TRS、世界矩阵、层级、变更版本、D4 标记的**唯一真源**都是 TransformDataStorage；
            // 本组件不再持有任何副本（历史上 local_pos/rot/scale、cachedWorldMatrix、matrixDirty、
            // child_ids、static_runtime_write_* 都曾在这里，改动只写一份就会出现读写不一致）。
            // 本组件剩下的自有数据：parent_id（对外是 EntityID 形式的父实体）与 fixed-pixel 参数。

            // Hierarchy: 父实体（EntityID 对外口径；存储里是 TransformID）
            EntityID parent_id;

            // Optimization settings
            Mobility mobility;

            // ── D4：静态写入语义化 ─────────────────────────────────────────────
            // （标记本身存在存储的 write_armed/write_warned 行里；这里只是转发 API）

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

            // 局部 TRS 的读写一律直落 SOA 存储（唯一真源）
            glm::vec3 GetLocalPosition() const;
            void SetLocalPosition(const glm::vec3& pos);

            glm::quat GetLocalRotation() const;
            void SetLocalRotation(const glm::quat& rot);

            glm::vec3 GetLocalScale() const;
            void SetLocalScale(const glm::vec3& scale);

            void SetLocalTRS(const glm::vec3& pos, const glm::quat& rot, const glm::vec3& scale);

        public:

            // World transform accessors
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

            // Helper function to get child entities as pointers
            void GetChildEntities(std::vector<Entity*>& out) const;

        public:

            // Mobility settings for optimization
            void SetMobility(Mobility new_mobility);
            Mobility GetMobility() const { return static_cast<Mobility>(mobility); }
            void SetMovable(bool isMovable);
            bool IsMovable() const { return mobility == Mobility::Movable; }
            bool IsStatic() const { return mobility == Mobility::Static; }
            bool IsDirty() const;

            /// 变更计数 / 变更位掩码：与其它变换状态一样，真源在存储（隐藏基类同名函数）
            uint64_t GetVersion() const;
            uint32_t GetChangeMask() const;

            // ── D4：静态写入诊断（A′：把"静态写完不动"做成 API 语义）────────────
            /// 该变换已被渲染侧消费过（TransformSystem 在静态段同步时置位）。
            /// 置位之后对 Static 物体的任何写入都算"运行期写"，代价是整段静态矩阵
            /// 重写 + 全部静态级联缓存失效；会动的对象应当在创建期迁到 Movable。
            void ArmStaticRuntimeWriteWarning();
            bool IsStaticRuntimeWriteArmed() const;
            /// 已经就"运行期写静态"报过一次（每组件一次，不刷屏）
            bool HasWarnedStaticRuntimeWrite() const;

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

            /// 把整棵子树的"世界矩阵变了"记到存储（孩子/孙子的 L2W 行都要重传）
            void MarkDescendantsDirty();

            /// 「存储行 + 世界」的薄句柄：数据面的唯一实现（本组件的访问器都委托给它）
            TransformAccessor GetAccessor() const;

            /// D4：运行期写 Static 物体的一次性告警（what = 调用方 setter 名）。
            void WarnStaticRuntimeWrite(const char *what);
        };
    }//namespace ecs
}//namespace hgl

