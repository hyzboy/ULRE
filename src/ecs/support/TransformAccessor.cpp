#include<hgl/ecs/support/TransformAccessor.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>

#include<glm/gtc/matrix_transform.hpp>

namespace hgl
{
    namespace ecs
    {
        namespace
        {
            /// 父链遍历的安全上限（正常层级远小于此；只为防脏数据成环）
            constexpr int kMaxHierarchyWalk = 256;

            inline bool InRange(const TransformDataStorage *storage,TransformID id)
            {
                return storage && id != INVALID_TRANSFORM_ID && id < static_cast<TransformID>(storage->GetCount());
            }
        }//namespace

        Entity *TransformAccessor::GetOwner() const
        {
            if (!context || !owner.IsValid())
                return nullptr;

            return context->GetEntity(owner);
        }

        // ── 局部 TRS：读写直落存储（唯一真源）────────────────────────────────

        glm::vec3 TransformAccessor::GetLocalPosition() const
        {
            return IsValid() ? storage->GetPosition(id) : glm::vec3(0.0f);
        }

        void TransformAccessor::SetLocalPosition(const glm::vec3 &pos)
        {
            if (IsValid())
                storage->SetPosition(id,pos);
        }

        glm::quat TransformAccessor::GetLocalRotation() const
        {
            return IsValid() ? storage->GetRotation(id) : glm::quat(1.0f,0.0f,0.0f,0.0f);
        }

        void TransformAccessor::SetLocalRotation(const glm::quat &rot)
        {
            if (IsValid())
                storage->SetRotation(id,rot);
        }

        glm::vec3 TransformAccessor::GetLocalScale() const
        {
            return IsValid() ? storage->GetScale(id) : glm::vec3(1.0f);
        }

        void TransformAccessor::SetLocalScale(const glm::vec3 &scale)
        {
            if (IsValid())
                storage->SetScale(id,scale);
        }

        void TransformAccessor::SetLocalTRS(const glm::vec3 &pos,const glm::quat &rot,const glm::vec3 &scale)
        {
            if (IsValid())
                storage->SetLocalTRS(id,pos,rot,scale);
        }

        // ── 层级：父链真源在存储 ──────────────────────────────────────────────

        TransformID TransformAccessor::GetParent() const
        {
            return IsValid() ? storage->GetParent(id) : INVALID_TRANSFORM_ID;
        }

        void TransformAccessor::SetParent(TransformID parent)
        {
            if (!IsValid())
                return;

            storage->SetParent(id,(parent == id) ? INVALID_TRANSFORM_ID : parent);   // 自我成环直接拒
        }

        // ── 世界变换：派生量（局部 TRS + 父链组合）────────────────────────────

        glm::mat4 TransformAccessor::GetWorldMatrix()
        {
            if (!IsValid())
                return glm::mat4(1.0f);

            if (storage->IsDirty(id) || storage->IsTopologyDirty())
                storage->UpdateDirtyWorldMatricesFlat();

            return storage->GetWorldMatrix(id);
        }

        glm::vec3 TransformAccessor::GetWorldPosition()
        {
            return glm::vec3(GetWorldMatrix()[3]);
        }

        void TransformAccessor::SetWorldPosition(const glm::vec3 &pos)
        {
            if (!IsValid())
                return;

            const TransformID parent = storage->GetParent(id);

            if (!InRange(storage,parent))
            {
                storage->SetPosition(id,pos);
                return;
            }

            const glm::mat4 parent_world = TransformAccessor(storage,parent,context).GetWorldMatrix();
            storage->SetPosition(id,glm::vec3(glm::inverse(parent_world) * glm::vec4(pos,1.0f)));
        }

        glm::quat TransformAccessor::GetWorldRotation()
        {
            if (!IsValid())
                return glm::quat(1.0f,0.0f,0.0f,0.0f);

            glm::quat world = storage->GetRotation(id);
            TransformID parent = storage->GetParent(id);

            for (int guard = 0; InRange(storage,parent) && guard < kMaxHierarchyWalk; ++guard)
            {
                world  = storage->GetRotation(parent) * world;
                parent = storage->GetParent(parent);
            }

            return world;
        }

        void TransformAccessor::SetWorldRotation(const glm::quat &rot)
        {
            if (!IsValid())
                return;

            const TransformID parent = storage->GetParent(id);

            if (!InRange(storage,parent))
            {
                storage->SetRotation(id,rot);
                return;
            }

            const glm::quat parent_world = TransformAccessor(storage,parent,context).GetWorldRotation();
            storage->SetRotation(id,glm::inverse(parent_world) * rot);
        }

        glm::vec3 TransformAccessor::GetWorldScale()
        {
            if (!IsValid())
                return glm::vec3(1.0f);

            glm::vec3 world = storage->GetScale(id);
            TransformID parent = storage->GetParent(id);

            for (int guard = 0; InRange(storage,parent) && guard < kMaxHierarchyWalk; ++guard)
            {
                world  = storage->GetScale(parent) * world;
                parent = storage->GetParent(parent);
            }

            return world;
        }

        void TransformAccessor::SetWorldScale(const glm::vec3 &scale)
        {
            if (!IsValid())
                return;

            const TransformID parent = storage->GetParent(id);

            if (!InRange(storage,parent))
            {
                storage->SetScale(id,scale);
                return;
            }

            const glm::vec3 parent_world = TransformAccessor(storage,parent,context).GetWorldScale();
            storage->SetScale(id,scale / parent_world);
        }

        // ── 移动性 / 脏标记 ──────────────────────────────────────────────────

        Mobility TransformAccessor::GetMobility() const
        {
            if (!IsValid())
                return Mobility::Static;

            return storage->GetMobility(id) ? Mobility::Movable : Mobility::Static;
        }

        void TransformAccessor::SetMobility(Mobility m)
        {
            if (IsValid())
                storage->SetMobility(id,(m == Mobility::Movable) ? 1 : 0);
        }

        bool TransformAccessor::IsDirty() const
        {
            return IsValid() ? storage->IsDirty(id) : false;
        }

        void TransformAccessor::MarkDirty()
        {
            if (IsValid())
                storage->SetDirty(id,true);
        }

        void TransformAccessor::UpdateIfDirty()
        {
            if (IsValid() && (storage->IsDirty(id) || storage->IsTopologyDirty()))
                storage->UpdateDirtyWorldMatricesFlat();
        }
    }//namespace ecs
}//namespace hgl
