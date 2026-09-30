#pragma once

#include<hgl/ecs/support/BoundingBoxDataStorage.h>
#include<hgl/ecs/core/EntityHandle.h>
#include<glm/glm.hpp>

namespace hgl
{
    namespace ecs
    {
        class ECSContext;

        /**
         * 包围盒的**薄句柄**（值类型，24 字节，可随意拷贝）。
         *
         * 它只记「哪一行 + 哪个世界」，自身**不持有任何包围盒副本**：
         * 所有读写直落 `BoundingBoxDataStorage`（local_bounds 唯一的真源），
         * world_bounds 是派生缓存（本地 AABB × 世界矩阵）。
         *
         * 这是原组件改造为 SoA 存储后的调用面（与 TransformAccessor 同范式）：
         * 调用点从「拿组件 shared_ptr」改成「拿 accessor 值」，语义不变。
         */
        class BoundingBoxAccessor
        {
            BoundingBoxDataStorage           *storage    = nullptr;
            BoundingBoxDataStorage::HandleID  id         = BoundingBoxDataStorage::INVALID_HANDLE;
            uint32_t                          generation = 0;         ///< 建柄时捕获的行世代（revision）：释放后旧柄自动失效
            ECSContext                       *context    = nullptr;   ///< 用于解析 owner 实体（可为空：纯数据操作不需要）

        public:

            BoundingBoxAccessor() = default;

            /// 建柄时**捕获行世代**：这是"句柄失效"的全部机制（与 TransformAccessor 一致）。
            /// 世代编码：**0 = 死/未分配；正奇数 = 活**。释放（Deallocate）会把它归 0
            /// ⇒ 旧柄与"死行上的新柄"都无效；将来行复用时 +2 保持奇数 ⇒ ABA 免疫。
            BoundingBoxAccessor(BoundingBoxDataStorage *s,
                                BoundingBoxDataStorage::HandleID i,
                                ECSContext *c = nullptr)
                : storage(s),id(i),generation(s ? s->GetGeneration(i) : 0),context(c) {}

            bool IsValid() const
            {
                return storage
                    && id != BoundingBoxDataStorage::INVALID_HANDLE
                    && generation != 0                                  // 0 = 死/未分配：死行上的新柄也无效
                    && generation == storage->GetGeneration(id);        // 世代不符 = 该行已换过主人
            }

            BoundingBoxDataStorage::HandleID  GetID() const      { return id; }
            BoundingBoxDataStorage           *GetStorage() const { return storage; }
            ECSContext                       *GetContext() const { return context; }

        public: // 归属（真源在存储的 owners 行）

            /// 所属实体（无效柄返回无效 EntityID）
            EntityID GetOwner() const
            {
                if (!IsValid())
                    return EntityID();

                return storage->GetOwner(id);
            }

        public: // 本地包围盒（读写直落存储）

            math::AABB GetLocalBounds() const
            {
                if (!IsValid())
                    return math::AABB();

                return storage->GetLocalBounds(id);
            }

            void SetLocalBounds(const math::AABB &bounds) const
            {
                if (!IsValid())
                    return;

                storage->SetLocalBounds(id,bounds);
            }

            /// 中心 = (min + max) * 0.5（派生量，存储侧不单独缓存）
            glm::vec3 GetCenter() const
            {
                if (!IsValid())
                    return glm::vec3(0.0f);

                return storage->GetCenter(id);
            }

            /// 半长 = (max - min) * 0.5（派生量，存储侧不单独缓存）
            glm::vec3 GetExtents() const
            {
                if (!IsValid())
                    return glm::vec3(0.0f);

                return storage->GetExtents(id);
            }

        public: // 世界包围盒（派生缓存）

            bool HasWorldBounds() const
            {
                return IsValid() && storage->HasWorldBounds(id);
            }

            math::AABB GetWorldBounds() const
            {
                if (!IsValid())
                    return math::AABB();

                return storage->GetWorldBounds(id);
            }

            void SetWorldBounds(const math::AABB &bounds) const
            {
                if (!IsValid())
                    return;

                storage->SetWorldBounds(id,bounds);
            }
        };

        // 值类型句柄：24 B（8 + 4 + 4 + 8），可随意拷贝、无堆开销
        static_assert(sizeof(BoundingBoxAccessor) == 24,
                      "BoundingBoxAccessor 必须是 24 字节的值类型句柄（storage/id/generation/context）");
    }//namespace ecs
}//namespace hgl
