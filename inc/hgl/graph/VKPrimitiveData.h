#pragma once

#include<hgl/graph/VKVertexInputLayout.h>
#include<hgl/graph/VKVertexAttribBuffer.h>
#include<hgl/graph/VKIndexBuffer.h>
#include<hgl/graph/AABB.h>

VK_NAMESPACE_BEGIN

/*
    1.截止2024.4.27，根据vulkan.gpuinfo.org统计，只有9%的设备maxVertexInputAttributes为16，不存在低于16的设备。
         9.0%的设备为28 - 31
        70.7%的设备为32
         9.6%的设备为64

        由于我们暂时没有发现需要使用16个以上属性的情况，所以这里暂定使用16。
        (如果时间过去久远，可再次查询此值是否可改成更高的值，以及是否需要)

    2.为何va_name使用char[][]而不是String以及动态分配内存？

        就是为了必避动态分配内存，以及可以直接memcpy处理，所以此处这样定义。
*/

constexpr const uint HGL_MAX_VERTEX_ATTRIB_COUNT=16;        ///<最大顶点属性数量

struct PrimitiveData
{
    const VIL *     vil;

    VkDeviceSize    vertex_count;

    VABAccess       vab_access[HGL_MAX_VERTEX_ATTRIB_COUNT];
    IBAccess        ib_access;
};

inline bool Init(PrimitiveData *pd,const VIL *_vil,const VkDeviceSize vc,const VkDeviceSize ic=0)
{
    if(!pd)return(false);
    if(!_vil)return(false);
    if(vc<=0)return(false);

    hgl_zero(*pd);

    pd->vil=_vil;
    pd->vertex_count=vc;
    pd->ib_access.count=ic;

    return(true);
}

inline int GetVABIndex(const PrimitiveData *pd,const AnsiString &name)
{
    if(!pd)return(-1);
    if(!pd->vil)return(-1);
    if(name.IsEmpty())return(-1);

    return pd->vil->GetIndex(name);
}

inline const VABAccess *GetVAB(const PrimitiveData *pd,const AnsiString &name)
{
    if(!pd)return(nullptr);    
    if(name.IsEmpty())return(nullptr);

    const int index=GetVABIndex(pd,name);

    if(index==-1)
        return(nullptr);

    return pd->vab_access+index;
}

inline VABAccess *SetVAB(PrimitiveData *pd,const AnsiString &name,VAB *vab,VkDeviceSize start=0)
{
    if(!pd)return(nullptr);
    if(name.IsEmpty())return(nullptr);

    const int index=GetVABIndex(pd,name);

    if(index==-1)
        return(nullptr);

    VABAccess *vaba=pd->vab_access+index;

    vaba->vab=vab;
    vaba->start=start;

    //#ifdef _DEBUG
    //    DebugUtils *du=device->GetDebugUtils();

    //    if(du)
    //    {
    //        du->SetBuffer(vab->GetBuffer(),prim_name+":VAB:Buffer:"+name);
    //        du->SetDeviceMemory(vab->GetVkMemory(),prim_name+":VAB:Memory:"+name);
    //    }
    //#endif//_DEBUG

    return vaba;
}

VK_NAMESPACE_END
