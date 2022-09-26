#ifndef HGL_GRAPH_VULKAN_DEBUG_MAKER_INCLUDE
#define HGL_GRAPH_VULKAN_DEBUG_MAKER_INCLUDE

#include<hgl/graph/VK.h>
#include<hgl/type/Color4f.h>

VK_NAMESPACE_BEGIN
struct DebugMakerFunction
{
    PFN_vkDebugMarkerSetObjectTagEXT	SetObjectTag;
    PFN_vkDebugMarkerSetObjectNameEXT	SetObjectName;
    PFN_vkCmdDebugMarkerBeginEXT		Begin;
    PFN_vkCmdDebugMarkerEndEXT			End;
    PFN_vkCmdDebugMarkerInsertEXT		Insert;
};//struct DebugMakerFunction

class DebugMaker
{
    VkDevice device;
    
    DebugMakerFunction dmf;

protected:

    void SetObjectName(uint64_t object, VkDebugReportObjectTypeEXT objectType, const char *name);
    void SetObjectTag(uint64_t object, VkDebugReportObjectTypeEXT objectType, uint64_t name, size_t tagSize, const void* tag);

private:

    friend DebugMaker *CreateDebugMaker(VkDevice device);

    DebugMaker(VkDevice dev,const DebugMakerFunction &f)
    {
        device=dev;
        dmf=f;
    }

public:
    
    void Begin(VkCommandBuffer cmdbuffer, const char * pMarkerName, const Color4f &color);
    void Insert(VkCommandBuffer cmdbuffer, const char *markerName, const Color4f &color);
    void End(VkCommandBuffer cmdBuffer);
    
    #define DEBUG_MAKER_SET_FUNC(type,MNAME)	void Set##type(Vk##type obj,const char *name){SetObjectName((uint64_t)obj,VK_DEBUG_REPORT_OBJECT_TYPE_##MNAME##_EXT,name);}

    DEBUG_MAKER_SET_FUNC(Instance,                  INSTANCE)
    DEBUG_MAKER_SET_FUNC(PhysicalDevice,            PHYSICAL_DEVICE)
    DEBUG_MAKER_SET_FUNC(Device,                    DEVICE)
    DEBUG_MAKER_SET_FUNC(Queue,					    QUEUE)
    DEBUG_MAKER_SET_FUNC(Semaphore,				    SEMAPHORE)
    DEBUG_MAKER_SET_FUNC(CommandBuffer,			    COMMAND_BUFFER)
    DEBUG_MAKER_SET_FUNC(Fence,					    FENCE)
    DEBUG_MAKER_SET_FUNC(DeviceMemory,			    DEVICE_MEMORY)
    DEBUG_MAKER_SET_FUNC(Buffer,				    BUFFER)
    DEBUG_MAKER_SET_FUNC(Image,					    IMAGE)
    DEBUG_MAKER_SET_FUNC(Event,					    EVENT)
    DEBUG_MAKER_SET_FUNC(QueryPool,				    QUERY_POOL)
    DEBUG_MAKER_SET_FUNC(BufferView,			    BUFFER_VIEW)
    DEBUG_MAKER_SET_FUNC(ShaderModule,			    SHADER_MODULE)
    DEBUG_MAKER_SET_FUNC(PipelineCache,		        PIPELINE_CACHE)
    DEBUG_MAKER_SET_FUNC(PipelineLayout,		    PIPELINE_LAYOUT)
    DEBUG_MAKER_SET_FUNC(RenderPass,			    RENDER_PASS)
    DEBUG_MAKER_SET_FUNC(Pipeline,				    PIPELINE)
    DEBUG_MAKER_SET_FUNC(DescriptorSetLayout,	    DESCRIPTOR_SET_LAYOUT)        
    DEBUG_MAKER_SET_FUNC(Sampler,				    SAMPLER)
    DEBUG_MAKER_SET_FUNC(DescriptorPool,		    DESCRIPTOR_POOL)
    DEBUG_MAKER_SET_FUNC(DescriptorSet,			    DESCRIPTOR_SET)
    DEBUG_MAKER_SET_FUNC(Framebuffer,			    FRAMEBUFFER)
    DEBUG_MAKER_SET_FUNC(CommandPool,               COMMAND_POOL)
    DEBUG_MAKER_SET_FUNC(SurfaceKHR,                SURFACE_KHR)
    DEBUG_MAKER_SET_FUNC(SwapchainKHR,              SWAPCHAIN_KHR)
    DEBUG_MAKER_SET_FUNC(DebugReportCallbackEXT,    DEBUG_REPORT_CALLBACK_EXT)
    DEBUG_MAKER_SET_FUNC(DisplayKHR,                DISPLAY_KHR)
    DEBUG_MAKER_SET_FUNC(DisplayModeKHR,            DISPLAY_MODE_KHR)
    DEBUG_MAKER_SET_FUNC(ValidationCacheEXT,        VALIDATION_CACHE_EXT)
    DEBUG_MAKER_SET_FUNC(SamplerYcbcrConversion,    SAMPLER_YCBCR_CONVERSION)
    DEBUG_MAKER_SET_FUNC(DescriptorUpdateTemplate,  DESCRIPTOR_UPDATE_TEMPLATE)
    DEBUG_MAKER_SET_FUNC(CuModuleNVX,               CU_MODULE_NVX)
    DEBUG_MAKER_SET_FUNC(CuFunctionNVX,             CU_FUNCTION_NVX)
    DEBUG_MAKER_SET_FUNC(AccelerationStructureKHR,  ACCELERATION_STRUCTURE_KHR)
    DEBUG_MAKER_SET_FUNC(AccelerationStructureNV,   ACCELERATION_STRUCTURE_NV)
        
    #undef DEBUG_MAKER_SET_FUNC
};//class DebugMaker
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_DEBUG_MAKER_INCLUDE
