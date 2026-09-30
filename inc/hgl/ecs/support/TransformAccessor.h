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
         * 这是 T8（`TransformComponent` → `TransformID`）的调用面：
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
            void      SetLocalPosition(const glm::vec3 &pos);

            glm::quat GetLocalRotation() const;
            void      SetLocalRotation(const glm::quat &rot);

            glm::vec3 GetLocalScale() const;
            void      SetLocalScale(const glm::vec3 &scale);

            void      SetLocalTRS(const glm::vec3 &pos,const glm::quat &rot,const glm::vec3 &scale);

        public: // 世界变换（派生：局部 TRS + 父链组合）

            /// 需要时按脏标记做一次平铺求值（不逐帧全量重算）
            glm::mat4 GetWorldMatrix();

            glm::vec3 GetWorldPosition();
            void      SetWorldPosition(const glm::vec3 &pos);

            glm::quat GetWorldRotation();
            void      SetWorldRotation(const glm::quat &rot);

            glm::vec3 GetWorldScale();
            void      SetWorldScale(const glm::vec3 &scale);

        public: // 层级（父链真源在存储的 parent_indices / children）

            TransformID GetParent() const;
            void        SetParent(TransformID parent);

            const std::vector<TransformID> &GetChildren() const;
            void AddChild(TransformID child);
            void RemoveChild(TransformID child);

            /// 实体 → 本世界变换行（无变换时返回无效 accessor）
            static TransformAccessor FromOwner(TransformDataStorage *storage,EntityID owner,ECSContext *context=nullptr);

        public: // 元数据（owner / 变更版本 / D4 标记 —— 真源在存储）

            uint32_t GetChangeMask() const;
            void     ClearChangeMask();
            void     TouchChange(uint32_t mask);      ///< 版本 +1 且累积位掩码
            void     AddChangeMask(uint32_t mask);
            uint64_t GetVersion() const;

            bool IsWriteArmed() const;                ///< D4：本行已被渲染侧消费过
            void ArmWriteWarning();
            bool HasWarnedWrite() const;
            void SetWriteWarned();

        public: // 移动性 / 脏标记

            Mobility GetMobility() const;
            void     SetMobility(Mobility m);
            bool     IsMovable() const { return GetMobility() == Mobility::Movable; }
            bool     IsStatic() const  { return GetMobility() == Mobility::Static; }

            bool IsDirty() const;
            void MarkDirty();
            void UpdateIfDirty();
        };
    }//namespace ecs
}//namespace hgl
