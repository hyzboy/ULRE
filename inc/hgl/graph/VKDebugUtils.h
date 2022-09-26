#ifndef HGL_GRAPH_VULKAN_DEBUG_UTILS_INCLUDE
#define HGL_GRAPH_VULKAN_DEBUG_UTILS_INCLUDE

#include<hgl/graph/VK.h>
#include<hgl/type/Color4f.h>

VK_NAMESPACE_BEGIN
struct DebugUtilsFunction
{
    PFN_vkSetDebugUtilsObjectNameEXT     SetName;
    PFN_vkCmdBeginDebugUtilsLabelEXT     Begin;
    PFN_vkCmdEndDebugUtilsLabelEXT       End;
};//struct DebugUtilsFunction    

class DebugUtils
{
    VkDevice device;

    DebugUtilsFunction duf;

private:
    
    friend DebugUtils *CreateDebugUtils(VkDevice);    

    DebugUtils(VkDevice dev,const DebugUtilsFunction &f)
    {
        device=dev;
        duf=f;
    }

public:
    ~DebugUtils()=default;

    void SetName(VkObjectType,uint64_t,const char *);

    void SetInstance            (VkInstance             inst,   const char *name){SetName(VK_OBJECT_TYPE_INSTANCE,              (uint64_t)inst,     name);}
    void SetPhysicsDevice       (VkPhysicalDevice       pd,     const char *name){SetName(VK_OBJECT_TYPE_PHYSICAL_DEVICE,       (uint64_t)pd,       name);}
    void SetDevice              (VkDevice               dev,    const char *name){SetName(VK_OBJECT_TYPE_DEVICE,                (uint64_t)dev,      name);}
    void SetQueue               (VkQueue                queue,  const char *name){SetName(VK_OBJECT_TYPE_QUEUE,                 (uint64_t)queue,    name);}
    void SetSemaphore           (VkSemaphore            sema,   const char *name){SetName(VK_OBJECT_TYPE_SEMAPHORE,             (uint64_t)sema,     name);}
    void SetCommandBuffer       (VkCommandBuffer        cb,     const char *name){SetName(VK_OBJECT_TYPE_COMMAND_BUFFER,        (uint64_t)cb,       name);}
    void SetFence               (VkFence                fence,  const char *name){SetName(VK_OBJECT_TYPE_FENCE,                 (uint64_t)fence,    name);}    
    void SetDeviceMemory        (VkDeviceMemory         mem,    const char *name){SetName(VK_OBJECT_TYPE_DEVICE_MEMORY,         (uint64_t)mem,      name);}
    void SetBuffer              (VkBuffer               buf,    const char *name){SetName(VK_OBJECT_TYPE_BUFFER,                (uint64_t)buf,      name);}
    void SetImage               (VkImage                img,    const char *name){SetName(VK_OBJECT_TYPE_IMAGE,                 (uint64_t)img,      name);}
    void SetEvent               (VkEvent                event,  const char *name){SetName(VK_OBJECT_TYPE_EVENT,                 (uint64_t)event,    name);}
    void SetQueryPool           (VkQueryPool            qpool,  const char *name){SetName(VK_OBJECT_TYPE_QUERY_POOL,            (uint64_t)qpool,    name);}
    void SetBufferView          (VkBufferView           bview,  const char *name){SetName(VK_OBJECT_TYPE_BUFFER_VIEW,           (uint64_t)bview,    name);}
    void SetImageView           (VkImageView            iview,  const char *name){SetName(VK_OBJECT_TYPE_IMAGE_VIEW,            (uint64_t)iview,    name);}
    void SetShaderModule        (VkShaderModule         smod,   const char *name){SetName(VK_OBJECT_TYPE_SHADER_MODULE,         (uint64_t)smod,     name);}
    void SetPipelineCache       (VkPipelineCache        pcache, const char *name){SetName(VK_OBJECT_TYPE_PIPELINE_CACHE,        (uint64_t)pcache,   name);}
    void SetPipelineLayout      (VkPipelineLayout       playout,const char *name){SetName(VK_OBJECT_TYPE_PIPELINE_LAYOUT,       (uint64_t)playout,  name);}
    void SetRenderPass          (VkRenderPass           rpass,  const char *name){SetName(VK_OBJECT_TYPE_RENDER_PASS,           (uint64_t)rpass,    name);}
    void SetPipeline            (VkPipeline             pipe,   const char *name){SetName(VK_OBJECT_TYPE_PIPELINE,              (uint64_t)pipe,     name);}
    void SetDescriptorSetLayout (VkDescriptorSetLayout  dsl,    const char *name){SetName(VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, (uint64_t)dsl,      name);}
    void SetSampler             (VkSampler              s,      const char *name){SetName(VK_OBJECT_TYPE_SAMPLER,               (uint64_t)s,        name);}
    void SetDescriptorPool      (VkDescriptorPool       dp,     const char *name){SetName(VK_OBJECT_TYPE_DESCRIPTOR_POOL,       (uint64_t)dp,       name);}
    void SetDescriptorSet       (VkDescriptorSet        ds,     const char *name){SetName(VK_OBJECT_TYPE_DESCRIPTOR_SET,        (uint64_t)ds,       name);}
    void SetFrameBuffer         (VkFramebuffer          fbo,    const char *name){SetName(VK_OBJECT_TYPE_FRAMEBUFFER,           (uint64_t)fbo,      name);}
    void SetCommandPool         (VkCommandPool          cp,     const char *name){SetName(VK_OBJECT_TYPE_COMMAND_POOL,          (uint64_t)cp,       name);}
    
    void Begin(VkCommandBuffer,const char *,const Color4f &color=Color4f(1,1,1,1));
    void End(VkCommandBuffer cmd_buf){duf.End(cmd_buf);}
};//class DebugUtils
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_DEBUG_UTILS_INCLUDE
