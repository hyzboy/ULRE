#ifndef HGL_GRAPH_VULKAN_DEBUG_UTILS_INCLUDE
#define HGL_GRAPH_VULKAN_DEBUG_UTILS_INCLUDE

#include<hgl/graph/VK.h>
#include<hgl/type/Color4f.h>

VK_NAMESPACE_BEGIN
struct DebugUtilsFunction
{
    PFN_vkSetDebugUtilsObjectNameEXT    SetName;
    PFN_vkSetDebugUtilsObjectTagEXT     SetTag;

    PFN_vkQueueBeginDebugUtilsLabelEXT  QueueBegin;
    PFN_vkQueueEndDebugUtilsLabelEXT    QueueEnd;
    PFN_vkQueueInsertDebugUtilsLabelEXT QueueInsert;
    
    PFN_vkCmdBeginDebugUtilsLabelEXT    CmdBegin;
    PFN_vkCmdEndDebugUtilsLabelEXT      CmdEnd;    
    PFN_vkCmdInsertDebugUtilsLabelEXT   CmdInsert;
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

#define DU_FUNC(n,N)    void Set##n(Vk##n obj,const char *name){SetName(VK_OBJECT_TYPE_##N,(uint64_t)obj,name);}

    DU_FUNC(Instance            ,INSTANCE              )
    DU_FUNC(PhysicalDevice      ,PHYSICAL_DEVICE       )
    DU_FUNC(Device              ,DEVICE                )
    DU_FUNC(Queue               ,QUEUE                 )
    DU_FUNC(Semaphore           ,SEMAPHORE             )
    DU_FUNC(CommandBuffer       ,COMMAND_BUFFER        )
    DU_FUNC(Fence               ,FENCE                 )
    DU_FUNC(DeviceMemory        ,DEVICE_MEMORY         )
    DU_FUNC(Buffer              ,BUFFER                )
    DU_FUNC(Image               ,IMAGE                 )
    DU_FUNC(Event               ,EVENT                 )
    DU_FUNC(QueryPool           ,QUERY_POOL            )
    DU_FUNC(BufferView          ,BUFFER_VIEW           )
    DU_FUNC(ImageView           ,IMAGE_VIEW            )
    DU_FUNC(ShaderModule        ,SHADER_MODULE         )
    DU_FUNC(PipelineCache       ,PIPELINE_CACHE        )
    DU_FUNC(PipelineLayout      ,PIPELINE_LAYOUT       )
    DU_FUNC(RenderPass          ,RENDER_PASS           )
    DU_FUNC(Pipeline            ,PIPELINE              )
    DU_FUNC(DescriptorSetLayout ,DESCRIPTOR_SET_LAYOUT )
    DU_FUNC(Sampler             ,SAMPLER               )
    DU_FUNC(DescriptorPool      ,DESCRIPTOR_POOL       )
    DU_FUNC(DescriptorSet       ,DESCRIPTOR_SET        )
    DU_FUNC(Framebuffer         ,FRAMEBUFFER           )
    DU_FUNC(CommandPool         ,COMMAND_POOL          )
        
    DU_FUNC(SamplerYcbcrConversion,     SAMPLER_YCBCR_CONVERSION)
    DU_FUNC(DescriptorUpdateTemplate,   DESCRIPTOR_UPDATE_TEMPLATE)
    DU_FUNC(PrivateDataSlot,            PRIVATE_DATA_SLOT)
    DU_FUNC(SurfaceKHR,                 SURFACE_KHR)
    DU_FUNC(SwapchainKHR,               SWAPCHAIN_KHR)
    DU_FUNC(DisplayKHR,                 DISPLAY_KHR)
    DU_FUNC(DisplayModeKHR,             DISPLAY_MODE_KHR)
    DU_FUNC(DebugReportCallbackEXT,     DEBUG_REPORT_CALLBACK_EXT)
#ifdef VK_ENABLE_BETA_EXTENSIONS
    DU_FUNC(VideoSessionKHR,            VIDEO_SESSION_KHR)
    DU_FUNC(VideoSessionParametersKHR,  VIDEO_SESSION_PARAMETERS_KH)
#endif//VK_ENABLE_BETA_EXTENSIONS
    DU_FUNC(CuModuleNVX,                    CU_MODULE_NVX)
    DU_FUNC(CuFunctionNVX,                  CU_FUNCTION_NVX)
    DU_FUNC(DebugUtilsMessengerEXT,         DEBUG_UTILS_MESSENGER_EXT)
    DU_FUNC(AccelerationStructureKHR,       ACCELERATION_STRUCTURE_KHR)
    DU_FUNC(ValidationCacheEXT,             VALIDATION_CACHE_EXT)
    DU_FUNC(AccelerationStructureNV,        ACCELERATION_STRUCTURE_NV)
    DU_FUNC(PerformanceConfigurationINTEL,  PERFORMANCE_CONFIGURATION_INTEL)
    DU_FUNC(DeferredOperationKHR,           DEFERRED_OPERATION_KHR)
    DU_FUNC(IndirectCommandsLayoutNV,       INDIRECT_COMMANDS_LAYOUT_NV)
//    DU_FUNC(BufferCollectionFuchsia,        BUFFER_COLLECTION_FUCHSIA)

#undef DU_FUNC
    
    void QueueBegin  (VkQueue,const char *,const Color4f &color=Color4f(1,1,1,1));
    void QueueEnd    (VkQueue q){duf.QueueEnd(q);}
    void QueueInsert (VkQueue q,const char *,const Color4f &color=Color4f(1,1,1,1));
    
    void CmdBegin    (VkCommandBuffer,const char *,const Color4f &color=Color4f(1,1,1,1));
    void CmdEnd      (VkCommandBuffer cmd_buf){duf.CmdEnd(cmd_buf);}
    void CmdInsert   (VkCommandBuffer cmd_buf,const char *,const Color4f &color=Color4f(1,1,1,1));
};//class DebugUtils
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_DEBUG_UTILS_INCLUDE
