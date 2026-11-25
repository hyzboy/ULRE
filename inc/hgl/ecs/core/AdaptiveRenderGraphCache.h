#pragma once

#include<hgl/ecs/core/render/RenderGraph.h>

namespace hgl::ecs
{
    /**
     * 自适应渲染图缓存（世界私有）。
     *
     * 三件事绑在一起，因为它们只在 `Render` 的一处协作：
     * 场景结构变脏 ⇒ 重新 gather 场景统计并算哈希 ⇒ 哈希变了才重建渲染图。
     * `~0ULL` 是"从未计算"的哨兵（首个哈希不可能是它）。
     */
    class AdaptiveRenderGraphCache
    {
    private:
        RenderGraph graph;
        uint64_t scene_hash = ~0ULL;
        bool dirty = true;      ///< 场景结构（entity/component 增删）变化：下次 Render 重 gather

    public:
        void MarkDirty() { dirty = true; }
        bool IsDirty() const { return dirty; }
        void MarkClean() { dirty = false; }

        /// 当前缓存的场景哈希是否需要重建渲染图
        bool NeedsRebuild(uint64_t hash) const { return hash != scene_hash; }

        void Store(uint64_t hash, RenderGraph &&value)
        {
            scene_hash = hash;
            graph = std::move(value);
        }

        RenderGraph &Graph() { return graph; }
        const RenderGraph &Graph() const { return graph; }
    };
}