#pragma once

#include<hgl/CoreType.h>
#include<hgl/ecs/core/EntityHandle.h>
#include<hgl/type/UnorderedMap.h>
#include<cstdint>
#include<unordered_map>

namespace hgl::graph
{
    class Pipeline;
    class RenderPass;
}//namespace hgl::graph

namespace hgl::ecs
{
    /**
     * 每实例 × 每 pass 的**运行期管线缓存**（B5-b）。
     *
     * 为什么存在：collect 阶段按当前 pass 解析出管线，紧随其后的 batch 消费同一 pass 的结果。
     * 这份缓存原先是 `MaterialRuntimeSlot` 上的两个指针（`runtime_pipeline_pass` / `runtime_pipeline`）——
     * 按「**每 pass 解析产物不入行，落世界侧旁表**（键 = 该实例）」的既定规则移到这里，
     * slot 才能变成全 POD、行宽才能冻结（B5 标题里的「行宽定稿」）。
     *
     * 语义与旧字段**逐字一致**：读时比对 pass，不匹配 ⇒ 未命中（返回 nullptr）——
     * 「管线按 RenderPass 维度键控、不同 RT 格式各自匹配」（A5b/D2）。
     * 不存 program 指针身份（D2：地址复用会误判）。
     *
     * 归属：**世界级**（`ECSContext` 持有）。清理：`InvalidateEntityRuntimePipeline` /
     * `DestroySlot` / 实体销毁路径。
     */
    class MaterialRuntimePipelineTable
    {
    public:

        struct Entry
        {
            graph::RenderPass *pass     = nullptr;
            graph::Pipeline   *pipeline = nullptr;
        };

        /// 登记本实例在 `pass` 下解析出的管线（覆盖旧值）
        void Set(EntityID id, graph::RenderPass *pass, graph::Pipeline *pipeline)
        {
            entries[id] = Entry{ pass, pipeline };
        }

        /// 取本实例在 `pass` 下的管线；未登记 / pass 不匹配 ⇒ nullptr
        graph::Pipeline *Get(EntityID id, graph::RenderPass *pass) const
        {
            const auto it = entries.find(id);

            if (it == entries.end() || it->second.pass != pass)
                return nullptr;

            return it->second.pipeline;
        }

        /// 摘除（实体销毁 / 管线失效时调用；未登记则忽略）
        void Clear(EntityID id) { entries.erase(id); }

    private:

        std::unordered_map<EntityID, Entry> entries;
    };
}//namespace hgl::ecs
