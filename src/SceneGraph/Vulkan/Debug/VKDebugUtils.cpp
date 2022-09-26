#include<hgl/graph/VKDebugUtils.h>

VK_NAMESPACE_BEGIN
void DebugUtils::SetName(VkObjectType type,uint64_t handle,const char *name)
{
    VkDebugUtilsObjectNameInfoEXT name_info={};

    name_info.sType         =VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    name_info.objectType    =type;
    name_info.objectHandle  =handle;
    name_info.pObjectName   =name;

    duf.SetName(device,&name_info);
}

void DebugUtils::Begin(VkCommandBuffer cmd_buf,const char *name,const Color4f &color)
{
    VkDebugUtilsLabelEXT label={};

    label.sType=VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    label.pLabelName=name;
    memcpy(label.color,&color,sizeof(float)*4);

    duf.Begin(cmd_buf,&label);
}

DebugUtils *CreateDebugUtils(VkDevice device)
{
    DebugUtilsFunction duf;

    duf.SetName =(PFN_vkSetDebugUtilsObjectNameEXT  )vkGetDeviceProcAddr(device,"vkSetDebugUtilsObjectNameEXT");
    duf.Begin   =(PFN_vkCmdBeginDebugUtilsLabelEXT  )vkGetDeviceProcAddr(device,"vkCmdBeginDebugUtilsLabelEXT");
    duf.End     =(PFN_vkCmdEndDebugUtilsLabelEXT    )vkGetDeviceProcAddr(device,"vkCmdEndDebugUtilsLabelEXT");

    if(!duf.SetName
     ||!duf.Begin
     ||!duf.End)
        return(nullptr);

    return(new DebugUtils(device,duf));
}
VK_NAMESPACE_END
