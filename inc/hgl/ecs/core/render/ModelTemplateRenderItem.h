#pragma once

#include <hgl/ecs/core/render/PrimitiveRenderItem.h>
#include <hgl/mtl/PipelineConfig.h>

namespace hgl::graph
{
    class Pipeline;
    class RenderPass;
}

namespace hgl::ecs
{
    /**
     * ModelTemplateRenderItem
     *
     * 专门用于承载多材质 GPU 模型模板中特定 Shading Group 的轻量渲染载体项。
     * 其几何体和材质常驻在 GPU SSBO 池中（CPU 无实体级 GeometryData/MaterialData），
     * 本项持有模板 ID、组索引以及该组针对当前 pass 解析好的 Pipeline 与 PipelineConfig。
     */
    class ModelTemplateRenderItem : public PrimitiveRenderItem
    {
    private:
        uint32_t shading_group = 0;
        uint32_t template_id = 0;
        graph::Pipeline *pipeline = nullptr;
        graph::mtl::MaterialPipelineConfig pipeline_config{};
        graph::RenderPass *cached_pass = nullptr;

    public:
        ModelTemplateRenderItem(
            EntityID ent_id,
            const TransformAccessor &trans,
            MaterialRuntimeRowID mat_row,
            uint32_t group_idx,
            uint32_t t_id,
            graph::RenderPass *pass,
            graph::Pipeline *p,
            const graph::mtl::MaterialPipelineConfig &cfg,
            ECSContext *ctx)
            : PrimitiveRenderItem(ent_id, trans, mat_row, ctx)
            , shading_group(group_idx)
            , template_id(t_id)
            , pipeline(p)
            , pipeline_config(cfg)
            , cached_pass(pass)
        {
        }

        ~ModelTemplateRenderItem() override = default;

        uint32_t GetShadingGroup() const { return shading_group; }
        uint32_t GetTemplateID() const { return template_id; }

        graph::Pipeline *GetPipeline(graph::RenderPass *render_pass) const override
        {
            if (pipeline && (!render_pass || render_pass == cached_pass))
                return pipeline;

            return PrimitiveRenderItem::GetPipeline(render_pass);
        }

        graph::mtl::MaterialPipelineConfig GetPipelineConfig(
            graph::RenderPass *render_pass) const override
        {
            if (pipeline && (!render_pass || render_pass == cached_pass))
                return pipeline_config;

            return PrimitiveRenderItem::GetPipelineConfig(render_pass);
        }
    };
}
