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
	
	#define DEBUG_MAKER_SET_FUNC(type,MNAME)	void Set##type##Name(Vk##type obj,const char *name){SetObjectName((uint64_t)obj,VK_DEBUG_REPORT_OBJECT_TYPE_##MNAME##_EXT,name);}

	DEBUG_MAKER_SET_FUNC(CommandBuffer,			COMMAND_BUFFER)
	DEBUG_MAKER_SET_FUNC(Queue,					QUEUE)
	DEBUG_MAKER_SET_FUNC(Image,					IMAGE)
	DEBUG_MAKER_SET_FUNC(Sampler,				SAMPLER)
	DEBUG_MAKER_SET_FUNC(Buffer,				BUFFER)
	DEBUG_MAKER_SET_FUNC(DeviceMemory,			DEVICE_MEMORY)
	DEBUG_MAKER_SET_FUNC(ShaderModule,			SHADER_MODULE)
	DEBUG_MAKER_SET_FUNC(Pipeline,				PIPELINE)
	DEBUG_MAKER_SET_FUNC(PipelineLayout,		PIPELINE_LAYOUT)
	DEBUG_MAKER_SET_FUNC(RenderPass,			RENDER_PASS)
	DEBUG_MAKER_SET_FUNC(Framebuffer,			FRAMEBUFFER)
	DEBUG_MAKER_SET_FUNC(DescriptorSetLayout,	DESCRIPTOR_SET_LAYOUT)
	DEBUG_MAKER_SET_FUNC(DescriptorSet,			DESCRIPTOR_SET)
	DEBUG_MAKER_SET_FUNC(Semaphore,				SEMAPHORE)
	DEBUG_MAKER_SET_FUNC(Fence,					FENCE)
	DEBUG_MAKER_SET_FUNC(Event,					EVENT)

	#undef DEBUG_MAKER_SET_FUNC
};//class DebugMaker
VK_NAMESPACE_END
#endif//HGL_GRAPH_VULKAN_DEBUG_MAKER_INCLUDE
