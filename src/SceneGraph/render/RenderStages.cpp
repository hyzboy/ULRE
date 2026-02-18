#include<hgl/graph/render/RenderStages.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/systems/tick/CameraSystem.h>
#include<hgl/ecs/systems/render/EnvironmentSystem.h>
#include<hgl/ecs/systems/render/LineRenderSystem.h>
#include<hgl/vk/VKBufferUpdateQueue.h>
#include<hgl/vk/VKCommandBuffer.h>
#include<hgl/vk/VKRenderTarget.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/graph/camera/ViewportInfo.h>
#include<hgl/graph/geo/line/LineRenderService.h>
#include<hgl/utils/ObjectTracker.h>
#include<hgl/log/Log.h>

namespace hgl::graph
{
    namespace
    {
        class StageScope
        {
        private:
            const char *name = nullptr;

        public:
            explicit StageScope(const char *stage_name)
                : name(stage_name)
            {
                HGL_CAPTURE_SCOPE();
                GLogDebug("[RenderStage] Begin: %s", name ? name : "(null)");
            }

            ~StageScope()
            {
                GLogDebug("[RenderStage] End: %s", name ? name : "(null)");
            }
        };

        class StageEcsPreBeginFrame : public RenderStage
        {
        public:

            const char *GetName() const override { return "EcsPreBeginFrame"; }

            void Execute(RenderStageContext &ctx) override
            {
                StageScope scope(GetName());
                if (ctx.ecs_context)
                    ctx.ecs_context->RenderPreBeginFrame(0.0f);
            }
        };


        class StageBeginFrame : public RenderStage
        {
        public:

            const char *GetName() const override { return "BeginFrame"; }

            void Execute(RenderStageContext &ctx) override
            {
                StageScope scope(GetName());
                if(!ctx.render_target)
                    return;

                const VkExtent2D &ext = ctx.render_target->GetExtent();
                const auto *vp_info = ctx.render_target->GetViewportInfo();
                if (!vp_info || vp_info->GetViewport().x != ext.width || vp_info->GetViewport().y != ext.height)
                {
                    ctx.render_target->OnResize(ext);
                }

                ctx.cmd = ctx.render_target->BeginRender();

                if(ctx.ecs_context)
                {
                    ctx.ecs_context->SetFrameIndex(ctx.render_target->GetCurrentFrameIndex());
                    ctx.ecs_context->RenderBeginFrame(0.0f);

                    auto camera_system = ctx.ecs_context->GetSystem<ecs::CameraSystem>();
                    if (camera_system)
                        camera_system->SyncCameraUBO();

                    auto environment_system = ctx.ecs_context->GetSystem<ecs::EnvironmentSystem>();
                    if (environment_system)
                        environment_system->SyncSkyUBO();
                }
            }
        };

        class StageBindDescriptor : public RenderStage
        {
        public:

            const char *GetName() const override { return "BindDescriptor"; }

            void Execute(RenderStageContext &ctx) override
            {
                StageScope scope(GetName());
                if(!ctx.cmd)
                    return;

                if(ctx.ecs_context)
                {
                    auto camera_system = ctx.ecs_context->GetSystem<ecs::CameraSystem>();
                    if(camera_system)
                        camera_system->BindDescriptor(ctx.cmd);
                }

            }
        };

        class StageFlushUpload : public RenderStage
        {
        public:

            const char *GetName() const override { return "FlushUpload"; }

            void Execute(RenderStageContext &ctx) override
            {
                StageScope scope(GetName());
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
                StageScope scope(GetName());
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
                StageScope scope(GetName());
                if(ctx.ecs_context && ctx.cmd)
                    ctx.ecs_context->Render(ctx.cmd, 0.0f);
            }
        };

        class StageEcsPostBeginFrame : public RenderStage
        {
        public:

            const char *GetName() const override { return "EcsPostBeginFrame"; }

            void Execute(RenderStageContext &ctx) override
            {
                StageScope scope(GetName());
                if (ctx.ecs_context)
                    ctx.ecs_context->RenderPostBeginFrame(0.0f);
            }
        };

        class StageLineRender : public RenderStage
        {
        public:

            const char *GetName() const override { return "LineRender"; }

            void Execute(RenderStageContext &ctx) override
            {
                StageScope scope(GetName());
                if (!ctx.line_render_service && ctx.ecs_context)
                {
                    auto line_system = ctx.ecs_context->GetSystem<ecs::LineRenderSystem>();
                    if (line_system)
                        ctx.line_render_service = line_system->GetLineRenderService();
                }

                if(ctx.line_render_service && ctx.cmd)
                {
                    ctx.line_render_service->Draw(ctx.cmd);
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
                StageScope scope(GetName());
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

        // RenderStages only cover a single RT/FBO pass and can be replayed
        // for multi-pass or deferred rendering workflows. Frame submit is handled elsewhere.

        static StageEcsPreBeginFrame ecs_pre_begin_frame;
        static StageBeginFrame begin_frame;
        static StageEcsPostBeginFrame ecs_post_begin_frame;
        static StageBindDescriptor bind_descriptor;
        static StageFlushUpload flush_upload;
        static StageBeginRenderPass begin_pass;
        static StageEcsRender ecs_render;
        static StageEndRenderPass end_pass;
        pipeline.AddStage(&ecs_pre_begin_frame);
        pipeline.AddStage(&begin_frame);
        pipeline.AddStage(&ecs_post_begin_frame);
        pipeline.AddStage(&bind_descriptor);
        pipeline.AddStage(&flush_upload);
        pipeline.AddStage(&begin_pass);
        pipeline.AddStage(&ecs_render);
        pipeline.AddStage(&end_pass);
    }
}//namespace hgl::graph

