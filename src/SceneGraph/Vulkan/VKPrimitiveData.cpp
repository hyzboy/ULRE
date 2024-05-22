#include<hgl/graph/VKVertexInputLayout.h>
#include<hgl/graph/VKVertexAttribBuffer.h>
#include<hgl/graph/VKIndexBuffer.h>

VK_NAMESPACE_BEGIN
struct PrimitiveData
{
    const VIL *     vil;

    VkDeviceSize    vertex_count;
    
/*
    1.��ֹ2024.4.27������vulkan.gpuinfo.orgͳ�ƣ�ֻ��9%���豸maxVertexInputAttributesΪ16�������ڵ���16���豸��
         9.0%���豸Ϊ28 - 31
        70.7%���豸Ϊ32
         9.6%���豸Ϊ64

        ����������ʱû�з�����Ҫʹ��16���������Ե���������������ݶ�ʹ��16��
        (���ʱ���ȥ��Զ�����ٴβ�ѯ��ֵ�Ƿ�ɸĳɸ��ߵ�ֵ���Լ��Ƿ���Ҫ)

    2.Ϊ��va_nameʹ��char[][]������String�Լ���̬�����ڴ棿

        ����Ϊ�˱رܶ�̬�����ڴ棬�Լ�����ֱ��memcpy�������Դ˴��������塣
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