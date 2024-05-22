#include<hgl/graph/VKVertexInputLayout.h>
#include<hgl/graph/VKVertexAttribBuffer.h>
#include<hgl/graph/VKIndexBuffer.h>

VK_NAMESPACE_BEGIN
struct PrimitiveData
{
    const VIL *     vil;

    VkDeviceSize    vertex_count;
    
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

    VABAccess       vab_access[HGL_MAX_VERTEX_ATTRIB_COUNT];
    IBAccess        ib_access;
};//struct PrimitiveData

PrimitiveData *CreatePrimitiveData(const VIL *_vil,const VkDeviceSize vc)
{
    if(!_vil)return(nullptr);
    if(vc<=0)return(nullptr);

    PrimitiveData *pd=hgl_zero_new<PrimitiveData>();

    pd->vil         =_vil;
    pd->vertex_count=vc;

    return(pd);
}

void Free(PrimitiveData *pd)
{
    if(!pd)return;

    delete pd;
}

const VkDeviceSize GetVertexCount(PrimitiveData *pd)
{
    if(!pd)return(0);

    return pd->vertex_count;
}

const int GetVABCount(PrimitiveData *pd)
{
    if(!pd)return(-1);
    if(!pd->vil)return(-2);

    return pd->vil->GetCount();
}

int GetVABIndex(const PrimitiveData *pd,const AnsiString &name)
{
    if(!pd)return(-1);
    if(!pd->vil)return(-1);
    if(name.IsEmpty())return(-1);

    return pd->vil->GetIndex(name);
}

VABAccess *GetVABAccess(PrimitiveData *pd,const int index)
{
    if(!pd)return(nullptr);
    if(!pd->vil)return(nullptr);
    if(index<0||index>=pd->vil->GetCount())return(nullptr);

    return pd->vab_access+index;
}

VABAccess *GetVABAccess(PrimitiveData *pd,const AnsiString &name)
{
    if(!pd)return(nullptr);
    if(!pd->vil)return(nullptr);
    if(name.IsEmpty())return(nullptr);

    const int index=pd->vil->GetIndex(name);

    if(index<0)return(nullptr);

    return pd->vab_access+index;
}

//VABAccess *SetVAB(PrimitiveData *pd,const int index,VAB *vab,VkDeviceSize start,VkDeviceSize count)
//{
//    if(!pd)return(nullptr);
//    if(!pd->vil)return(nullptr);
//    if(index<0||index>=pd->vil->GetCount())return(nullptr);
//
//    VABAccess *vaba=pd->vab_access+index;
//
//    vaba->vab=vab;
//    vaba->start=start;
//    vaba->count=count;
//
//    //#ifdef _DEBUG
//    //    DebugUtils *du=device->GetDebugUtils();
//
//    //    if(du)
//    //    {
//    //        du->SetBuffer(vab->GetBuffer(),prim_name+":VAB:Buffer:"+name);
//    //        du->SetDeviceMemory(vab->GetVkMemory(),prim_name+":VAB:Memory:"+name);
//    //    }
//    //#endif//_DEBUG
//
//    return vaba;
//}

void Destory(PrimitiveData *pd)
{
    if(!pd)return;

    VABAccess *vab=pd->vab_access;

    for(uint32_t i=0;i<pd->vil->GetCount();i++)
    {
        if(vab->vab)
        {
            delete vab->vab;
            vab->vab=nullptr;
        }

        ++vab;
    }

    if(pd->ib_access.buffer)
    {
        delete pd->ib_access.buffer;
        pd->ib_access.buffer=nullptr;
    }
}

void SetIndexBuffer(PrimitiveData *pd,IndexBuffer *ib,const VkDeviceSize ic)
{
    if(!pd)return;
    
    pd->ib_access.buffer=ib;
    pd->ib_access.start=0;
    pd->ib_access.count=ic;
}

IBAccess *GetIBAccess(PrimitiveData *pd)
{
    if(!pd)return(nullptr);

    return &(pd->ib_access);
}

VK_NAMESPACE_END