#pragma once

#include<hgl/vk/VK.h>
#include<hgl/vk/VKVABList.h>
#include<hgl/vk/pipeline/VKPipeline.h>
#include<hgl/vk/pipeline/VKComputePipeline.h>
#include<hgl/graph/mesh/Primitive.h>
#include<hgl/color/Color4f.h>
#include<hgl/type/MemoryUtil.h>
#include<hgl/mtl/PipelineConfig.h>
#include<hgl/log/Log.h>
namespace hgl::graph{
class VulkanCmdBuffer
{
public:

    OBJECT_LOGGER

protected:

    const VulkanDevAttr *dev_attr;

    VkCommandPool cmd_pool;

    VkCommandBuffer cmd_buf;

    bool cmd_begin;

public:

    VulkanCmdBuffer(const VulkanDevAttr *attr,VkCommandBuffer cb,VkCommandPool pool=VK_NULL_HANDLE);
    virtual ~VulkanCmdBuffer();

    operator VkCommandBuffer(){return cmd_buf;}
    operator const VkCommandBuffer()const{return cmd_buf;}
    operator const VkCommandBuffer *()const{return &cmd_buf;}

    const bool IsBegin()const{return cmd_begin;}

    bool scene_sets_bound=false; ///< 本 cmd 已绑全局 Scene/Bindless 集（GraphicsContext::BindGlobalDescriptorSets 守卫，每次 Begin 复位）

    virtual bool Begin();
    virtual bool End()
    {
        if(!cmd_begin)
            return(false);

        cmd_begin=false;

        return(vkEndCommandBuffer(cmd_buf)==VK_SUCCESS);
    }

    void PipelineBarrier2(const VkDependencyInfo *dep_info);
    void PipelineBarrier2(const VkDependencyInfo &dep_info);

    void MemoryBarrier2(VkPipelineStageFlags2 src_stage,
                        VkPipelineStageFlags2 dst_stage,
                        VkAccessFlags2 src_access,
                        VkAccessFlags2 dst_access);

    void BufferMemoryBarrier2(VkBuffer buffer,
                              VkPipelineStageFlags2 srcStageMask,
                              VkPipelineStageFlags2 dstStageMask,
                              VkAccessFlags2 srcAccessMask,
                              VkAccessFlags2 dstAccessMask,
                              VkDeviceSize offset=0,
                              VkDeviceSize size=VK_WHOLE_SIZE);

    void ImageMemoryBarrier2(VkImage image,
                             VkPipelineStageFlags2 srcStageMask,
                             VkPipelineStageFlags2 dstStageMask,
                             VkAccessFlags2 srcAccessMask,
                             VkAccessFlags2 dstAccessMask,
                             VkImageLayout oldImageLayout,
                             VkImageLayout newImageLayout,
                             const VkImageSubresourceRange &subresourceRange);

    void BufferMemoryBarrier(VkBuffer buffer,
                             VkPipelineStageFlags2 srcStageMask,
                             VkPipelineStageFlags2 dstStageMask,
                             VkAccessFlags2 srcAccessMask,
                             VkAccessFlags2 dstAccessMask,
                             VkDeviceSize offset=0,
                             VkDeviceSize size=VK_WHOLE_SIZE)
    {
        BufferMemoryBarrier2(buffer, srcStageMask, dstStageMask, srcAccessMask, dstAccessMask, offset, size);
    }

    void ImageMemoryBarrier(VkImage image,
                            VkPipelineStageFlags2 srcStageMask,
                            VkPipelineStageFlags2 dstStageMask,
                            VkAccessFlags2 srcAccessMask,
                            VkAccessFlags2 dstAccessMask,
                            VkImageLayout oldImageLayout,
                            VkImageLayout newImageLayout,
                            const VkImageSubresourceRange &subresourceRange)
    {
        ImageMemoryBarrier2(image, srcStageMask, dstStageMask, srcAccessMask, dstAccessMask, oldImageLayout, newImageLayout, subresourceRange);
    }

    void FillBuffer(VkBuffer buffer, VkDeviceSize offset, VkDeviceSize size, uint32_t data)
    {
        vkCmdFillBuffer(cmd_buf, buffer, offset, size, data);
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

struct RenderPassOptions
{
    bool load_color = false;
    bool load_depth = false;
    VkImageLayout depth_old_layout = VK_IMAGE_LAYOUT_UNDEFINED;
    bool use_scissor = false;
    VkRect2D scissor{};
    bool clear_scissor_depth = false;
    float clear_depth_value = 0.0f; // Reversed-Z 默认（0.0f = 远平面）
};

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
    const VkViewport &GetViewport()const{return viewport;}

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

    void BeginRendering(const VkRenderingInfo *ri)
    {
        if(!ri)return;

        vkCmdBeginRendering(cmd_buf,ri);
    }

    using RenderPassOptions = hgl::graph::RenderPassOptions;

    // Dynamic Rendering：直接从 render target 取附件构造 VkRenderingInfo
    // （替代 BeginRenderPass 的 framebuffer 路径——无 render pass/framebuffer 依赖）
    // 实现见 VKCommandBufferRender.cpp（需 IRenderTarget 完整定义，避免循环 include）
    bool BeginRendering(IRenderTarget *rt, const RenderPassOptions *options = nullptr);

    // 局部深度区域清空（使用 vkCmdClearAttachments，必须在 BeginRendering 内部调用）
    void ClearDepthRect(const VkRect2D &rect, float depth = 0.0f);

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
        vkCmdPushConstants(cmd_buf,layout,(VkShaderStageFlags)(hgl::graph::kMeshFragment | VK_SHADER_STAGE_COMPUTE_BIT),0,size,data);
    }

    void PushConstants(const void *data,const uint32_t size)                        {vkCmdPushConstants(cmd_buf,pipeline_layout,(VkShaderStageFlags)(hgl::graph::kMeshFragment | VK_SHADER_STAGE_COMPUTE_BIT),0,       size,data);}
    void PushConstants(const void *data,const uint32_t offset,const uint32_t size)  {vkCmdPushConstants(cmd_buf,pipeline_layout,(VkShaderStageFlags)(hgl::graph::kMeshFragment | VK_SHADER_STAGE_COMPUTE_BIT),offset,  size,data);}

    void SetViewport        (uint32_t first,uint32_t count,const VkViewport *vp)    {vkCmdSetViewport(cmd_buf,first,count,vp);}
    void SetScissor         (uint32_t first,uint32_t count,const VkRect2D *sci)     {vkCmdSetScissor(cmd_buf,first,count,sci);}

    void SetDepthBias       (float constant_factor,float clamp,float slope_factor)  {vkCmdSetDepthBias(cmd_buf,constant_factor,clamp,slope_factor);}
    void SetDepthBounds     (float min_db,float max_db)                             {vkCmdSetDepthBounds(cmd_buf,min_db,max_db);}
    void SetBlendConstants  (const float constants[4])                              {vkCmdSetBlendConstants(cmd_buf,constants);}

    void SetStencilCompareMask  (VkStencilFaceFlags faceMask,uint32_t compareMask)  {vkCmdSetStencilCompareMask(cmd_buf,faceMask,compareMask);}
    void SetStencilWriteMask    (VkStencilFaceFlags faceMask,uint32_t compareMask)  {vkCmdSetStencilWriteMask(cmd_buf,faceMask,compareMask);}
    void SetStencilReference    (VkStencilFaceFlags faceMask,uint32_t compareMask)  {vkCmdSetStencilReference(cmd_buf,faceMask,compareMask);}

public: //draw

    // Mesh Shader（VK_EXT_mesh_shader）：以 threadgroup 网格发起绘制，无顶点输入
    // 实现见 VKCommandBufferRender.cpp——扩展函数需 vkGetDeviceProcAddr 动态加载
    void DrawMeshTasks(const uint32_t group_count_x,const uint32_t group_count_y=1,const uint32_t group_count_z=1);

    // Mesh Shader 间接绘制（multi-draw 合批）：不支持 multiDrawIndirect 时逐条退化
    void DrawMeshTasksIndirect(VkBuffer,VkDeviceSize offset,uint32_t drawCount,uint32_t stride=sizeof(VkDrawMeshTasksIndirectCommandEXT));
    void DrawMeshTasksIndirect(VkBuffer buf,uint32_t drawCount,uint32_t stride=sizeof(VkDrawMeshTasksIndirectCommandEXT)){return DrawMeshTasksIndirect(buf,0,drawCount,stride);}

    // Mesh Shader 间接计数绘制（VK_EXT_mesh_shader + drawIndirectCount）：GPU 写入 count 驱动动态绘制数
    void DrawMeshTasksIndirectCount(VkBuffer buffer,VkDeviceSize offset,VkBuffer countBuffer,VkDeviceSize countBufferOffset,uint32_t maxDrawCount,uint32_t stride=sizeof(VkDrawMeshTasksIndirectCommandEXT));
    void DrawMeshTasksIndirectCount(VkBuffer buffer,VkBuffer countBuffer,uint32_t maxDrawCount,uint32_t stride=sizeof(VkDrawMeshTasksIndirectCommandEXT)){return DrawMeshTasksIndirectCount(buffer,0,countBuffer,0,maxDrawCount,stride);}

public: //dynamic state

    // EDS 1/2/3 全动态状态：材质配置渲染侧应用（pipeline 只保留 shader 部分）。
    // 实现见 VKCommandBufferRender.cpp
    void ApplyPipelineState(const mtl::MaterialPipelineConfig &config);

public:

};//class RenderCmdBuffer:public VulkanCmdBuffer

/**
 * Compute 命令缓冲：vkCmdDispatch / vkCmdDispatchIndirect 及 COMPUTE bind point
 * 的管线/描述符/push constant 绑定。提交走通用 DeviceQueue::Submit(VulkanCmdBuffer*)。
 * 由 VulkanDevice::CreateComputeCommandBuffer 创建，复用 graphics family cmd_pool
 * （Vulkan 保证 graphics queue 支持 compute）。
 */
class ComputeCmdBuffer:public VulkanCmdBuffer
{
public:

    ComputeCmdBuffer(const VulkanDevAttr *attr,VkCommandBuffer cb):VulkanCmdBuffer(attr,cb)
    {
    }

    bool BindPipeline(ComputePipeline *p)
    {
        if(!p)return(false);

        vkCmdBindPipeline(cmd_buf,VK_PIPELINE_BIND_POINT_COMPUTE,*p);
        return(true);
    }

    bool BindDescriptorSets(VkPipelineLayout pipeline_layout,const uint32_t first_set,const VkDescriptorSet *ds_list,const uint32_t ds_count,const uint32_t *offset=nullptr,const uint32_t offset_count=0)
    {
        if(!ds_list||ds_count<=0)return(false);

        vkCmdBindDescriptorSets(cmd_buf,VK_PIPELINE_BIND_POINT_COMPUTE,pipeline_layout,first_set,ds_count,ds_list,offset_count,offset);

        return(true);
    }

    void PushConstants(VkPipelineLayout pipeline_layout,const void *data,const uint32_t size)
    {
        vkCmdPushConstants(cmd_buf,pipeline_layout,VK_SHADER_STAGE_COMPUTE_BIT,0,size,data);
    }

    void PushConstants(VkPipelineLayout pipeline_layout,const void *data,const uint32_t offset,const uint32_t size)
    {
        vkCmdPushConstants(cmd_buf,pipeline_layout,VK_SHADER_STAGE_COMPUTE_BIT,offset,size,data);
    }

public: //dispatch

    void Dispatch(const uint32_t group_count_x,const uint32_t group_count_y=1,const uint32_t group_count_z=1)
    {
        vkCmdDispatch(cmd_buf,group_count_x,group_count_y,group_count_z);
    }

    // buffer 须带 VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT（IndirectDispatchBuffer::GetVkBuffer 可取）
    void DispatchIndirect(VkBuffer buf,VkDeviceSize offset=0)
    {
        vkCmdDispatchIndirect(cmd_buf,buf,offset);
    }
};//class ComputeCmdBuffer:public VulkanCmdBuffer

class TextureCmdBuffer:public VulkanCmdBuffer
{
public:

    TextureCmdBuffer(const VulkanDevAttr *attr,VkCommandBuffer cb,VkCommandPool pool=VK_NULL_HANDLE):VulkanCmdBuffer(attr,cb,pool)
    {
    }

    template<typename ...ARGS> void CopyBufferToImage   (ARGS...args){vkCmdCopyBufferToImage(cmd_buf,args...);}
    template<typename ...ARGS> void CopyImageToBuffer   (ARGS...args){vkCmdCopyImageToBuffer(cmd_buf,args...);}
    template<typename ...ARGS> void BlitImage           (ARGS...args){vkCmdBlitImage        (cmd_buf,args...);}
};//class TextureCmdBuffer:public VulkanCmdBuffer
}//namespace hgl::graph
