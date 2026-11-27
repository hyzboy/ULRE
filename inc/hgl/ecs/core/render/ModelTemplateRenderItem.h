#pragma once

#include <hgl/ecs/core/render/PrimitiveRenderItem.h>
#include <hgl/ecs/core/Context.h>
#include <hgl/ecs/support/material/MaterialRuntimePipelineTable.h>
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
     * 承载 GPU 模型模板中特定 Shading Group，包括归一化后的单图元模板。
     * 部件几何与材质由模板池提供，实体可以保留 GeometryData 用于资产和包围盒查询。
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

        graph::ShaderProgram *GetShaderProgram() const override
        {
            auto *program = context ? context->RenderData().GetMaterialRuntimePipelineTable().GetProgram(
                entity_id, cached_pass, shading_group) : nullptr;
            return program ? program : PrimitiveRenderItem::GetShaderProgram();
        }

        graph::Pipeline *GetPipeline(graph::RenderPass *render_pass) const override
        {
            if (pipeline && (!render_pass || render_pass == cached_pass))
                return pipeline;

            return context
                ? context->RenderData().GetMaterialRuntimePipelineTable().Get(entity_id, render_pass, shading_group)
                : nullptr;
        }

        graph::mtl::MaterialPipelineConfig GetPipelineConfig(
            graph::RenderPass *render_pass) const override
        {
            if (pipeline && (!render_pass || render_pass == cached_pass))
                return pipeline_config;

            return context
                ? context->RenderData().GetMaterialRuntimePipelineTable().GetConfig(entity_id, render_pass, shading_group)
                : graph::mtl::MaterialPipelineConfig{};
        }
    };
}
