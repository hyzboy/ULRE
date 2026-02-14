#pragma once

#include<System.h>
#include<TransformComponent.h>
#include<BoundingBoxComponent.h>

namespace hgl::ecs
{
    /**
     * @brief 示例系统：演示如何使用EntityQuery缓存机制
     *
     * 该系统演示：
     * 1. CreateQuery<>() 创建一个缓存的实体查询
     * 2. 每次需要遍历实体时，直接从缓存获取已匹配的实体
     * 3. 当实体添加/移除组件时，缓存自动标记为脏，下次重建时只扫描必要的实体
     *
     * 所有需要Transform和BoundingBox的实体会被自动跟踪，无需每帧重新扫描所有实体
     */
    class QueryCacheExampleSystem : public System
    {
    public:
        QueryCacheExampleSystem() = default;
        virtual ~QueryCacheExampleSystem() = default;

        virtual bool Initialize()
        {
            // 创建一个查询：查找同时拥有Transform和BoundingBox的实体
            // 第一次调用时会立即扫描所有实体
            // 之后每次添加/移除组件时缓存会自动标记为脏
            query = CreateQuery<TransformComponent, BoundingBoxComponent>();

            if (!query)
                return false;

            return true;
        }

        virtual void Tick(ECSContext* context)
        {
            if (!query)
                return;

            // 重建缓存（如果脏标记被设置）
            // 如果缓存未脏，此操作不执行任何操作
            query->Rebuild(context);

            // 直接从缓存获取所有匹配的实体，无需遍历全部
            const auto& entities = query->GetEntities();

            for (EntityID id : entities)
            {
                auto entity = context->GetEntity(id);
                if (!entity)
                    continue;

                // 处理这个实体...
                auto* transform = entity->GetComponent<TransformComponent>();
                auto* bbox = entity->GetComponent<BoundingBoxComponent>();

                if (transform && bbox)
                {
                    // 更新边界框位置基于变换
                    bbox->Update(transform->GetWorldMatrix());
                }
            }
        }

        // 获取缓存的实体数量（用于性能监测）
        size_t GetCachedEntityCount() const
        {
            return query ? query->GetEntities().size() : 0;
        }

    private:
        EntityQuery* query = nullptr;  // 缓存查询对象
    };

    /**
     * @brief 高级示例：多种组件查询
     *
     * 演示单个系统管理多个不同的查询，每个查询缓存不同的组件组合
     */
    class MultiQueryCacheSystem : public System
    {
    public:
        virtual bool Initialize()
        {
            // 创建多个查询，每个查询缓存不同的组件类型组合
            transform_query = CreateQuery<TransformComponent>();
            bbox_query = CreateQuery<BoundingBoxComponent>();
            both_query = CreateQuery<TransformComponent, BoundingBoxComponent>();

            return transform_query && bbox_query && both_query;
        }

        virtual void Tick(ECSContext* context)
        {
            if (!transform_query || !bbox_query || !both_query)
                return;

            // 三个缓存独立管理，相互不影响
            transform_query->Rebuild(context);
            bbox_query->Rebuild(context);
            both_query->Rebuild(context);

            // 使用第一个查询：所有有Transform的
            size_t transform_count = transform_query->GetEntities().size();

            // 使用第二个查询：所有有BoundingBox的
            size_t bbox_count = bbox_query->GetEntities().size();

            // 使用第三个查询：同时有两个组件的（最优化）
            size_t both_count = both_query->GetEntities().size();
        }

    private:
        EntityQuery* transform_query = nullptr;
        EntityQuery* bbox_query = nullptr;
        EntityQuery* both_query = nullptr;
    };

    /**
     * @brief 缓存系统性能优势说明
     *
     * 传统方法（无缓存）：
     * - 每帧都遍历所有实体 O(n)
     * - 对于有1000个实体、只有100个有效的情况，浪费900次检查
     *
     * 使用EntityQuery缓存：
     * - 初始：O(n) 扫描所有实体建立缓存
     * - 后续帧：O(1) 直接使用缓存列表，无需扫描
     * - 添加组件：O(1) 标记脏，下次调用Rebuild时O(m) m为脏实体数
     * - 删除组件：O(1) 标记脏，下次调用Rebuild时O(m) m为脏实体数
     *
     * 对于稳定的实体集合，性能提升为 100-1000倍
     */
}
