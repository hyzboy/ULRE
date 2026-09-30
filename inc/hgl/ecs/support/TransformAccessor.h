#pragma once

#include<hgl/ecs/support/TransformID.h>
#include<hgl/ecs/core/EntityHandle.h>
#include<glm/glm.hpp>
#include<glm/gtc/quaternion.hpp>

namespace hgl
{
    namespace ecs
    {
        class ECSContext;
        class Entity;

        /**
         * 变换的**薄句柄**（值类型，8~24 字节，可随意拷贝）。
         *
         * 它只记「哪一行 + 哪个世界」，自身**不持有任何变换副本**：
         * 所有读写直落 `TransformDataStorage`（局部 TRS 唯一的真源），
         * 世界矩阵是派生量、由平铺求值算出（见 `UpdateDirtyWorldMatricesFlat`）。
         *
         * 这是 T8（变换 `TransformID` 化）的调用面：
         * 调用点从「拿组件 shared_ptr」改成「拿 accessor 值」，语义不变。
         */
        class TransformAccessor
        {
            TransformDataStorage *storage = nullptr;
            TransformID           id      = INVALID_TRANSFORM_ID;
            ECSContext           *context = nullptr;   ///< 用于解析 owner 实体（可为空：纯数据操作不需要）

        public:

            TransformAccessor() = default;
            TransformAccessor(TransformDataStorage *s,TransformID i,ECSContext *c=nullptr)
                : storage(s),id(i),context(c) {}

            bool IsValid() const { return storage && id != INVALID_TRANSFORM_ID; }

            TransformID           GetID() const         { return id; }
            TransformDataStorage *GetStorage() const    { return storage; }
            ECSContext           *GetContext() const    { return context; }

            /// 所属实体（真源在存储的 owners 行；不再由组件持有）
            EntityID GetOwnerID() const;

            /// 解析 owner 实体（context 为空、无 owner 或实体已销毁时返回 nullptr）
            Entity *GetOwner() const;

        public: // 局部 TRS（读写直落存储）

            glm::vec3 GetLocalPosition() const;
            void      SetLocalPosition(const glm::vec3 &pos) const;

            glm::quat GetLocalRotation() const;
            void      SetLocalRotation(const glm::quat &rot) const;

            glm::vec3 GetLocalScale() const;
            void      SetLocalScale(const glm::vec3 &scale) const;

            void      SetLocalTRS(const glm::vec3 &pos,const glm::quat &rot,const glm::vec3 &scale) const;

        public: // 世界变换（派生：局部 TRS + 父链组合）

            /// 需要时按脏标记做一次平铺求值（不逐帧全量重算）
            glm::mat4 GetWorldMatrix() const;

            glm::vec3 GetWorldPosition() const;
            void      SetWorldPosition(const glm::vec3 &pos) const;

            glm::quat GetWorldRotation() const;
            void      SetWorldRotation(const glm::quat &rot) const;

            glm::vec3 GetWorldScale() const;
            void      SetWorldScale(const glm::vec3 &scale) const;

        public: // 层级（父链真源在存储的 parent_indices / children）

            TransformID GetParent() const;
            void        SetParent(TransformID parent) const;

            const std::vector<TransformID> &GetChildren() const;
            void AddChild(TransformID child) const;
            void RemoveChild(TransformID child) const;

            /// 实体 → 本世界变换行（无变换时返回无效 accessor）
            static TransformAccessor FromOwner(TransformDataStorage *storage,EntityID owner,ECSContext *context=nullptr);

        public: // 元数据（owner / 变更版本 / D4 标记 —— 真源在存储）

            uint32_t GetChangeMask() const;
            void     ClearChangeMask() const;
            void     TouchChange(uint32_t mask) const;      ///< 版本 +1 且累积位掩码
            void     AddChangeMask(uint32_t mask) const;    ///< 仅累积位掩码（不改版本）
            uint64_t GetVersion() const;

            bool IsWriteArmed() const;                ///< D4：本行已被渲染侧消费过
            void ArmWriteWarning() const;
            bool HasWarnedWrite() const;
            void SetWriteWarned() const;

        public: // 表现层：fixed-pixel 尺寸控制（状态真源在存储，算法在这里）

            void SetFixedPixelSizingEnabled(bool enabled) const;
            bool IsFixedPixelSizingEnabled() const;
            void SetFixedPixelSizingParameters(float pixel_diameter,float reference_world_diameter,float min_scale=0.01f) const;
            void SetFixedPixelSizingContext(const hgl::graph::CameraInfo *camera_info,const hgl::graph::ViewportInfo *viewport_info) const;

            float ComputeWorldUnitsPerPixel(const hgl::graph::CameraInfo *camera_info,const hgl::graph::ViewportInfo *viewport_info) const;
            float ComputeFixedPixelUniformScale(const hgl::graph::CameraInfo *camera_info,const hgl::graph::ViewportInfo *viewport_info,float pixel_diameter,float reference_world_diameter) const;
            bool  ApplyFixedPixelUniformScale(const hgl::graph::CameraInfo *camera_info,const hgl::graph::ViewportInfo *viewport_info,float pixel_diameter,float reference_world_diameter,float min_scale=0.01f) const;

        public: // 移动性 / 脏标记

            Mobility GetMobility() const;
            void     SetMobility(Mobility m) const;
            bool     IsMovable() const { return GetMobility() == Mobility::Movable; }
            bool     IsStatic() const  { return GetMobility() == Mobility::Static; }

            bool IsDirty() const;
            void MarkDirty() const;
            void UpdateIfDirty() const;

        private: // 内部：变更记账（掩码累积 + 子级版本 bump）与 D4「运行期写静态」告警

            /// 写入本行后记账：版本 +1、累积位掩码、标脏，并逐行 bump 子/孙的 WorldMatrix。
            /// （平铺求值只算矩阵；"哪些行重传 GPU"由版本号比对决定 ⇒ 只标脏不 bump，
            ///   子节点的 L2W 行不会重传。）
            void MarkLocalChanged(uint32_t change_mask) const;

            /// D4：运行期写 Static 物体的一次性告警（每行只报一次）。
            void WarnStaticRuntimeWrite(const char *what) const;
        };
    }//namespace ecs
}//namespace hgl
