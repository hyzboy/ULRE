#include <hgl/graph/render/RenderContext.h>
#include <hgl/vk/VKRenderTarget.h>

namespace hgl::graph
{
    Pipeline* RenderContext::CreatePipeline(ShaderProgram* material,
                                            const PipelineData* pd)
    {
        if (!current_render_target)
            return nullptr;

        RenderPass* rp = current_render_target->GetRenderPass();
        return rp ? rp->CreatePipeline(material, pd) : nullptr;
    }


    void RenderContext::SetCurrentRenderTarget(IRenderTarget* rt)
    {
        current_render_target = rt;
    }

    IRenderTarget* RenderContext::GetCurrentRenderTarget() const
    {
        return current_render_target;
    }

    void RenderContext::SetCurrentRenderCmdBuffer(RenderCmdBuffer* cmd)
    {
        current_render_cmd_buf = cmd;
    }

    RenderCmdBuffer* RenderContext::GetCurrentRenderCmdBuffer() const
    {
        return current_render_cmd_buf;
    }

} // namespace hgl::graph
