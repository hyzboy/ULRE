#pragma once

#include<System.h>
#include<TransformComponent.h>
#include<BoundingBoxComponent.h>
#include<glm/glm.hpp>

namespace hgl::ecs
{
    /**
     * @brief 示例1：基础参与 - 所有有Transform和BoundingBox的实体都参与
     *
     * 架构流程：
     * 1. System启动：CreateQuery<TransformComponent, BoundingBoxComponent>()
     * 2. Entity添加组件：
     *    - Entity添加Transform → Context通知System → System查询TryAddEntity → 检查是否有BoundingBox
     *    - Entity添加BoundingBox → Context通知System → System查询TryAddEntity → 检查是否有Transform & 谓词
     *    如果都满足，直接加入缓存（O(1)）
     * 3. 后续Tick：System::Tick直接从缓存遍历，无需扫描全部Entity
     */
    class AABBUpdateSystem : public System
    {
    public:
        virtual bool Initialize()
        {
            // 创建查询：同时需要Transform和BoundingBox
            aabb_query = CreateQuery<TransformComponent, BoundingBoxComponent>();

            return aabb_query != nullptr;
        }

        virtual void Tick(ECSContext* context)
        {
            if (!aabb_query)
                return;

            // 直接从缓存遍历，无需每帧重新扫描所有Entity
            for (EntityID id : aabb_query->GetEntities())
            {
                auto entity = context->GetEntity(id);
                if (!entity)
                    continue;

                auto* transform = entity->GetComponent<TransformComponent>();
                auto* bbox = entity->GetComponent<BoundingBoxComponent>();

                if (transform && bbox)
                {
                    // 更新AABB位置
                    bbox->Update(transform->GetWorldMatrix());
                }
            }
        }

    private:
        EntityQuery* aabb_query = nullptr;
    };

    /**
     * @brief 示例2：条件参与 - 只有距离摄像机足够近的树才更新骨骼动画
     *
     * 场景：有1000棵树，但场景有10000个实体
     * 问题：全部树都有SkeletonComponent，但大部分树距离摄像机很远，不值得更新动画
     * 解决方案：使用WithPredicate()条件过滤
     *
     * 架构流程：
     * 1. System启动：
     *    query = CreateQuery<SkeletonComponent, TransformComponent>()
     *           ->WithPredicate([](Entity* e) { return Distance(e->transform, camera) < 100m; })
     * 2. Entity添加组件：
     *    - Entity添加SkeletonComponent → System检查：有Transform吗？ → 距离<100m吗？
     *    - 只有都满足才加入缓存（比如只有200棵树满足）
     * 3. 后续Tick：
     *    - 只遍历200棵在视距内的树，其他8000棵树完全不处理
     *    - 性能提升40倍（8000 → 200）
     * 4. 当玩家移动时：
     *    - 其他树靠近视距 → Entity添加组件时NotifyContext → System的Predicate检查通过 → 加入缓存
     *    - 树远离视距 → Predicate不通过 → 被从缓存移除
     */
    class LODSkeletonAnimationSystem : public System
    {
    private:
        Entity* camera_entity = nullptr;
        float lod_distance = 100.0f;

    public:
        explicit LODSkeletonAnimationSystem(float distance = 100.0f)
            : lod_distance(distance) {}

        virtual bool Initialize()
        {
            // 条件参与：创建查询并添加距离条件
            skeleton_query = CreateQuery<TransformComponent>();

            if (!skeleton_query)
                return false;

            // WithPredicate：只有距离<100m的实体才参与
            skeleton_query->WithPredicate([this](Entity* entity) {
                if (!camera_entity)
                    return false;

                auto* entity_transform = entity->GetComponent<TransformComponent>();
                auto* camera_transform = camera_entity->GetComponent<TransformComponent>();

                if (!entity_transform || !camera_transform)
                    return false;

                glm::vec3 entity_pos = entity_transform->GetWorldPosition();
                glm::vec3 camera_pos = camera_transform->GetWorldPosition();
                float distance = glm::distance(entity_pos, camera_pos);

                return distance < lod_distance;
            });

            return true;
        }

        void SetCameraEntity(Entity* camera) { camera_entity = camera; }

        virtual void Tick(ECSContext* context)
        {
            if (!skeleton_query)
                return;

            // 遍历的只是满足条件的实体（在视距内的树）
            // 不管有多少棵远处的树，这里永远只处理近处的
            for (EntityID id : skeleton_query->GetEntities())
            {
                auto entity = context->GetEntity(id);
                if (!entity)
                    continue;

                // 更新动画处理...
                // 这段代码只会对少数靠近摄像机的树执行
            }
        }

    private:
        EntityQuery* skeleton_query = nullptr;
    };

    /**
     * @brief 示例3：主动参与 - AI活跃度管理
     *
     * 场景：有500个NPC，但只有30个在玩家附近活跃
     * 问题：
     *   - 复杂的AI计算（寻路、行为树等）不能简单用距离判断
     *   - 需要游戏逻辑动态决定谁参与AI更新
     * 解决方案：System手动管理参与列表，由AI Manager根据业务逻辑控制
     *
     * 架构流程：
     * 1. System启动：CreateQuery<AIComponent>() 初始为空
     * 2. NPC靠近玩家时：
     *    - AI Manager检查：这个NPC应该被激活吗？
     *    - 调用 aiSystem->AddEntityManually(ai_query, npc_id)
     *    - NPC被加入AI更新队列，开始复杂计算
     * 3. NPC远离玩家时：
     *    - AI Manager检查：这个NPC应该休眠吗？
     *    - 调用 aiSystem->RemoveEntityManually(ai_query, npc_id)
     *    - NPC被移出更新队列，只做最小化逻辑
     * 4. Tick中只处理活跃的NPC，性能稳定（O(n) n=活跃NPC数）
     */
    class AISystem : public System
    {
    private:
        struct AIEntity
        {
            EntityID id;
            float last_decision_time = 0.0f;
            // ... AI状态
        };

        std::vector<AIEntity> active_ai_entities;

    public:
        virtual bool Initialize()
        {
            // 创建查询：被手动管理的AI实体
            active_query = CreateQuery<TransformComponent>();

            return active_query != nullptr;
        }

        /// 游戏逻辑调用：激活某个NPC的AI
        void ActivateNPC(ECSContext* context, EntityID npc_id)
        {
            auto entity = context->GetEntity(npc_id);
            if (!entity)
                return;

            // 手动添加到活跃队列
            AddEntityManually(active_query, npc_id);

            active_ai_entities.push_back({npc_id, 0.0f});
        }

        /// 游戏逻辑调用：休眠某个NPC的AI
        void DeactivateNPC(EntityID npc_id)
        {
            // 手动移除
            RemoveEntityManually(active_query, npc_id);

            auto it = std::find_if(active_ai_entities.begin(), active_ai_entities.end(),
                [npc_id](const AIEntity& ai) { return ai.id == npc_id; });
            if (it != active_ai_entities.end())
            {
                active_ai_entities.erase(it);
            }
        }

        virtual void Tick(ECSContext* context)
        {
            if (!active_query)
                return;

            // 只处理活跃的AI（被手动加入的）
            // 即使有500个NPC，如果只有30个活跃，这里只循环30次
            for (auto& ai : active_ai_entities)
            {
                auto entity = context->GetEntity(ai.id);
                if (!entity)
                    continue;

                // AI决策逻辑...
                // 这个昂贵的计算只在必要的NPC上执行
            }
        }

        size_t GetActiveNPCCount() const { return active_ai_entities.size(); }
        size_t GetTotalNPCCount(ECSContext* context) const
        {
            if (!active_query)
                return 0;
            return active_query->GetEntityCount();
        }

    private:
        EntityQuery* active_query = nullptr;
    };

    /**
     * @brief 示例4：混合参与模式
     *
     * 实际应用中常常需要混合使用上述三种模式
     *
     * 场景：渲染系统中
     * - 所有Renderable必须进渲染队列（基础参与）
     * - 但只有在视锥体内的才需要提交批绘制（条件参与）
     * - 特殊物体可以由逻辑控制是否强制渲染（手动参与）
     */
}
