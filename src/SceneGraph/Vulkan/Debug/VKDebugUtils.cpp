#include<hgl/graph/VKDebugUtils.h>

VK_NAMESPACE_BEGIN
VKDebugUtils::VKDebugUtils(VkDevice dev)
{
    device=dev;

    s_vkSetDebugUtilsObjectName=(PFN_vkSetDebugUtilsObjectNameEXT)vkGetDeviceProcAddr(device,"vkSetDebugUtilsObjectNameEXT");
    s_vkCmdBeginDebugUtilsLabel=(PFN_vkCmdBeginDebugUtilsLabelEXT)vkGetDeviceProcAddr(device,"vkCmdBeginDebugUtilsLabelEXT");
    s_vkCmdEndDebugUtilsLabel=(PFN_vkCmdEndDebugUtilsLabelEXT)vkGetDeviceProcAddr(device,"vkCmdEndDebugUtilsLabelEXT");
}

void VKDebugUtils::SetName(VkObjectType type,uint64_t handle,const char *name)
{
    if(!s_vkSetDebugUtilsObjectName)return;

    VkDebugUtilsObjectNameInfoEXT name_info={};

    name_info.sType=VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    name_info.objectType=type;
    name_info.objectHandle=handle;
    name_info.pObjectName=name;

    s_vkSetDebugUtilsObjectName(device,&name_info);
}

void VKDebugUtils::Begin(VkCommandBuffer cmd_buf,const char *name,const Color4f &color)
{
    if(!s_vkCmdBeginDebugUtilsLabel)return;

    VkDebugUtilsLabelEXT label={};

    label.sType=VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    label.pLabelName=name;
    memcpy(label.color,&color,sizeof(float)*4);

    s_vkCmdBeginDebugUtilsLabel(cmd_buf,&label);
}
VK_NAMESPACE_END
