#include<hgl/graph/VKDebugMaker.h>

VK_NAMESPACE_BEGIN
void DebugMaker::SetObjectName(uint64_t object, VkDebugReportObjectTypeEXT objectType, const char *name)
{
	// Check for valid function pointer (may not be present if not running in a debugging application)
	if(!dmf.SetObjectName)return;
	
	VkDebugMarkerObjectNameInfoEXT nameInfo = {};
	nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT;
	nameInfo.objectType = objectType;
	nameInfo.object = object;
	nameInfo.pObjectName = name;

	dmf.SetObjectName(device, &nameInfo);
}

void DebugMaker::SetObjectTag(uint64_t object, VkDebugReportObjectTypeEXT objectType, uint64_t name, size_t tagSize, const void* tag)
{
	// Check for valid function pointer (may not be present if not running in a debugging application)
	if(!dmf.SetObjectTag)return;

	VkDebugMarkerObjectTagInfoEXT tagInfo = {};
	tagInfo.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_TAG_INFO_EXT;
	tagInfo.objectType = objectType;
	tagInfo.object = object;
	tagInfo.tagName = name;
	tagInfo.tagSize = tagSize;
	tagInfo.pTag = tag;

	dmf.SetObjectTag(device, &tagInfo);
}

void DebugMaker::Begin(VkCommandBuffer cmdbuffer, const char * pMarkerName, const Color4f &color)
{
	//Check for valid function pointer (may not be present if not running in a debugging application)
	if(!dmf.Begin)return;
	
	VkDebugMarkerMarkerInfoEXT markerInfo = {};
	markerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT;
	memcpy(markerInfo.stop_color, &color, sizeof(float) * 4);
	markerInfo.pMarkerName = pMarkerName;
	
	dmf.Begin(cmdbuffer, &markerInfo);
}

void DebugMaker::Insert(VkCommandBuffer cmdbuffer, const char *markerName, const Color4f &color)
{
	// Check for valid function pointer (may not be present if not running in a debugging application)
	if(!dmf.Insert)return;
	
	VkDebugMarkerMarkerInfoEXT markerInfo = {};
	markerInfo.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT;
	memcpy(markerInfo.stop_color, &color, sizeof(float) * 4);
	markerInfo.pMarkerName = markerName;

	dmf.Insert(cmdbuffer, &markerInfo);
}

void DebugMaker::End(VkCommandBuffer cmdBuffer)
{
	// Check for valid function (may not be present if not running in a debugging application)
	if(!dmf.End)return;

	dmf.End(cmdBuffer);
}

DebugMaker *CreateDebugMaker(VkDevice device)
{
	DebugMakerFunction dmf;

	dmf.SetObjectTag	= reinterpret_cast<PFN_vkDebugMarkerSetObjectTagEXT	>(vkGetDeviceProcAddr(device, "vkDebugMarkerSetObjectTagEXT"));
	dmf.SetObjectName	= reinterpret_cast<PFN_vkDebugMarkerSetObjectNameEXT>(vkGetDeviceProcAddr(device, "vkDebugMarkerSetObjectNameEXT"));
	dmf.Begin			= reinterpret_cast<PFN_vkCmdDebugMarkerBeginEXT		>(vkGetDeviceProcAddr(device, "vkCmdDebugMarkerBeginEXT"));
	dmf.End				= reinterpret_cast<PFN_vkCmdDebugMarkerEndEXT		>(vkGetDeviceProcAddr(device, "vkCmdDebugMarkerEndEXT"));
	dmf.Insert			= reinterpret_cast<PFN_vkCmdDebugMarkerInsertEXT	>(vkGetDeviceProcAddr(device, "vkCmdDebugMarkerInsertEXT"));

	if(!dmf.SetObjectTag	)return(nullptr);
	if(!dmf.SetObjectName	)return(nullptr);
	if(!dmf.Begin			)return(nullptr);
	if(!dmf.End				)return(nullptr);
	if(!dmf.Insert			)return(nullptr);

	return(new DebugMaker(device,dmf));
}
VK_NAMESPACE_END