#include<hgl/vk/VKCommandBuffer.h>
#include<hgl/vk/VKDomainMaterialBinding.h>
#include<hgl/log/Log.h>
#include<hgl/vk/pipeline/VKGraphicsPipelineLayoutData.h>
#include<hgl/vk/pipeline/VKRenderTargetFormat.h>
#include<hgl/graph/mesh/Primitive.h>
#include<hgl/vk/VKDeviceAttribute.h>
#include<hgl/vk/VKPhysicalDevice.h>
#include<hgl/vk/VKIndexBuffer.h>
#include<hgl/vk/VKRenderTarget.h>
#include<hgl/graph/module/DescriptorTimingDiagnostics.h>
#include<hgl/vk/VKRenderTargetData.h>
#include<hgl/vk/VKTexture.h>

namespace hgl::graph{
bool RenderCmdBuffer::BindVAB(const VABList *vab_list)
{
    if(!vab_list)
    {
        LogError("VABList is null");
        return(false);
    }

    if(!vab_list->IsFull())
    {
        LogError("VABList is not full");
        return(false);
    }

    vkCmdBindVertexBuffers(cmd_buf,
                           0,                           //first binding
                           vab_list->GetWriteCount(),   //binding count (use actual count, not capacity)
                           vab_list->GetVABList(),      //buffers
                           vab_list->GetVABOffset());   //buffer offsets

    return(true);
}

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

bool RenderCmdBuffer::BeginSetup(const RenderTargetData *rtd)
{
    if(!rtd) return false;

    cv_count = rtd->color_count + (rtd->depth_texture ? 1 : 0);
    SetClear();

    render_area.offset = {0, 0};
    render_area.extent = rtd->extent;

    viewport.x        = 0;
    viewport.y        = 0;
    viewport.width    = (float)rtd->extent.width;
    viewport.height   = (float)rtd->extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    return true;
}

bool RenderCmdBuffer::BindDescriptorSets(MaterialTemplate *mtl)
{
    if(!mtl)return(false);

    {
        uint32_t count=0;

        MaterialParameters *mp;
        VkDescriptorSet ds[DESCRIPTOR_SET_TYPE_COUNT];

        LOG_DESC_TIMING("RenderCmdBuffer::BindDescriptorSets(MaterialTemplate) ENTRY mtl=%p", (void*)mtl);

        ENUM_CLASS_FOR(DescriptorSetType,int,i)
        {
            mp=mtl->GetMP((DescriptorSetType)i);

            if(mp)
            {
                LOG_DESC_TIMING("  DescriptorSetType[%d]: calling mp->Update() mp=%p", (int)i, (void*)mp);
                mp->Update();
                LOG_DESC_TIMING("  DescriptorSetType[%d]: mp->Update() DONE, obtained vk handle=%p", (int)i, (void*)mp->GetVkDescriptorSet());

                ds[count]=mp->GetVkDescriptorSet();
                ++count;
            }
        }

        if(count>0)
        {
            pipeline_layout=mtl->GetPipelineLayout();

            LOG_DESC_TIMING("  calling vkCmdBindDescriptorSets with count=%u", count);
            vkCmdBindDescriptorSets(cmd_buf,VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline_layout,0,count,ds,0,0);
            LOG_DESC_TIMING("  vkCmdBindDescriptorSets DONE");
        }

        LOG_DESC_TIMING("RenderCmdBuffer::BindDescriptorSets(MaterialTemplate) EXIT");
    }

    return(true);
}

bool RenderCmdBuffer::BindDescriptorSets(DomainMaterialBinding *binding)
{
    if(!binding) return false;

    LOG_DESC_TIMING("RenderCmdBuffer::BindDescriptorSets(DomainMaterialBinding) ENTRY binding=%p", (void*)binding);

    MaterialParameters *mp = binding->GetPerMaterialMP();
    if(!mp) {
        LOG_DESC_TIMING("  No PerMaterialMP found");
        return false;
    }

    LOG_DESC_TIMING("  calling mp->Update() mp=%p", (void*)mp);
    mp->Update();
    LOG_DESC_TIMING("  mp->Update() DONE");
    const VkDescriptorSet ds = mp->GetVkDescriptorSet();

    const auto *pld = binding->GetMaterial()->GetGraphicsPipelineLayoutData();
    const uint32_t first_set = pld ? (uint32_t)pld->GetVulkanSetIndex(DescriptorSetType::PerMaterial) : 0;

    pipeline_layout = binding->GetPipelineLayout();
    LOG_DESC_TIMING("  calling vkCmdBindDescriptorSets first_set=%u", first_set);
    vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipeline_layout, first_set, 1, &ds, 0, nullptr);
    LOG_DESC_TIMING("RenderCmdBuffer::BindDescriptorSets(DomainMaterialBinding) EXIT");

    return true;
}

void RenderCmdBuffer::BindIBO(IndexBuffer *ibo,const VkDeviceSize byte_offset)
{
    //LogVerbose(u"BindIBO entry");

    if(!ibo)
    {
        LogError("Null IBO");
        return;
    }

    //std::cerr << "[RenderCmdBuffer::BindIBO] IBO buffer: " << ibo->GetBuffer() << std::endl;
    //std::cerr << "[RenderCmdBuffer::BindIBO] IBO type: " << (int)ibo->GetType() << std::endl;
    //std::cerr << "[RenderCmdBuffer::BindIBO] Byte offset: " << byte_offset << std::endl;

    vkCmdBindIndexBuffer(cmd_buf,
                         ibo->GetVkBuffer(),
                         byte_offset,
                         VkIndexType(ibo->GetType()));

//    std::cerr << "[RenderCmdBuffer::BindIBO] === EXIT ===" << std::endl;
}

bool RenderCmdBuffer::BindDataBuffer(const GeometryDataBuffer *geom_data_buffer)
{
    //LogVerbose(u"BindDataBuffer entry");

    if(!geom_data_buffer)
    {
        LogError("Null geometry data buffer");
        return(false);
    }

    if(geom_data_buffer->vab_count<=0)
    {
        LogError("No VABs to bind");
        return(false);
    }

    {
        static uint32_t s_binddatabuf_tick = 0;
        if (++s_binddatabuf_tick <= 4u)
        {
            for (uint32_t _i = 0; _i < geom_data_buffer->vab_count; ++_i)
                GLogDebug("[VULKAN_BIND_VBO] tick=%u slot[%u] VkBuffer=%p offset=%llu",
                          s_binddatabuf_tick, _i,
                          (void*)geom_data_buffer->vab_list[_i],
                          (unsigned long long)geom_data_buffer->vab_offset[_i]);
        }
    }

    vkCmdBindVertexBuffers(cmd_buf,
                           0,               //first binding
                           geom_data_buffer->vab_count,
                           geom_data_buffer->vab_list,
                           geom_data_buffer->vab_offset);        //vab byte offsets

//    std::cerr << "[RenderCmdBuffer::BindDataBuffer] Vertex buffers bound" << std::endl;

    if(geom_data_buffer->ibo)
    {
//        std::cerr << "[RenderCmdBuffer::BindDataBuffer] Binding IBO: " << geom_data_buffer->ibo << std::endl;
        BindIBO(geom_data_buffer->ibo);
    }
    else
    {
//        std::cerr << "[RenderCmdBuffer::BindDataBuffer] No IBO to bind" << std::endl;
    }

//    std::cerr << "[RenderCmdBuffer::BindDataBuffer] === EXIT (success) ===" << std::endl;
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

void RenderCmdBuffer::DrawIndexedIndirect(  VkBuffer        buffer,
                                            VkDeviceSize    offset,
                                            uint32_t        drawCount,
                                            uint32_t        stride)
{
    if(this->dev_attr->physical_device->SupportMDI())
        vkCmdDrawIndexedIndirect(cmd_buf,buffer,offset,drawCount,stride);
    else
    for(uint32_t i=0;i<drawCount;i++)
        vkCmdDrawIndexedIndirect(cmd_buf,buffer,offset+i*stride,1,stride);
}

void RenderCmdBuffer::Draw(const GeometryDataBuffer *geom_data_buffer,const GeometryDrawRange *geom_draw_range,const uint32_t instance_count,const uint32_t first_instance)
{
    //LogVerbose(u"Draw entry");

    if(!geom_data_buffer||!geom_draw_range)
    {
        LogError("Null parameter in Draw");
        return;
    }

    if (geom_data_buffer->ibo)
    {
        //std::cerr << "[RenderCmdBuffer::Draw] Using INDEXED draw" << std::endl;
        //std::cerr << "[RenderCmdBuffer::Draw]   index_count: " << geom_draw_range->index_count << std::endl;
        //std::cerr << "[RenderCmdBuffer::Draw]   first_index: " << geom_draw_range->first_index << std::endl;
        //std::cerr << "[RenderCmdBuffer::Draw]   vertex_offset: " << geom_draw_range->vertex_offset << std::endl;

        vkCmdDrawIndexed(   cmd_buf,
                            geom_draw_range->index_count,
                            instance_count,
                            geom_draw_range->first_index,
                            geom_draw_range->vertex_offset, //这里的vertexOffset是针对所有VAB的
                            first_instance);    //这里的first_instance针对的是instance Rate更新的VAB的起始实例数，不是指instance批量渲染

//        std::cerr << "[RenderCmdBuffer::Draw] vkCmdDrawIndexed called" << std::endl;
    }
    else
    {
        //std::cerr << "[RenderCmdBuffer::Draw] Using NON-INDEXED draw" << std::endl;
        //std::cerr << "[RenderCmdBuffer::Draw]   vertex_count: " << geom_draw_range->vertex_count << std::endl;
        //std::cerr << "[RenderCmdBuffer::Draw]   vertex_offset: " << geom_draw_range->vertex_offset << std::endl;

        vkCmdDraw(          cmd_buf,
                            geom_draw_range->vertex_count,
                            instance_count,
                            geom_draw_range->vertex_offset,
                            first_instance);

//        std::cerr << "[RenderCmdBuffer::Draw] vkCmdDraw called" << std::endl;
    }

//    std::cerr << "[RenderCmdBuffer::Draw] === EXIT ===" << std::endl;
}

//void RenderCmdBuffer::DrawIndexed(const IBAccess *iba,const uint32_t instance_count)
//{
//    if(!iba||instance_count<=0)return;
//
//    vkCmdBindIndexBuffer(cmd_buf,
//                         iba->buffer->GetBuffer(),
//                         iba->start*iba->buffer->GetStride(),
//                         VkIndexType(iba->buffer->GetType()));
//
//    vkCmdDrawIndexed(cmd_buf,
//                     iba->count,
//                     instance_count,
//                     0,                 //first index
//                     0,                 //vertex offset
//                     0);                //first instance
//}

namespace
{
    inline VkImageAspectFlags GetDepthAspectFlags(VkFormat fmt)
    {
        switch(fmt)
        {
            case VK_FORMAT_D16_UNORM_S8_UINT:
            case VK_FORMAT_D24_UNORM_S8_UINT:
            case VK_FORMAT_D32_SFLOAT_S8_UINT:
                return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
            default:
                return VK_IMAGE_ASPECT_DEPTH_BIT;
        }
    }
}//anonymous namespace

bool RenderCmdBuffer::BeginRenderingDynamic(const RenderTargetData *rtd)
{
    if(!rtd) return false;

    const uint32_t barrier_count = rtd->color_count + (rtd->depth_texture ? 1 : 0);

    if(barrier_count > 0)
    {
        // Use a fixed upper-bound stack array (16 color attachments + 1 depth is well beyond typical use)
        VkImageMemoryBarrier barriers[17] = {};

        // Pre-render color barriers: UNDEFINED → COLOR_ATTACHMENT_OPTIMAL
        // (loadOp=CLEAR means we discard previous content, so UNDEFINED old-layout is always valid)
        for(uint32_t i = 0; i < rtd->color_count; i++)
        {
            Texture2D *tex = rtd->color_textures[i];
            VkImageMemoryBarrier &b = barriers[i];
            b.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            b.srcAccessMask                   = 0;
            b.dstAccessMask                   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            b.oldLayout                       = VK_IMAGE_LAYOUT_UNDEFINED; // discard; we CLEAR
            b.newLayout                       = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            b.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
            b.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
            b.image                           = tex->GetImage();
            b.subresourceRange                = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        }

        // Pre-render depth barrier: UNDEFINED → DEPTH_STENCIL_ATTACHMENT_OPTIMAL
        if(rtd->depth_texture)
        {
            Texture2D *tex = rtd->depth_texture;
            VkImageMemoryBarrier &b = barriers[rtd->color_count];
            b.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            b.srcAccessMask                   = 0;
            b.dstAccessMask                   = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            b.oldLayout                       = VK_IMAGE_LAYOUT_UNDEFINED; // discard; we CLEAR
            b.newLayout                       = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            b.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
            b.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
            b.image                           = tex->GetImage();
            b.subresourceRange                = {GetDepthAspectFlags(tex->GetFormat()), 0, 1, 0, 1};
        }

        vkCmdPipelineBarrier(cmd_buf,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
            0,
            0, nullptr,
            0, nullptr,
            barrier_count, barriers);
    }

    // Build color attachment info array
    VkRenderingAttachmentInfoKHR color_attachments[16] = {};
    for(uint32_t i = 0; i < rtd->color_count; i++)
    {
        Texture2D *tex = rtd->color_textures[i];
        VkRenderingAttachmentInfoKHR &ai  = color_attachments[i];
        ai.sType                          = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
        ai.imageView                      = tex->GetVulkanImageView();
        ai.imageLayout                    = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        ai.resolveMode                    = VK_RESOLVE_MODE_NONE;
        ai.resolveImageView               = VK_NULL_HANDLE;
        ai.resolveImageLayout             = VK_IMAGE_LAYOUT_UNDEFINED;
        ai.loadOp                         = VK_ATTACHMENT_LOAD_OP_CLEAR;
        ai.storeOp                        = VK_ATTACHMENT_STORE_OP_STORE;
        ai.clearValue                     = clear_values[i];
    }

    // Build depth attachment info
    VkRenderingAttachmentInfoKHR depth_attachment = {};
    const bool has_depth = (rtd->depth_texture != nullptr);
    if(has_depth)
    {
        Texture2D *tex = rtd->depth_texture;
        depth_attachment.sType            = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
        depth_attachment.imageView        = tex->GetVulkanImageView();
        depth_attachment.imageLayout      = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depth_attachment.resolveMode      = VK_RESOLVE_MODE_NONE;
        depth_attachment.resolveImageView = VK_NULL_HANDLE;
        depth_attachment.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depth_attachment.loadOp           = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depth_attachment.storeOp          = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depth_attachment.clearValue       = clear_values[cv_count - 1]; // last slot is depth
    }

    VkRenderingInfoKHR rendering_info    = {};
    rendering_info.sType                 = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;
    rendering_info.renderArea            = render_area;
    rendering_info.layerCount            = 1;
    rendering_info.colorAttachmentCount  = rtd->color_count;
    rendering_info.pColorAttachments     = color_attachments;
    rendering_info.pDepthAttachment      = has_depth ? &depth_attachment : nullptr;
    rendering_info.pStencilAttachment    = nullptr;

    dev_attr->pfn_vkCmdBeginRenderingKHR(cmd_buf, &rendering_info);

    vkCmdSetViewport(cmd_buf, 0, 1, &viewport);
    vkCmdSetScissor (cmd_buf, 0, 1, &render_area);

    pipeline_layout = VK_NULL_HANDLE;

    return true;
}

void RenderCmdBuffer::EndRenderingDynamic(const RenderTargetData *rtd)
{
    dev_attr->pfn_vkCmdEndRenderingKHR(cmd_buf);

    if(!rtd || rtd->color_count == 0) return;

    // Post-render barrier: COLOR_ATTACHMENT_OPTIMAL → final_color_layout
    // srcAccess = write, dstAccess = 0 (semaphore handles cross-submission visibility)
    VkImageMemoryBarrier barriers[16] = {};
    for(uint32_t i = 0; i < rtd->color_count; i++)
    {
        Texture2D *tex = rtd->color_textures[i];
        VkImageMemoryBarrier &b = barriers[i];
        b.sType                 = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        b.srcAccessMask         = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        b.dstAccessMask         = 0;
        b.oldLayout             = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        b.newLayout             = rtd->final_color_layout;
        b.srcQueueFamilyIndex   = VK_QUEUE_FAMILY_IGNORED;
        b.dstQueueFamilyIndex   = VK_QUEUE_FAMILY_IGNORED;
        b.image                 = tex->GetImage();
        b.subresourceRange      = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

        // Update CPU-side layout tracking
        TextureData *td = tex->GetData();
        if(td) td->image_layout = rtd->final_color_layout;
    }

    vkCmdPipelineBarrier(cmd_buf,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        0,
        0, nullptr,
        0, nullptr,
        rtd->color_count, barriers);
}

}//namespace hgl::graph
