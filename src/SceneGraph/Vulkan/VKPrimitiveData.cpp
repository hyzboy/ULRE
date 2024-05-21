#include<hgl/graph/VKPrimitiveData.h>

VK_NAMESPACE_BEGIN
bool InitPrimitiveData(PrimitiveData *pd,const VIL *_vil,const VkDeviceSize vc)
{
    if(!pd)return(false);
    if(!_vil)return(false);
    if(vc<=0)return(false);

    hgl_zero(*pd);

    pd->vil=_vil;
    pd->vertex_count=vc;

    return(true);
}

int GetVABIndex(const PrimitiveData *pd,const AnsiString &name)
{
    if(!pd)return(-1);
    if(!pd->vil)return(-1);
    if(name.IsEmpty())return(-1);

    return pd->vil->GetIndex(name);
}

VABAccess *GetVAB(PrimitiveData *pd,const int index)
{
    if(!pd)return(nullptr);
    if(!pd->vil)return(nullptr);
    if(index<0||index>=pd->vil->GetCount())return(nullptr);

    return pd->vab_access+index;
}

VABAccess *SetVAB(PrimitiveData *pd,const int index,VAB *vab,VkDeviceSize start,VkDeviceSize count)
{
    if(!pd)return(nullptr);
    if(!pd->vil)return(nullptr);
    if(index<0||index>=pd->vil->GetCount())return(nullptr);

    VABAccess *vaba=pd->vab_access+index;

    vaba->vab=vab;
    vaba->start=start;
    vaba->count=count;

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

void SetIndexBuffer(PrimitiveData *pd,IndexBuffer *ib,const VkDeviceSize ic)
{
    if(!pd)return;
    
    pd->ib_access.buffer=ib;
    pd->ib_access.start=0;
    pd->ib_access.count=ic;
}

VK_NAMESPACE_END