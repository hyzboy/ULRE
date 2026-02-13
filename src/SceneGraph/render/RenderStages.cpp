#include<hgl/graph/RenderStages.h>
#include<hgl/ecs/Context.h>
#include<hgl/graph/RenderContext.h>
#include<hgl/graph/VKBufferUpdateQueue.h>
#include<hgl/graph/VKCommandBuffer.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/geo/line/LineRenderManager.h>

namespace hgl::graph
{
    namespace
    {
        class StageBeginFrame : public RenderStage
        {
        public:

            const char *GetName() const override { return "BeginFrame"; }

            void Execute(RenderStageContext &ctx) override
            {
                if(!ctx.render_target)
                    return;

                ctx.cmd = ctx.render_target->BeginRender();

                if(ctx.ecs_context)
                    ctx.ecs_context->SetFrameIndex(ctx.render_target->GetCurrentFrameIndex());
            }
        };

        class StageBindDescriptor : public RenderStage
        {
        public:

            const char *GetName() const override { return "BindDescriptor"; }

            void Execute(RenderStageContext &ctx) override
            {
                if(ctx.render_context && ctx.cmd)
                    ctx.render_context->BindDescriptor(ctx.cmd);
            }
        };

        class StageFlushUpload : public RenderStage
        {
        public:

            const char *GetName() const override { return "FlushUpload"; }

            void Execute(RenderStageContext &ctx) override
            {
                if(!ctx.render_target || !ctx.cmd)
                    return;

                VulkanDevice *device = ctx.render_target->GetDevice();
                if(!device)
                    return;

                auto *update_queue = device->GetBufferUpdateQueue();
                if(update_queue && update_queue->HasPendingUpdates())
                {
                    update_queue->FlushAll(ctx.cmd->operator VkCommandBuffer());

                    VkMemoryBarrier barrier{};
                    barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
                    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                    barrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_UNIFORM_READ_BIT;

                    vkCmdPipelineBarrier(ctx.cmd->operator VkCommandBuffer(),
                                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                                         VK_PIPELINE_STAGE_VERTEX_INPUT_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                         0, 1, &barrier, 0, nullptr, 0, nullptr);
                }
            }
        };

        class StageBeginRenderPass : public RenderStage
        {
        public:

            const char *GetName() const override { return "BeginRenderPass"; }

            void Execute(RenderStageContext &ctx) override
            {
                if(!ctx.cmd)
                    return;

                if(ctx.clear_color)
                    ctx.cmd->SetClearColor(0,*ctx.clear_color);

                ctx.cmd->BeginRenderPass();
            }
        };

        class StageEcsRender : public RenderStage
        {
        public:

            const char *GetName() const override { return "EcsRender"; }

            void Execute(RenderStageContext &ctx) override
            {
                if(ctx.ecs_context && ctx.cmd)
                    ctx.ecs_context->Render(ctx.cmd, 0.0f);
            }
        };

        class StageLineRender : public RenderStage
        {
        public:

            const char *GetName() const override { return "LineRender"; }

            void Execute(RenderStageContext &ctx) override
            {
                if(ctx.line_render_manager && ctx.cmd)
                {
                    ctx.line_render_manager->Draw(ctx.cmd);
                    ctx.render_result = true;
                }
            }
        };

        class StageEndRenderPass : public RenderStage
        {
        public:

            const char *GetName() const override { return "EndRenderPass"; }

            void Execute(RenderStageContext &ctx) override
            {
                if(!ctx.cmd || !ctx.render_target)
                    return;

                ctx.cmd->EndRenderPass();
                ctx.render_target->EndRender();
            }
        };
    }

    void BuildEcsPipeline(RenderStagePipeline &pipeline)
    {
        if(!pipeline.GetStages().empty())
            return;

        static StageBeginFrame begin_frame;
        static StageBindDescriptor bind_descriptor;
        static StageFlushUpload flush_upload;
        static StageBeginRenderPass begin_pass;
        static StageEcsRender ecs_render;
        static StageEndRenderPass end_pass;

        pipeline.AddStage(&begin_frame);
        pipeline.AddStage(&bind_descriptor);
        pipeline.AddStage(&flush_upload);
        pipeline.AddStage(&begin_pass);
        pipeline.AddStage(&ecs_render);
        pipeline.AddStage(&end_pass);
    }
}//namespace hgl::graph
