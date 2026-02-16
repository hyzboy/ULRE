#include <hgl/graph/render/RenderContext.h>
#include <hgl/vk/VKRenderTarget.h>

namespace hgl::graph
{
    RenderContext::RenderContext(VulkanDevice* dev,
                                 TextureManager* tex_mgr,
                                 BufferManager* buf_mgr,
                                 MaterialManager* mat_mgr,
                                 SamplerManager* samp_mgr,
                                 RenderPassManager* rp_mgr,
                                 GeometryManager* geo_mgr,
                                 PrimitiveManager* prim_mgr)
        : device(dev)
        , texture_manager(tex_mgr)
        , buffer_manager(buf_mgr)
        , material_manager(mat_mgr)
        , sampler_manager(samp_mgr)
        , render_pass_manager(rp_mgr)
        , geometry_manager(geo_mgr)
        , primitive_manager(prim_mgr)
    {
    }


    Pipeline* RenderContext::CreatePipeline(Material* material,
                                            const VertexInputLayout* vil,
                                            const PipelineData* pd,
                                            bool prim_restart)
    {
        if (!current_render_target)
            return nullptr;

        RenderPass* rp = current_render_target->GetRenderPass();
        return rp ? rp->CreatePipeline(material, vil, pd, prim_restart) : nullptr;
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
