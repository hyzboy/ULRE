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

void RenderCmdBuffer::SetRenderArea(const VkExtent2D &ext2d)
{
    render_area.offset.x=0;
    render_area.offset.y=0;
    render_area.extent=ext2d;
}

bool RenderCmdBuffer::BeginRendering(IRenderTarget *rt)
{
    if(!rt)return(false);

    const uint32_t color_count=rt->GetColorCount();
    const uint32_t has_depth=rt->hasDepth()?1:0;

    // Dynamic Rendering：无 render pass 的自动布局转换——必须先手动把附件
    // 从 UNDEFINED 转换到 attachment 布局（VUID-vkCmdBeginRendering-pRenderingInfo-09592/09588）
    VkImageMemoryBarrier barriers[8]{};

    for(uint32_t i=0;i<color_count;i++)
    {
        Texture2D *tex=rt->GetColorTexture(i);
        if(!tex)continue;

        barriers[i].sType               =VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barriers[i].srcAccessMask       =0;
        barriers[i].dstAccessMask       =VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barriers[i].oldLayout           =VK_IMAGE_LAYOUT_UNDEFINED;
        barriers[i].newLayout           =VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        barriers[i].srcQueueFamilyIndex =VK_QUEUE_FAMILY_IGNORED;
        barriers[i].dstQueueFamilyIndex =VK_QUEUE_FAMILY_IGNORED;
        barriers[i].image               =tex->GetImage();
        barriers[i].subresourceRange    ={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
    }

    if(has_depth)
    {
        Texture2D *depth_tex=rt->GetDepthTexture();
        if(depth_tex)
        {
            VkImageMemoryBarrier &db=barriers[color_count];

            db.sType               =VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            db.srcAccessMask       =0;
            db.dstAccessMask       =VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            db.oldLayout           =VK_IMAGE_LAYOUT_UNDEFINED;
            db.newLayout           =VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            db.srcQueueFamilyIndex =VK_QUEUE_FAMILY_IGNORED;
            db.dstQueueFamilyIndex =VK_QUEUE_FAMILY_IGNORED;
            db.image               =depth_tex->GetImage();
            // D32_SFLOAT_S8_UINT 混合格式：aspectMask 必须同时含 DEPTH+STENCIL
            //（VUID-VkImageMemoryBarrier-image-03320，未启用 separateDepthStencilLayouts）
            db.subresourceRange    ={VK_IMAGE_ASPECT_DEPTH_BIT|VK_IMAGE_ASPECT_STENCIL_BIT,0,1,0,1};
        }
    }

    vkCmdPipelineBarrier(cmd_buf,
                         VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT|VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                         0,
                         0,nullptr,
                         0,nullptr,
                         color_count+has_depth,barriers);

    // render_area / viewport 从 render target extent 设置
    //（原 BindFramebuffer 负责此初始化；dynamic rendering 下无 framebuffer，改在此处）
    const VkExtent2D &ext=rt->GetExtent();

    render_area.offset.x=0;
    render_area.offset.y=0;
    render_area.extent=ext;

    viewport.x          =0;
    viewport.y          =0;
    viewport.minDepth   =0.0f;
    viewport.maxDepth   =1.0f;
    viewport.width      =static_cast<float>(ext.width);
    viewport.height     =static_cast<float>(ext.height);

    // clear value 数组：color_count 个颜色 + 1 个深度（如需要）
    const uint32_t clear_count=color_count+has_depth;

    if(cv_count<clear_count)
    {
        clear_values=hgl_align_realloc<VkClearValue>(clear_values,clear_count);
        cv_count=clear_count;
        // 不调 SetClear()——其"最后一个是 depth"的语义是老 render pass 布局，
        // dynamic rendering 下 color/depth 的 clear 值分别从 clear_values[0..color_count) 与 [color_count] 取
    }

    VkRenderingAttachmentInfo color_atts[8]{};

    for(uint32_t i=0;i<color_count;i++)
    {
        const RenderingAttachment att=rt->GetColorAttachment(i);
        if(!att.IsValid())continue;

        color_atts[i].sType         =VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        color_atts[i].imageView     =att.image_view;
        color_atts[i].imageLayout   =VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        color_atts[i].loadOp        =VK_ATTACHMENT_LOAD_OP_CLEAR;
        color_atts[i].storeOp       =VK_ATTACHMENT_STORE_OP_STORE;
        color_atts[i].clearValue    =(i<cv_count)?clear_values[i]:VkClearValue{};
    }

    VkRenderingAttachmentInfo depth_att{};

    if(rt->hasDepth())
    {
        const RenderingAttachment att=rt->GetDepthAttachment();

        if(att.IsValid())
        {
            depth_att.sType         =VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            depth_att.imageView     =att.image_view;
            depth_att.imageLayout   =VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            depth_att.loadOp        =VK_ATTACHMENT_LOAD_OP_CLEAR;
            depth_att.storeOp       =VK_ATTACHMENT_STORE_OP_STORE;
            depth_att.clearValue    =(color_count<cv_count)?clear_values[color_count]:VkClearValue{};
        }
    }

    VkRenderingInfo ri{};
    ri.sType                =VK_STRUCTURE_TYPE_RENDERING_INFO;
    ri.renderArea           =render_area;
    ri.layerCount           =1;
    ri.colorAttachmentCount =color_count;
    ri.pColorAttachments    =color_atts;
    ri.pDepthAttachment     =depth_att.imageView?&depth_att:nullptr;

    vkCmdBeginRendering(cmd_buf,&ri);

    vkCmdSetViewport(cmd_buf,0,1,&viewport);
    vkCmdSetScissor(cmd_buf,0,1,&render_area);

    pipeline_layout=VK_NULL_HANDLE;

    return(true);
}

void RenderCmdBuffer::EndRenderingPresent(IRenderTarget *rt)
{
    vkCmdEndRendering(cmd_buf);

    if(!rt)return;

    const uint32_t color_count=rt->GetColorCount();

    VkImageMemoryBarrier barriers[8]{};

    for(uint32_t i=0;i<color_count;i++)
    {
        Texture2D *tex=rt->GetColorTexture(i);
        if(!tex)continue;

        barriers[i].sType               =VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barriers[i].srcAccessMask       =VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barriers[i].dstAccessMask       =0;
        barriers[i].oldLayout           =VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        barriers[i].newLayout           =VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        barriers[i].srcQueueFamilyIndex =VK_QUEUE_FAMILY_IGNORED;
        barriers[i].dstQueueFamilyIndex =VK_QUEUE_FAMILY_IGNORED;
        barriers[i].image               =tex->GetImage();
        barriers[i].subresourceRange    ={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
    }

    if(color_count>0)
        vkCmdPipelineBarrier(cmd_buf,
                             VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                             VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                             0,
                             0,nullptr,
                             0,nullptr,
                             color_count,barriers);
}

void RenderCmdBuffer::DrawMeshTasks(const uint32_t group_count_x,const uint32_t group_count_y,const uint32_t group_count_z)
{
    if(!dev_attr||!dev_attr->cmd_draw_mesh_tasks)
        return;

    dev_attr->cmd_draw_mesh_tasks(cmd_buf,group_count_x,group_count_y,group_count_z);
}

void RenderCmdBuffer::DrawMeshTasksIndirect(VkBuffer buffer,VkDeviceSize offset,uint32_t drawCount,uint32_t stride)
{
    if(!dev_attr||!dev_attr->cmd_draw_mesh_tasks_indirect)
        return;

    if(this->dev_attr->physical_device->SupportMDI())
        dev_attr->cmd_draw_mesh_tasks_indirect(cmd_buf,buffer,offset,drawCount,stride);
    else
    for(uint32_t i=0;i<drawCount;i++)
        dev_attr->cmd_draw_mesh_tasks_indirect(cmd_buf,buffer,offset+i*stride,1,stride);
}

bool RenderCmdBuffer::BindDescriptorSets(ShaderProgram *mtl, MaterialParameters *override_per_object, MaterialParameters *override_material)
{
    if(!mtl)return(false);

    pipeline_layout=mtl->GetPipelineLayout();

    ENUM_CLASS_FOR(DescriptorSetType,int,i)
    {
        MaterialParameters *mp=nullptr;

        // per-object / material 集支持调用方传入独立 MP（多实例共享同一 ShaderProgram
        // 时，各实例必须使用自己的描述符集，避免录制期间互相覆盖）
        if(i==static_cast<int>(DescriptorSetType::PerObject))
            mp=override_per_object?override_per_object:mtl->GetMP((DescriptorSetType)i);
        else
        if(i==static_cast<int>(DescriptorSetType::Material))
            mp=override_material?override_material:mtl->GetMP((DescriptorSetType)i);
        else
            mp=mtl->GetMP((DescriptorSetType)i);

        if(!mp)
            continue;

        mp->Update();

        const VkDescriptorSet ds=mp->GetVkDescriptorSet();
        vkCmdBindDescriptorSets(cmd_buf,VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline_layout,uint32_t(i),1,&ds,0,nullptr);
    }

    return(true);
}
}//namespace hgl::graph


