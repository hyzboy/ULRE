#include<hgl/graph/render/RenderStages.h>
#include<hgl/vk/VKCommandBuffer.h>
#include<hgl/vk/IGPUBuffer.h>
#include<hgl/vk/VKRenderTarget.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/graph/camera/ViewportInfo.h>
#include<hgl/object/ObjectTracker.h>
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

                const auto &registry = device->GetGPUBufferRegistry();
                bool had_uploads = false;

                for(auto *gpu_buf : registry)
                {
                    if(gpu_buf && gpu_buf->IsDirty())
                    {
                        gpu_buf->CopyToDevice(ctx.cmd->operator VkCommandBuffer());
                        gpu_buf->ClearDirty();
                        had_uploads = true;
                    }
                }

                if(had_uploads)
                {
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

        // RenderStages only cover RT/FBO pass orchestration and can be replayed
        // for multi-pass workflows. ECS business systems run in ECSContext phases.

        static StageBeginFrame begin_frame;
        static StageFlushUpload flush_upload;
        static StageBeginRenderPass begin_pass;
        static StageEndRenderPass end_pass;

        pipeline.AddStage(&begin_frame);
        pipeline.AddStage(&flush_upload);
        pipeline.AddStage(&begin_pass);
        pipeline.AddStage(&end_pass);
    }
}//namespace hgl::graph

