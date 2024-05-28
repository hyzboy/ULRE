#ifndef HGL_GRAPH_VULKAN_COMMAND_BUFFER_INCLUDE
#define HGL_GRAPH_VULKAN_COMMAND_BUFFER_INCLUDE

#include<hgl/graph/VK.h>
#include<hgl/graph/VKVABList.h>
#include<hgl/graph/VKPipeline.h>
#include<hgl/graph/VKDescriptorSet.h>
#include<hgl/color/Color4f.h>
VK_NAMESPACE_BEGIN
class GPUCmdBuffer
{
protected:

    const GPUDeviceAttribute *dev_attr;

    VkCommandBuffer cmd_buf;

public:

    GPUCmdBuffer(const GPUDeviceAttribute *attr,VkCommandBuffer cb);
    virtual ~GPUCmdBuffer();

    operator VkCommandBuffer(){return cmd_buf;}
    operator const VkCommandBuffer()const{return cmd_buf;}
    operator const VkCommandBuffer *()const{return &cmd_buf;}
    
    bool Begin();
    bool End(){return(vkEndCommandBuffer(cmd_buf)==VK_SUCCESS);}

#ifdef _DEBUG
    void SetDebugName(const UTF8String &);
    void BeginRegion(const UTF8String &,const Color4f &);
    void EndRegion();
#else
    void BeginRegion(const UTF8String &,const Color4f &){}
    void EndRegion(){}
#endif//_DEBUG
};//class GPUCmdBuffer

class RenderCmdBuffer:public GPUCmdBuffer
{
    uint32_t cv_count;
    VkClearValue *clear_values;
    VkRect2D render_area;
    VkViewport viewport;
   
    Framebuffer *fbo;
    RenderPassBeginInfo rp_begin;
    VkPipelineLayout pipeline_layout;

    void SetFBO(Framebuffer *);

public:

    RenderCmdBuffer(const GPUDeviceAttribute *attr,VkCommandBuffer cb);
    ~RenderCmdBuffer();

    void SetRenderArea(const VkRect2D &ra){render_area=ra;}
    void SetRenderArea(const VkExtent2D &);
    void SetViewport(const VkViewport &vp){viewport=vp;}

    void SetClearColor(uint32_t index,const Color4f &cc)
    {
        if(index>=cv_count)return;

        hgl_cpy(clear_values[index].color.float32,cc.rgba,4);
    }

    void SetClearDepthStencil(uint32_t index,float d=1.0f,float s=0)
    {
        if(index>=cv_count)return;

        VkClearValue *cv=clear_values+index;

        cv->depthStencil.depth=d;
        cv->depthStencil.stencil=s;
    }

    //以上设定在Begin开始后即不可改变

    bool BindFramebuffer(RenderPass *rp,Framebuffer *fb);

    bool BeginRenderPass();
    void NextSubpass(){vkCmdNextSubpass(cmd_buf,VK_SUBPASS_CONTENTS_INLINE);}
    void EndRenderPass(){vkCmdEndRenderPass(cmd_buf);}

    void BeginRendering(const VkRenderingInfoKHR *ri)
    {
        if(!ri)return;

        vkCmdBeginRenderingKHR(cmd_buf,ri);
    }

    void EndRendering()
    {
        vkCmdEndRenderingKHR(cmd_buf);
    }

    bool BindPipeline(Pipeline *p)
    {
        if(!p)return(false);

        vkCmdBindPipeline(cmd_buf,VK_PIPELINE_BIND_POINT_GRAPHICS,*p);
        return(true);
    }

    bool BindDescriptorSets(DescriptorSet *dsl)
    {
        if(!dsl)return(false);

        pipeline_layout=dsl->GetPipelineLayout();

        const VkDescriptorSet ds=dsl->GetDescriptorSet();

        vkCmdBindDescriptorSets(cmd_buf,VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline_layout,0,1,&ds,0,nullptr);

        return(true);
    }

    bool BindDescriptorSets(DescriptorSet *dsl,const uint32_t offset)
    {
        if(!dsl)return(false);

        pipeline_layout=dsl->GetPipelineLayout();

        const VkDescriptorSet ds=dsl->GetDescriptorSet();

        vkCmdBindDescriptorSets(cmd_buf,VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline_layout,0,1,&ds,1,&offset);

        return(true);
    }

    bool BindDescriptorSets(VkPipelineLayout pipeline_layout,const uint32_t first_set,const VkDescriptorSet *ds_list,const uint32_t ds_count,const uint32_t *offset,const uint32_t offset_count)
    {
        if(!ds_list||ds_count<=0)return(false);

        vkCmdBindDescriptorSets(cmd_buf,VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline_layout,first_set,ds_count,ds_list,offset_count,offset);

        return(true);
    }

    bool BindDescriptorSets(Material *);

    bool PushDescriptorSet(VkPipelineLayout pipeline_layout,uint32_t set,uint32_t count,const VkWriteDescriptorSet *write_desc_set)
    {
        vkCmdPushDescriptorSetKHR(cmd_buf,VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline_layout,set,count,write_desc_set);
    }

    void PushConstants(VkShaderStageFlagBits shader_stage_bit,uint32_t offset,uint32_t size,const void *pValues)
    {
        vkCmdPushConstants(cmd_buf,pipeline_layout,(VkShaderStageFlagBits)shader_stage_bit,offset,size,pValues);
    }

    void PushConstants(const void *data,const uint32_t size)                        {vkCmdPushConstants(cmd_buf,pipeline_layout,VK_SHADER_STAGE_VERTEX_BIT,0,       size,data);}
    void PushConstants(const void *data,const uint32_t offset,const uint32_t size)  {vkCmdPushConstants(cmd_buf,pipeline_layout,VK_SHADER_STAGE_VERTEX_BIT,offset,  size,data);}

    void BindVAB(const uint32_t first,const uint32_t count,const VkBuffer *vab,const VkDeviceSize *offsets)
    {
        vkCmdBindVertexBuffers(cmd_buf,first,count,vab,offsets);
    }

    bool BindVAB(VABList *vab_list)
    {
        if(!vab_list)return(false);

        if(!vab_list->IsFull())return(false);

        vkCmdBindVertexBuffers(cmd_buf,
                               0,                       //first binding
                               vab_list->vab_count,     //binding count
                               vab_list->vab_list,      //buffers
                               vab_list->vab_offset);   //buffer offsets

        return(true);
    }

    void BindIBO(IndexBuffer *,const VkDeviceSize byte_offset=0);

    bool BindRenderBuffer(const PrimitiveDataBuffer *);

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
                                void DrawIndexed        (const uint32_t index_count )                               {vkCmdDrawIndexed(cmd_buf,index_count,1,0,0,0);}
                                void Draw               (const uint32_t vertex_count,const uint32_t instance_count) {vkCmdDraw(cmd_buf,vertex_count,instance_count,0,0);}
                                void DrawIndexed        (const uint32_t index_count ,const uint32_t instance_count) {vkCmdDrawIndexed(cmd_buf,index_count,instance_count,0,0,0);}
                                void DrawIndexed        (const uint32_t index_count ,const uint32_t instance_count,const uint32_t firstIndex,const int32_t vertexOffset,const uint32_t firstInstance) 
                                {
                                    vkCmdDrawIndexed(cmd_buf,
                                                     index_count,
                                                     instance_count,
                                                     firstIndex,
                                                     vertexOffset,
                                                     firstInstance);
                                }

//    template<typename ...ARGS>  void Draw               (ARGS...args)                   {vkCmdDraw(cmd_buf,args...);}
//    template<typename ...ARGS>  void DrawIndexed        (ARGS...args)                   {vkCmdDrawIndexed(cmd_buf,args...);}

                                void DrawIndirect       (VkBuffer,VkDeviceSize, uint32_t drawCount,uint32_t stride=sizeof(VkDrawIndirectCommand         ));
                                void DrawIndexedIndirect(VkBuffer,VkDeviceSize, uint32_t drawCount,uint32_t stride=sizeof(VkDrawIndexedIndirectCommand  ));
                                void DrawIndirect       (VkBuffer buf,          uint32_t drawCount,uint32_t stride=sizeof(VkDrawIndirectCommand         )){return DrawIndirect(         buf,0,drawCount,stride);}
                                void DrawIndexedIndirect(VkBuffer buf,          uint32_t drawCount,uint32_t stride=sizeof(VkDrawIndexedIndirectCommand  )){return DrawIndexedIndirect(  buf,0,drawCount,stride);}

                                void Draw               (const PrimitiveDataBuffer *prb,const PrimitiveRenderData *prd,const uint32_t instance_count=1,const uint32_t first_instance=0);

public: //dynamic state
};//class RenderCmdBuffer:public GPUCmdBuffer

class TextureCmdBuffer:public GPUCmdBuffer
{
    VkImageMemoryBarrier imageMemoryBarrier;

public:

    TextureCmdBuffer(const GPUDeviceAttribute *attr,VkCommandBuffer cb):GPUCmdBuffer(attr,cb)
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
};//class TextureCmdBuffer:public GPUCmdBuffer
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_COMMAND_BUFFER_INCLUDE
