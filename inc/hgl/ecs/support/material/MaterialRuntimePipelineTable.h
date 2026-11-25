#pragma once

#include<hgl/CoreType.h>
#include<hgl/ecs/core/entity/EntityHandle.h>
#include<hgl/mtl/PipelineConfig.h>
#include<hgl/type/UnorderedMap.h>

namespace hgl::graph
{
    class Pipeline;
    class RenderPass;
    class ShaderProgram;
}//namespace hgl::graph

namespace hgl::ecs
{
    /**
     * 每实例 × 每 Shading Group 的当前 pass 管线缓存。
     *
     * 为什么存在：collect 阶段按当前 pass 解析出管线，紧随其后的 batch 消费同一 pass 的结果。
     * 这份缓存原先是 `MaterialRuntimeSlot` 上的两个指针（`runtime_pipeline_pass` / `runtime_pipeline`）——
     * 按「**每 pass 解析产物不入行，落世界侧旁表**（键 = 该实例）」的既定规则移到这里，
     * slot 才能变成全 POD、行宽才能冻结（B5 标题里的「行宽定稿」）。
     *
     * 每个 pass 独立保存各组，阴影收集不能覆盖主帧重放所需的管线。
     * 同一实体的不同组互不覆盖，Clear 一次摘除该实体的全部 pass 与组。
     * program 仅为本 pass 的借用产物，不作为持久身份或缓存键。
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
            graph::ShaderProgram *program = nullptr;

            /// EDS 1/2/3：管线对象**不保存**渲染状态（见 `inc/hgl/vk/pipeline/VKPipeline.h`），
            /// 解析时得到的 `MaterialPipelineConfig` 必须与管线一起存下来 —— 它就是
            /// 「本实例在本 pass 下的动态渲染状态」的唯一来源，绘制期由批次带给
            /// `RenderCmdBuffer::ApplyPipelineState`。
            graph::mtl::MaterialPipelineConfig config;
        };

        /// 登记本实例的指定组在 `pass` 下解析出的管线与配套渲染状态
        void Set(EntityID id, graph::RenderPass *pass, graph::Pipeline *pipeline,
                 const graph::mtl::MaterialPipelineConfig &config, uint32_t shading_group = 0,
                 graph::ShaderProgram *program = nullptr)
        {
            entries[id][pass][shading_group] = Entry{ pass, pipeline, program, config };
        }

        graph::ShaderProgram *GetProgram(EntityID id, graph::RenderPass *pass,
                                          uint32_t shading_group = 0) const
        {
            const Entry *entry = FindEntry(id, pass, shading_group);
            return entry ? entry->program : nullptr;
        }

        /// 取本实例在 `pass` 下的管线；未登记 / pass 不匹配 ⇒ nullptr
        graph::Pipeline *Get(EntityID id, graph::RenderPass *pass, uint32_t shading_group = 0) const
        {
            const Entry *entry = FindEntry(id, pass, shading_group);

            return entry ? entry->pipeline : nullptr;
        }

        /// 取本实例在 `pass` 下的动态渲染状态；未登记 / pass 不匹配 ⇒ 默认配置
        ///（此时 `Get` 也必然返回 nullptr，该图元根本不会进批）。
        graph::mtl::MaterialPipelineConfig GetConfig(EntityID id, graph::RenderPass *pass,
                                                     uint32_t shading_group = 0) const
        {
            const Entry *entry = FindEntry(id, pass, shading_group);

            return entry ? entry->config : graph::mtl::MaterialPipelineConfig{};
        }

        /// 摘除（实体销毁 / 管线失效时调用；未登记则忽略）
        void Clear(EntityID id) { entries.DeleteByKey(id); }

    private:

        UnorderedMap<EntityID, UnorderedMap<graph::RenderPass *, UnorderedMap<uint32_t, Entry>>> entries;

        const Entry *FindEntry(EntityID id, graph::RenderPass *pass, uint32_t shading_group) const
        {
            const auto *passes = entries.GetValuePointer(id);
            const auto *groups = passes ? passes->GetValuePointer(pass) : nullptr;
            const Entry *entry = groups ? groups->GetValuePointer(shading_group) : nullptr;
            return entry && entry->pass == pass ? entry : nullptr;
        }
    };
}//namespace hgl::ecs
