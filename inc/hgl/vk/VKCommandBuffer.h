#pragma once

#include<hgl/vk/VK.h>
#include<hgl/vk/VKVABList.h>
#include<hgl/vk/pipeline/VKPipeline.h>
#include<hgl/vk/VKDescriptorSet.h>
#include<hgl/graph/mesh/Primitive.h>
#include<hgl/color/Color4f.h>
#include<hgl/type/MemoryUtil.h>
#include<hgl/log/Log.h>
namespace hgl::graph{
class VulkanCmdBuffer
{
public:

    OBJECT_LOGGER

protected:

    const VulkanDevAttr *dev_attr;

    VkCommandBuffer cmd_buf;

    bool cmd_begin;

public:

    VulkanCmdBuffer(const VulkanDevAttr *attr,VkCommandBuffer cb);
    virtual ~VulkanCmdBuffer();

    operator VkCommandBuffer(){return cmd_buf;}
    operator const VkCommandBuffer()const{return cmd_buf;}
    operator const VkCommandBuffer *()const{return &cmd_buf;}

    const bool IsBegin()const{return cmd_begin;}

    virtual bool Begin();
    virtual bool End()
    {
        if(!cmd_begin)
            return(false);

        cmd_begin=false;

        return(vkEndCommandBuffer(cmd_buf)==VK_SUCCESS);
    }

#ifdef _DEBUG
    void SetDebugName(const AnsiString &);
    void BeginRegion(const AnsiString &,const Color4f &);
    void EndRegion();
#else
    void BeginRegion(const AnsiString &,const Color4f &){}
    void EndRegion(){}
#endif//_DEBUG
};//class VulkanCmdBuffer

class RenderCmdBuffer:public VulkanCmdBuffer
{
private:

    uint32_t cv_count;
    VkClearValue *clear_values;
    VkRect2D render_area;
    VkViewport viewport;

    VkPipelineLayout pipeline_layout;

    /*
    * 绝大部分desc绑定会全部使用这些自动绑定器绑定
    * 该数据在渲染前分别会有各自的模块设置进来
    * 比如
    *    DescriptSetType::RenderTarget  即该由RenderTarget模块设置
    *    DescriptSetType::World         预留/旧体系标记
    */

public:

    RenderCmdBuffer(const VulkanDevAttr *attr,VkCommandBuffer cb);
    ~RenderCmdBuffer();

    bool End() override
    {
        return VulkanCmdBuffer::End();
    }

    void SetRenderArea(const VkRect2D &ra){render_area=ra;}
    void SetRenderArea(const VkExtent2D &);
    void SetViewport(const VkViewport &vp){viewport=vp;}

    void SetClearColor(uint32_t index,const Color4f &cc)
    {
        if(index>=cv_count)
        {
            // clear_values 未就绪（BeginRendering 前调用）：延迟到 BeginRendering 时应用
            if(index>=16)return;
            clear_values=hgl_align_realloc<VkClearValue>(clear_values,index+1);
            cv_count=index+1;
        }

        mem_copy<float>(clear_values[index].color.float32,(const float *)&cc,4);
    }

    void SetClearDepthStencil(uint32_t index,float d=0.0f,float s=0)
    {
        if(index>=cv_count)return;

        VkClearValue *cv=clear_values+index;

        cv->depthStencil.depth=d;
        cv->depthStencil.stencil=s;
    }

    void SetLineWidth(float w)
    {
        vkCmdSetLineWidth(cmd_buf,w);
    }

    //以上设定在Begin开始后即不可改变

    bool BindDescriptorSets(ShaderProgram *);

    void BeginRendering(const VkRenderingInfo *ri)
    {
        if(!ri)return;

        vkCmdBeginRendering(cmd_buf,ri);
    }

    // Dynamic Rendering：直接从 render target 取附件构造 VkRenderingInfo
    // （替代 BeginRenderPass 的 framebuffer 路径——无 render pass/framebuffer 依赖）
    // 实现见 VKCommandBufferRender.cpp（需 IRenderTarget 完整定义，避免循环 include）
    bool BeginRendering(IRenderTarget *rt);

    void EndRendering()
    {
        vkCmdEndRendering(cmd_buf);
    }

    // EndRendering 后将 color 附件转换到 PRESENT_SRC（dynamic rendering 无自动转换）
    // ——swapchain present 要求 image 处于 PRESENT_SRC_KHR（VUID-VkPresentInfoKHR-pImageIndices-01430）
    // 实现见 VKCommandBufferRender.cpp（需 IRenderTarget 完整定义）
    void EndRenderingPresent(IRenderTarget *rt);

    bool BindPipeline(Pipeline *p)
    {
        if(!p)return(false);

        vkCmdBindPipeline(cmd_buf,VK_PIPELINE_BIND_POINT_GRAPHICS,*p);
        return(true);
    }

    bool BindDescriptorSets(VkPipelineLayout pipeline_layout,const uint32_t first_set,const VkDescriptorSet *ds_list,const uint32_t ds_count,const uint32_t *offset,const uint32_t offset_count)
    {
        if(!ds_list||ds_count<=0)return(false);

        vkCmdBindDescriptorSets(cmd_buf,VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline_layout,first_set,ds_count,ds_list,offset_count,offset);

        return(true);
    }

    void PushConstants(VkShaderStageFlagBits shader_stage_bit,uint32_t offset,uint32_t size,const void *pValues)
    {
        vkCmdPushConstants(cmd_buf,pipeline_layout,(VkShaderStageFlagBits)shader_stage_bit,offset,size,pValues);
    }

    // 显式 layout 版（pipeline_layout 成员可能未设置——owner_batch 单集绑定分支）
    void PushConstants(VkPipelineLayout layout,const void *data,const uint32_t size)
    {
        vkCmdPushConstants(cmd_buf,layout,(VkShaderStageFlagBits)ShaderStage::Vertex,0,size,data);
    }

    void PushConstants(const void *data,const uint32_t size)                        {vkCmdPushConstants(cmd_buf,pipeline_layout,(VkShaderStageFlagBits)ShaderStage::Vertex,0,       size,data);}
    void PushConstants(const void *data,const uint32_t offset,const uint32_t size)  {vkCmdPushConstants(cmd_buf,pipeline_layout,(VkShaderStageFlagBits)ShaderStage::Vertex,offset,  size,data);}

    void SetViewport        (uint32_t first,uint32_t count,const VkViewport *vp)    {vkCmdSetViewport(cmd_buf,first,count,vp);}
    void SetScissor         (uint32_t first,uint32_t count,const VkRect2D *sci)     {vkCmdSetScissor(cmd_buf,first,count,sci);}

    void SetDepthBias       (float constant_factor,float clamp,float slope_factor)  {vkCmdSetDepthBias(cmd_buf,constant_factor,clamp,slope_factor);}
    void SetDepthBounds     (float min_db,float max_db)                             {vkCmdSetDepthBounds(cmd_buf,min_db,max_db);}
    void SetBlendConstants  (const float constants[4])                              {vkCmdSetBlendConstants(cmd_buf,constants);}

    void SetStencilCompareMask  (VkStencilFaceFlags faceMask,uint32_t compareMask)  {vkCmdSetStencilCompareMask(cmd_buf,faceMask,compareMask);}
    void SetStencilWriteMask    (VkStencilFaceFlags faceMask,uint32_t compareMask)  {vkCmdSetStencilWriteMask(cmd_buf,faceMask,compareMask);}
    void SetStencilReference    (VkStencilFaceFlags faceMask,uint32_t compareMask)  {vkCmdSetStencilReference(cmd_buf,faceMask,compareMask);}

public: //draw

                                void Draw               (const uint32_t vertex_count)                               {vkCmdDraw(cmd_buf,vertex_count,1,0,0);}
                                void Draw               (const uint32_t vertex_count,const uint32_t instance_count) {vkCmdDraw(cmd_buf,vertex_count,instance_count,0,0);}

//    template<typename ...ARGS>  void Draw               (ARS...args)                   {vkCmdDraw(cmd_buf,args...);}

                                void DrawIndirect       (VkBuffer,VkDeviceSize, uint32_t drawCount,uint32_t stride=sizeof(VkDrawIndirectCommand         ));
                                void DrawIndirect       (VkBuffer buf,          uint32_t drawCount,uint32_t stride=sizeof(VkDrawIndirectCommand         )){return DrawIndirect(         buf,0,drawCount,stride);}

                                void Draw               (const GeometryDataBuffer *,const GeometryDrawRange *,const uint32_t instance_count=1,const uint32_t first_instance=0);

    // Mesh Shader（VK_EXT_mesh_shader）：以 threadgroup 网格发起绘制，无顶点输入
    // 实现见 VKCommandBufferRender.cpp——扩展函数需 vkGetDeviceProcAddr 动态加载
    void DrawMeshTasks(const uint32_t group_count_x,const uint32_t group_count_y=1,const uint32_t group_count_z=1);

public: //dynamic state

public:

};//class RenderCmdBuffer:public VulkanCmdBuffer

class TextureCmdBuffer:public VulkanCmdBuffer
{
    VkImageMemoryBarrier imageMemoryBarrier;

public:

    TextureCmdBuffer(const VulkanDevAttr *attr,VkCommandBuffer cb):VulkanCmdBuffer(attr,cb)
    {
        imageMemoryBarrier.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        imageMemoryBarrier.pNext=nullptr;
        imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    }

    template<typename ...ARGS> void PipelineBarrier     (ARGS...args){vkCmdPipelineBarrier  (cmd_buf,args...);}
    template<typename ...ARGS> void CopyBufferToImage   (ARGS...args){vkCmdCopyBufferToImage(cmd_buf,args...);}
    template<typename ...ARGS> void CopyImageToBuffer   (ARGS...args){vkCmdCopyImageToBuffer(cmd_buf,args...);}
    template<typename ...ARGS> void BlitImage           (ARGS...args){vkCmdBlitImage        (cmd_buf,args...);}

    void ImageMemoryBarrier(VkImage image,
                            VkPipelineStageFlags srcStageMask,
                            VkPipelineStageFlags dstStageMask,
                            VkAccessFlags srcAccessMask,
                            VkAccessFlags dstAccessMask,
                            VkImageLayout oldImageLayout,
                            VkImageLayout newImageLayout,
                            VkImageSubresourceRange subresourceRange)
    {
        imageMemoryBarrier.srcAccessMask = srcAccessMask;
        imageMemoryBarrier.dstAccessMask = dstAccessMask;
        imageMemoryBarrier.oldLayout = oldImageLayout;
        imageMemoryBarrier.newLayout = newImageLayout;
        imageMemoryBarrier.image = image;
        imageMemoryBarrier.subresourceRange = subresourceRange;

        vkCmdPipelineBarrier(   cmd_buf,
                                srcStageMask,
                                dstStageMask,
                                0,
                                0, nullptr,
                                0, nullptr,
                                1, &imageMemoryBarrier);
    }
};//class TextureCmdBuffer:public VulkanCmdBuffer
}//namespace hgl::graph
