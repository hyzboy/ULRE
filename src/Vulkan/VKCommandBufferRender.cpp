#include<hgl/vk/VKCommandBuffer.h>
#include<hgl/vk/VKRenderPass.h>
#include<hgl/vk/VKFramebuffer.h>
#include<hgl/graph/mesh/Primitive.h>
#include<hgl/vk/VKDeviceAttribute.h>
#include<hgl/vk/VKPhysicalDevice.h>
#include<hgl/vk/buffer/IndexBuffer.h>
#include<hgl/vk/VKRenderTarget.h>

namespace hgl::graph
{
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

bool RenderCmdBuffer::BeginRendering(IRenderTarget *rt, const RenderPassOptions *options)
{
    if(!rt)return(false);

    const uint32_t color_count=rt->GetColorCount();
    const uint32_t has_depth=rt->hasDepth()?1:0;

    // Dynamic Rendering：无 render pass 的自动布局转换——必须先手动把附件
    // 从 UNDEFINED 转换到 attachment 布局（VUID-vkCmdBeginRendering-pRenderingInfo-09592/09588）
    VkImageMemoryBarrier2 barriers[8]{};

    for(uint32_t i=0;i<color_count;i++)
    {
        Texture2D *tex=rt->GetColorTexture(i);
        if(!tex)continue;

        barriers[i].sType               =VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
        barriers[i].srcStageMask        =VK_PIPELINE_STAGE_2_NONE;
        barriers[i].srcAccessMask       =VK_ACCESS_2_NONE;
        barriers[i].dstStageMask        =VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        barriers[i].dstAccessMask       =VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
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
            VkImageMemoryBarrier2 &db=barriers[color_count];

            const bool load_depth = options && options->load_depth;
            VkImageLayout old_layout = VK_IMAGE_LAYOUT_UNDEFINED;
            VkPipelineStageFlags2 src_stage = VK_PIPELINE_STAGE_2_NONE;
            VkAccessFlags2 src_access = VK_ACCESS_2_NONE;

            if (load_depth)
            {
                old_layout = (options && options->depth_old_layout != VK_IMAGE_LAYOUT_UNDEFINED)
                             ? options->depth_old_layout
                             : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                src_stage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
                src_access = VK_ACCESS_2_SHADER_READ_BIT;
            }

            db.sType               =VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
            db.srcStageMask        =src_stage;
            db.srcAccessMask       =src_access;
            db.dstStageMask        =VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT|VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
            db.dstAccessMask       =VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            db.oldLayout           =old_layout;
            db.newLayout           =VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            db.srcQueueFamilyIndex =VK_QUEUE_FAMILY_IGNORED;
            db.dstQueueFamilyIndex =VK_QUEUE_FAMILY_IGNORED;
            db.image               =depth_tex->GetImage();
            // aspectMask 必须与格式匹配：D32_SFLOAT_S8_UINT 这类混合格式要 DEPTH+STENCIL
            //（VUID-VkImageMemoryBarrier-image-03320，未启用 separateDepthStencilLayouts），
            // 而 D16_UNORM / D32_SFLOAT 这类纯深度格式不能带 STENCIL 位。
            db.subresourceRange    ={IsStencilFormat(depth_tex->GetFormat())
                                         ?static_cast<VkImageAspectFlags>(VK_IMAGE_ASPECT_DEPTH_BIT|VK_IMAGE_ASPECT_STENCIL_BIT)
                                         :static_cast<VkImageAspectFlags>(VK_IMAGE_ASPECT_DEPTH_BIT),
                                    0,1,0,1};
        }
    }

    VkDependencyInfo dep_info{};
    dep_info.sType                    =VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dep_info.imageMemoryBarrierCount =color_count+has_depth;
    dep_info.pImageMemoryBarriers    =barriers;

    PipelineBarrier2(&dep_info);

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

    // 深度清屏值：本引擎为 **Reversed-Z**——`mtl::PipelineConfig::depth_compare_op`
    // 默认 VK_COMPARE_OP_GREATER_OR_EQUAL，`Camera::use_reversed_z` 默认 true，
    // 投影走 MakeInfiniteReversedZProj（近平面 1.0 / 远平面 0.0）。
    // 因此深度附件必须清 **0.0f**（远平面）。清 1.0f 会让 GREATER_OR_EQUAL 拒绝
    // 所有片元——整个场景只剩清屏色，表现为「示例什么都看不到」。
    // 也与 SetClearDepthStencil() 的既有默认值一致。
    if(has_depth)
        SetClearDepthStencil(color_count,0.0f,0.0f);

    VkRenderingAttachmentInfo color_atts[8]{};

    for(uint32_t i=0;i<color_count;i++)
    {
        const RenderingAttachment att=rt->GetColorAttachment(i);
        if(!att.IsValid())continue;

        const bool load_color = options && options->load_color;

        color_atts[i].sType         =VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        color_atts[i].imageView     =att.image_view;
        color_atts[i].imageLayout   =VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        color_atts[i].loadOp        =load_color ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_CLEAR;
        color_atts[i].storeOp       =VK_ATTACHMENT_STORE_OP_STORE;
        color_atts[i].clearValue    =(i<cv_count)?clear_values[i]:VkClearValue{};
    }

    VkRenderingAttachmentInfo depth_att{};

    if(rt->hasDepth())
    {
        const RenderingAttachment att=rt->GetDepthAttachment();

        if(att.IsValid())
        {
            const bool load_depth = options && options->load_depth;

            depth_att.sType         =VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            depth_att.imageView     =att.image_view;
            depth_att.imageLayout   =VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            depth_att.loadOp        =load_depth ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_CLEAR;
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

    if (options && options->use_scissor)
    {
        vkCmdSetScissor(cmd_buf,0,1,&options->scissor);
    }
    else
    {
        vkCmdSetScissor(cmd_buf,0,1,&render_area);
    }

    if (options && options->clear_scissor_depth && rt->hasDepth())
    {
        VkClearAttachment clear_att{};
        clear_att.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        clear_att.clearValue.depthStencil.depth = options->clear_depth_value;
        clear_att.clearValue.depthStencil.stencil = 0;

        VkClearRect clear_rect{};
        clear_rect.rect = options->use_scissor ? options->scissor : render_area;
        clear_rect.baseArrayLayer = 0;
        clear_rect.layerCount = 1;

        vkCmdClearAttachments(cmd_buf, 1, &clear_att, 1, &clear_rect);
    }

    pipeline_layout=VK_NULL_HANDLE;

    return(true);
}

void RenderCmdBuffer::ClearDepthRect(const VkRect2D &rect, float depth)
{
    VkClearAttachment clear_att{};
    clear_att.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    clear_att.clearValue.depthStencil.depth = depth;
    clear_att.clearValue.depthStencil.stencil = 0;

    VkClearRect clear_rect{};
    clear_rect.rect = rect;
    clear_rect.baseArrayLayer = 0;
    clear_rect.layerCount = 1;

    vkCmdClearAttachments(cmd_buf, 1, &clear_att, 1, &clear_rect);
}

void RenderCmdBuffer::EndRenderingPresent(IRenderTarget *rt)
{
    vkCmdEndRendering(cmd_buf);

    if(!rt)return;

    const uint32_t color_count = rt->GetColorCount();
    Texture2D *depth_tex = rt->hasDepth() ? rt->GetDepthTexture() : nullptr;

    // ---- 窗口交换链：颜色附件转 PRESENT_SRC 交呈现引擎（维持原有语义）----
    if(rt->IsSwapchain())
    {
        VkImageMemoryBarrier2 barriers[8]{};

        for(uint32_t i=0;i<color_count;i++)
        {
            Texture2D *tex=rt->GetColorTexture(i);
            if(!tex)continue;

            barriers[i].sType               =VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
            barriers[i].srcStageMask        =VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
            barriers[i].srcAccessMask       =VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
            barriers[i].dstStageMask        =VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
            barriers[i].dstAccessMask       =VK_ACCESS_2_NONE;
            barriers[i].oldLayout           =VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            barriers[i].newLayout           =VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            barriers[i].srcQueueFamilyIndex =VK_QUEUE_FAMILY_IGNORED;
            barriers[i].dstQueueFamilyIndex =VK_QUEUE_FAMILY_IGNORED;
            barriers[i].image               =tex->GetImage();
            barriers[i].subresourceRange    ={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
        }

        if(color_count>0)
        {
            VkDependencyInfo dep_info{};
            dep_info.sType                    =VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
            dep_info.imageMemoryBarrierCount =color_count;
            dep_info.pImageMemoryBarriers    =barriers;

            PipelineBarrier2(&dep_info);
        }

        return;
    }

    // ---- 离屏：转可采样布局，供后续 pass 绑定采样 ----
    // 颜色与深度都转 SHADER_READ_ONLY_OPTIMAL——采样侧（BindlessTextureManager）
    // 的 descriptor imageLayout 固定为 SHADER_READ_ONLY_OPTIMAL，两者必须一致。
    // depth-only RT（shadow map）没有任何颜色附件，深度是唯一需要转换的附件——
    // 原实现只在 color_count>0 时才发 barrier，导致纯深度目标的深度停留在
    // attachment 布局，采样即为非法。
    VkImageMemoryBarrier2 barriers[8]{};
    uint32_t barrier_count=0;

    for(uint32_t i=0;i<color_count&&barrier_count<8;i++)
    {
        Texture2D *tex=rt->GetColorTexture(i);
        if(!tex)continue;

        VkImageMemoryBarrier2 &b=barriers[barrier_count];

        b.sType               =VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
        b.srcStageMask        =VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        b.srcAccessMask       =VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
        b.dstStageMask        =VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
        b.dstAccessMask       =VK_ACCESS_2_SHADER_READ_BIT;
        b.oldLayout           =VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        b.newLayout           =VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        b.srcQueueFamilyIndex =VK_QUEUE_FAMILY_IGNORED;
        b.dstQueueFamilyIndex =VK_QUEUE_FAMILY_IGNORED;
        b.image               =tex->GetImage();
        b.subresourceRange    ={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};

        ++barrier_count;
    }

    if(depth_tex&&barrier_count<8)
    {
        VkImageMemoryBarrier2 &b=barriers[barrier_count];

        b.sType               =VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
        b.srcStageMask        =VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT|VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
        b.srcAccessMask       =VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        b.dstStageMask        =VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
        b.dstAccessMask       =VK_ACCESS_2_SHADER_READ_BIT;
        b.oldLayout           =VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        b.newLayout           =VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        b.srcQueueFamilyIndex =VK_QUEUE_FAMILY_IGNORED;
        b.dstQueueFamilyIndex =VK_QUEUE_FAMILY_IGNORED;
        b.image               =depth_tex->GetImage();
        // aspectMask 必须与格式匹配：纯深度格式（D16/D32）不能声明 STENCIL 位
        b.subresourceRange    ={IsStencilFormat(depth_tex->GetFormat())
                                     ?static_cast<VkImageAspectFlags>(VK_IMAGE_ASPECT_DEPTH_BIT|VK_IMAGE_ASPECT_STENCIL_BIT)
                                     :static_cast<VkImageAspectFlags>(VK_IMAGE_ASPECT_DEPTH_BIT),
                                0,1,0,1};

        ++barrier_count;
    }

    if(barrier_count>0)
    {
        VkDependencyInfo dep_info{};
        dep_info.sType                    =VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        dep_info.imageMemoryBarrierCount =barrier_count;
        dep_info.pImageMemoryBarriers    =barriers;

        PipelineBarrier2(&dep_info);
    }
}

void RenderCmdBuffer::ApplyPipelineState(const mtl::MaterialPipelineConfig &config)
{
    // EDS 1/2/3 动态状态应用（与 Resolver 的 pDynamicState 列表一一对应）：
    // pipeline 只保留 shader 部分，材质渲染状态全部在渲染侧设置。
    // 函数指针设备创建时 vkGetDeviceProcAddr 加载一次（VulkanDevAttr）。

    if(!dev_attr)
        return;

    if(dev_attr->cmd_set_cull_mode)
        dev_attr->cmd_set_cull_mode(cmd_buf, static_cast<VkCullModeFlags>(config.cull_mode));

    if(dev_attr->cmd_set_depth_test_enable)
        dev_attr->cmd_set_depth_test_enable(cmd_buf, config.depth_test ? VK_TRUE : VK_FALSE);
    if(dev_attr->cmd_set_depth_write_enable)
        dev_attr->cmd_set_depth_write_enable(cmd_buf, config.depth_write ? VK_TRUE : VK_FALSE);
    if(dev_attr->cmd_set_depth_compare_op)
        dev_attr->cmd_set_depth_compare_op(cmd_buf, config.depth_compare_op);

    // 颜色混合：attachment 0（alpha 通道 src=ONE/dst=ZERO——RGB 通道混合、alpha 通道直通，
    // 与旧 pipeline 烘焙路径的 SetAlphaBlend 一致）
    const VkBool32 blend_enable = config.alpha_blend ? VK_TRUE : VK_FALSE;

    if(dev_attr->cmd_set_color_blend_enable)
        dev_attr->cmd_set_color_blend_enable(cmd_buf, 0u, 1u, &blend_enable);

    if(dev_attr->cmd_set_color_blend_equation)
    {
        VkColorBlendEquationEXT blend_equation{};
        blend_equation.srcColorBlendFactor = config.blend_src;
        blend_equation.dstColorBlendFactor = config.blend_dst;
        blend_equation.colorBlendOp = VK_BLEND_OP_ADD;
        blend_equation.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        blend_equation.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        blend_equation.alphaBlendOp = VK_BLEND_OP_ADD;
        dev_attr->cmd_set_color_blend_equation(cmd_buf, 0u, 1u, &blend_equation);
    }

    if(dev_attr->cmd_set_color_write_mask)
    {
        const VkColorComponentFlags write_mask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
            | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        dev_attr->cmd_set_color_write_mask(cmd_buf, 0u, 1u, &write_mask);
    }

    const float blend_constants[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    vkCmdSetBlendConstants(cmd_buf, blend_constants);   // 1.0 核心函数（无 EXT 名）

    if(dev_attr->cmd_set_polygon_mode)
        dev_attr->cmd_set_polygon_mode(cmd_buf, config.wireframe ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL);

    vkCmdSetLineWidth(cmd_buf, config.line_width);

    if(dev_attr->cmd_set_alpha_to_coverage_enable)
        dev_attr->cmd_set_alpha_to_coverage_enable(cmd_buf, config.alpha_to_coverage ? VK_TRUE : VK_FALSE);
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

    // 多命令直发——引擎设备创建强制 multiDrawIndirect（VKDeviceCreater VHRC_F10），
    // 无逐条退化路径（零兼容：必用不留分支）
    dev_attr->cmd_draw_mesh_tasks_indirect(cmd_buf,buffer,offset,drawCount,stride);
}

void RenderCmdBuffer::DrawMeshTasksIndirectCount(VkBuffer buffer,VkDeviceSize offset,
                                                 VkBuffer countBuffer,VkDeviceSize countBufferOffset,
                                                 uint32_t maxDrawCount,uint32_t stride)
{
    if(!dev_attr||!dev_attr->cmd_draw_mesh_tasks_indirect_count)
        return;

    dev_attr->cmd_draw_mesh_tasks_indirect_count(cmd_buf,buffer,offset,countBuffer,countBufferOffset,maxDrawCount,stride);
}

}//namespace hgl::graph


