#include<hgl/graph/VKDebugUtils.h>

VK_NAMESPACE_BEGIN
DebugUtils *CreateDebugUtils(VkDevice device)
{
    DebugUtilsFunction duf;

#define DUF_GETFUNC(n,N)    duf.n=(PFN_vk##N##EXT)vkGetDeviceProcAddr(device,"vk"#N"EXT");if(!duf.##n)return(nullptr);

    DUF_GETFUNC(SetName,    SetDebugUtilsObjectName     );
    DUF_GETFUNC(SetTag,     SetDebugUtilsObjectTag      );
    
    DUF_GETFUNC(QueueBegin, QueueBeginDebugUtilsLabel   );
    DUF_GETFUNC(QueueEnd,   QueueEndDebugUtilsLabel     );
    DUF_GETFUNC(QueueInsert,QueueInsertDebugUtilsLabel  );

    DUF_GETFUNC(CmdBegin,   CmdBeginDebugUtilsLabel     );
    DUF_GETFUNC(CmdEnd,     CmdEndDebugUtilsLabel       );
    DUF_GETFUNC(CmdInsert,  CmdInsertDebugUtilsLabel    );

    return(new DebugUtils(device,duf));
}

void DebugUtils::SetName(VkObjectType type,uint64_t handle,const char *name)
{
    VkDebugUtilsObjectNameInfoEXT name_info={};

    name_info.sType         =VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    name_info.objectType    =type;
    name_info.objectHandle  =handle;
    name_info.pObjectName   =name;

    duf.SetName(device,&name_info);
}

struct DebugUtilsLabel:public VkDebugUtilsLabelEXT
{
    DebugUtilsLabel(const char *n,const Color4f &c)
    {
        sType=VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
        pNext=nullptr;
        pLabelName=n;
        color[0]=c.r;
        color[1]=c.g;
        color[2]=c.b;
        color[3]=c.a;
    }
};//struct DebugUtilsLabel

void DebugUtils::QueueBegin(VkQueue q,const char *name,const Color4f &color)
{
    DebugUtilsLabel label(name,color);

    duf.QueueBegin(q,&label);
}

void DebugUtils::QueueInsert(VkQueue q,const char *name,const Color4f &color)
{
    DebugUtilsLabel label(name,color);

    duf.QueueInsert(q,&label);
}

void DebugUtils::CmdBegin(VkCommandBuffer cmd_buf,const char *name,const Color4f &color)
{
    DebugUtilsLabel label(name,color);

    duf.CmdBegin(cmd_buf,&label);
}

void DebugUtils::CmdInsert(VkCommandBuffer cmd_buf,const char *name,const Color4f &color)
{
    DebugUtilsLabel label(name,color);

    duf.CmdInsert(cmd_buf,&label);
}
VK_NAMESPACE_END
