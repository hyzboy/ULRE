#include<hgl/vk/VKCommandBuffer.h>
#include<hgl/vk/VKRenderPass.h>
#include<hgl/vk/VKFramebuffer.h>
#include<hgl/graph/mesh/Primitive.h>
#include<hgl/vk/VKDeviceAttribute.h>
#include<hgl/vk/VKPhysicalDevice.h>
#include<hgl/vk/VKIndexBuffer.h>
#include<hgl/vk/VKRenderTarget.h>

namespace hgl::graph{
RenderCmdBuffer::RenderCmdBuffer(const VulkanDevAttr *attr,VkCommandBuffer cb):VulkanCmdBuffer(attr,cb)
{
    cv_count=0;
    clear_values=nullptr;

    mem_zero(render_area);
    mem_zero(viewport);

    pipeline_layout=VK_NULL_HANDLE;
}

RenderCmdBuffer::~RenderCmdBuffer()
{
    if(clear_values)
        hgl_free(clear_values);
}

void RenderCmdBuffer::SetClear()
{
    if(cv_count>0)
    {
        clear_values=hgl_align_realloc<VkClearValue>(clear_values,cv_count);

        clear_values[cv_count-1].depthStencil.depth = 0.0f;
        clear_values[cv_count-1].depthStencil.stencil = 0;
    }
    else if(clear_values)
    {
        hgl_free(clear_values);
        clear_values=nullptr;
    }
}

void RenderCmdBuffer::SetRenderArea(const VkExtent2D &ext2d)
{
    render_area.offset.x=0;
    render_area.offset.y=0;
    render_area.extent=ext2d;
}

bool RenderCmdBuffer::BindFramebuffer(Framebuffer *fbo)
{
    if(!fbo)return(false);

    cv_count=fbo->GetAttachmentCount();
    SetClear();

    render_area.offset.x=0;
    render_area.offset.y=0;
    render_area.extent=fbo->GetExtent();

    rp_begin.renderPass         = *fbo->GetRenderPass();
    rp_begin.framebuffer        = *fbo;
    rp_begin.renderArea         = render_area;
    rp_begin.clearValueCount    = cv_count;
    rp_begin.pClearValues       = clear_values;

    viewport.x          = 0;
    viewport.y          = 0;
    viewport.minDepth   = 0.0f;
    viewport.maxDepth   = 1.0f;
    viewport.width      = render_area.extent.width;
    viewport.height     = render_area.extent.height;

    return(true);
};

bool RenderCmdBuffer::BeginRenderPass()
{
    vkCmdBeginRenderPass(cmd_buf, &rp_begin, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdSetViewport(cmd_buf,0,1,&viewport);
    vkCmdSetScissor(cmd_buf,0,1,&render_area);

    pipeline_layout=VK_NULL_HANDLE;

    return(true);
}

bool RenderCmdBuffer::BindDescriptorSets(ShaderProgram *mtl)
{
    if(!mtl)return(false);

    pipeline_layout=mtl->GetPipelineLayout();

    ENUM_CLASS_FOR(DescriptorSetType,int,i)
    {
        MaterialParameters *mp=mtl->GetMP((DescriptorSetType)i);
        if(!mp)
            continue;

        mp->Update();

        const VkDescriptorSet ds=mp->GetVkDescriptorSet();
        vkCmdBindDescriptorSets(cmd_buf,VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline_layout,uint32_t(i),1,&ds,0,nullptr);
    }

    return(true);
}
void RenderCmdBuffer::DrawIndirect( VkBuffer        buffer,
                                    VkDeviceSize    offset,
                                    uint32_t        drawCount,
                                    uint32_t        stride)
{
    if(this->dev_attr->physical_device->SupportMDI())
        vkCmdDrawIndirect(cmd_buf,buffer,offset,drawCount,stride);
    else
    for(uint32_t i=0;i<drawCount;i++)
        vkCmdDrawIndirect(cmd_buf,buffer,offset+i*stride,1,stride);
}

void RenderCmdBuffer::Draw(const GeometryDataBuffer *geom_data_buffer,const GeometryDrawRange *geom_draw_range,const uint32_t instance_count,const uint32_t first_instance)
{
    if(!geom_data_buffer||!geom_draw_range)
    {
        LogError("Null parameter in Draw");
        return;
    }

    // SSBO 顶点输入：统一非索引绘制（vkCmdDraw）——gl_VertexIndex 语义：
    //   有 IBO 的几何：每个索引一次 vs（查表 sbo_index[gl_VertexIndex]→索引值）——vertexCount = index_count
    //   无 IBO 的几何：顶点直通（gl_VertexIndex = 顶点序号）——vertexCount = vertex_count
    const uint32_t vertex_count = (geom_draw_range->index_count > 0)
        ? geom_draw_range->index_count
        : geom_draw_range->vertex_count;

    vkCmdDraw(cmd_buf,
              vertex_count,
              instance_count,
              0,   // SSBO 顶点输入：段偏移走 push constant（vertex_base）——vkCmdDraw 的 firstVertex=0
              first_instance);
}
}//namespace hgl::graph


