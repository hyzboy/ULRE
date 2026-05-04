#include<hgl/vk/VKCommandBuffer.h>
#include<hgl/vk/VKDomainResourceBinding.h>
#include<hgl/vk/pipeline/VKGraphicsPipelineLayoutData.h>
#include<hgl/vk/pipeline/VKRenderTargetFormat.h>
#include<hgl/graph/mesh/Primitive.h>
#include<hgl/vk/VKDeviceAttribute.h>
#include<hgl/vk/VKPhysicalDevice.h>
#include<hgl/vk/VKIndexBuffer.h>
#include<hgl/vk/VKRenderTarget.h>
#include<hgl/vk/VKRenderTargetData.h>
#include<hgl/vk/VKTexture.h>

namespace hgl::graph{
namespace
{
    bool CanDrawMeshTasks(const VulkanDevAttr *attr, const char *entry_name, const void *proc)
    {
        if (attr && attr->mesh_shader_extension && attr->mesh_shader_enabled && proc)
            return true;

        static bool warned = false;
        if (!warned)
        {
            GLogWarning("[RenderCmdBuffer::%s] Mesh shader draw command unavailable: ext=%s mesh=%s proc=%s",
                        entry_name ? entry_name : "<unknown>",
                        attr && attr->mesh_shader_extension ? "yes" : "no",
                        attr && attr->mesh_shader_enabled ? "yes" : "no",
                        proc ? "yes" : "no");
            warned = true;
        }

        return false;
    }

    bool CanDrawMeshTasksPlatform(const char *entry_name)
    {
        static bool warned = false;
        if (!warned)
        {
            GLogWarning("[RenderCmdBuffer::%s] Mesh shader command is not compiled on this platform/toolchain.",
                        entry_name ? entry_name : "<unknown>");
            warned = true;
        }
        return false;
    }
}

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
    bound_mesh_pipeline=false;
    frame_vertex_draw_count=0;
    frame_indexed_draw_count=0;
    frame_mesh_draw_count=0;
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

bool RenderCmdBuffer::BindDescriptorSets(ShaderMaterialProgram *mtl)
{
    if(!mtl)return(false);

    {
        uint32_t count=0;

        MaterialParameters *mp;
        VkDescriptorSet ds[DESCRIPTOR_SET_TYPE_COUNT];

        ENUM_CLASS_FOR(DescriptorSetType,int,i)
        {
            mp=mtl->GetMP((DescriptorSetType)i);

            if(mp)
            {
                mp->Update();

                ds[count]=mp->GetVkDescriptorSet();
                ++count;
            }
        }

        if(count>0)
        {
            pipeline_layout=mtl->GetPipelineLayout();

            vkCmdBindDescriptorSets(cmd_buf,VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline_layout,0,count,ds,0,0);
        }
    }

    return(true);
}

bool RenderCmdBuffer::BindDescriptorSets(DomainResourceBinding *binding)
{
    if(!binding) return false;

    // Keep non-PerMaterial sets (e.g. set0 PerObject) from material path bound first.
    BindDescriptorSets(binding->GetShaderMaterialProgram());

    const auto *pld = binding->GetShaderMaterialProgram()->GetGraphicsPipelineLayoutData();
    pipeline_layout = binding->GetPipelineLayout();

    // Override PerObject set with domain's own MP (isolates TransformData/TransformID per domain)
    MaterialParameters *po_mp = binding->GetPerObjectMP();
    if (po_mp)
    {
        po_mp->Update();
        const VkDescriptorSet po_ds = po_mp->GetVkDescriptorSet();
        const uint32_t first_set_po = pld ? (uint32_t)pld->GetVulkanSetIndex(DescriptorSetType::PerObject) : 2;
        vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                pipeline_layout, first_set_po, 1, &po_ds, 0, nullptr);
    }

    MaterialParameters *mp = binding->GetPerMaterialMP();
    if(!mp) return false;

    mp->Update();
    const VkDescriptorSet ds = mp->GetVkDescriptorSet();

    const uint32_t first_set = pld ? (uint32_t)pld->GetVulkanSetIndex(DescriptorSetType::PerMaterial) : 0;

        LogInfo("[RenderCmdBuffer] BindDescriptorSets(domain) material=%s domain=%p set=%u ds=%p",
            binding->GetShaderMaterialProgram() ? binding->GetShaderMaterialProgram()->GetName().c_str() : "<null>",
            static_cast<void *>(binding->GetDomain()),
            first_set,
                (void *)ds);

    vkCmdBindDescriptorSets(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipeline_layout, first_set, 1, &ds, 0, nullptr);

    return true;
}

void RenderCmdBuffer::BindIBO(IndexBuffer *ibo,const VkDeviceSize byte_offset)
{
    //LogVerbose(u"BindIBO entry");

    if(bound_mesh_pipeline)
    {
        static bool warned_mesh_ibo_bind = false;
        if(!warned_mesh_ibo_bind)
        {
            LogWarning("[RenderCmdBuffer::BindIBO] Ignore vkCmdBindIndexBuffer while mesh pipeline is bound");
            warned_mesh_ibo_bind = true;
        }
        return;
    }

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

    if(geom_data_buffer->vab_count<=0 && !geom_data_buffer->ibo)
    {
        LogError("No vertex or index buffers to bind");
        return(false);
    }

//    std::cerr << "[RenderCmdBuffer::BindDataBuffer] Calling vkCmdBindVertexBuffers..." << std::endl;

    // Log each buffer
    //for(uint32_t i = 0; i < geom_data_buffer->vab_count; i++)
    //{
    //    std::cerr << "[RenderCmdBuffer::BindDataBuffer]   Buffer[" << i << "]: "
    //              << geom_data_buffer->vab_list[i]
    //              << ", offset: " << geom_data_buffer->vab_offset[i] << std::endl;
    //}

    if(geom_data_buffer->vab_count>0)
    {
        vkCmdBindVertexBuffers(cmd_buf,
                               0,               //first binding
                               geom_data_buffer->vab_count,
                               geom_data_buffer->vab_list,
                               geom_data_buffer->vab_offset);        //vab byte offsets
    }

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

    frame_vertex_draw_count += drawCount;
}

void RenderCmdBuffer::DrawIndexedIndirect(  VkBuffer        buffer,
                                            VkDeviceSize    offset,
                                            uint32_t        drawCount,
                                            uint32_t        stride)
{
    if(bound_mesh_pipeline)
    {
        LogError("[RenderCmdBuffer::DrawIndexedIndirect] DrawIndexedIndirect is blocked while mesh pipeline is bound");
        return;
    }

    if(this->dev_attr->physical_device->SupportMDI())
        vkCmdDrawIndexedIndirect(cmd_buf,buffer,offset,drawCount,stride);
    else
    for(uint32_t i=0;i<drawCount;i++)
        vkCmdDrawIndexedIndirect(cmd_buf,buffer,offset+i*stride,1,stride);

    frame_indexed_draw_count += drawCount;
}

bool RenderCmdBuffer::DrawMeshTasks(const uint32_t group_count_x,
                                    const uint32_t group_count_y,
                                    const uint32_t group_count_z)
{
#ifdef VK_EXT_mesh_shader
    if (!CanDrawMeshTasks(dev_attr,
                          "DrawMeshTasks",
                          reinterpret_cast<const void *>(dev_attr ? dev_attr->pfn_vkCmdDrawMeshTasksEXT : nullptr)))
    {
        return false;
    }

    dev_attr->pfn_vkCmdDrawMeshTasksEXT(cmd_buf,
                                        group_count_x,
                                        group_count_y,
                                        group_count_z);
    ++frame_mesh_draw_count;
    return true;
#else
    (void)group_count_x;
    (void)group_count_y;
    (void)group_count_z;
    return CanDrawMeshTasksPlatform("DrawMeshTasks");
#endif
}

bool RenderCmdBuffer::DrawMeshTasksIndirect(VkBuffer buffer,
                                            VkDeviceSize offset,
                                            uint32_t draw_count,
                                            uint32_t stride)
{
#ifdef VK_EXT_mesh_shader
    if (!CanDrawMeshTasks(dev_attr,
                          "DrawMeshTasksIndirect",
                          reinterpret_cast<const void *>(dev_attr ? dev_attr->pfn_vkCmdDrawMeshTasksIndirectEXT : nullptr)))
    {
        return false;
    }

    dev_attr->pfn_vkCmdDrawMeshTasksIndirectEXT(cmd_buf,
                                                buffer,
                                                offset,
                                                draw_count,
                                                stride);
    frame_mesh_draw_count += draw_count;
    return true;
#else
    (void)buffer;
    (void)offset;
    (void)draw_count;
    (void)stride;
    return CanDrawMeshTasksPlatform("DrawMeshTasksIndirect");
#endif
}

bool RenderCmdBuffer::DrawMeshTasksIndirectCount(VkBuffer buffer,
                                                 VkDeviceSize offset,
                                                 VkBuffer count_buffer,
                                                 VkDeviceSize count_offset,
                                                 uint32_t max_draw_count,
                                                 uint32_t stride)
{
#ifdef VK_EXT_mesh_shader
    if (!CanDrawMeshTasks(dev_attr,
                          "DrawMeshTasksIndirectCount",
                          reinterpret_cast<const void *>(dev_attr ? dev_attr->pfn_vkCmdDrawMeshTasksIndirectCountEXT : nullptr)))
    {
        return false;
    }

    dev_attr->pfn_vkCmdDrawMeshTasksIndirectCountEXT(cmd_buf,
                                                     buffer,
                                                     offset,
                                                     count_buffer,
                                                     count_offset,
                                                     max_draw_count,
                                                     stride);
    ++frame_mesh_draw_count;
    return true;
#else
    (void)buffer;
    (void)offset;
    (void)count_buffer;
    (void)count_offset;
    (void)max_draw_count;
    (void)stride;
    return CanDrawMeshTasksPlatform("DrawMeshTasksIndirectCount");
#endif
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

        ++frame_indexed_draw_count;

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

        ++frame_vertex_draw_count;

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
