#include<hgl/graph/VKPrimitiveData.h>

VK_NAMESPACE_BEGIN
bool Init(PrimitiveData *pd,const VIL *_vil,const VkDeviceSize vc,const VkDeviceSize ic=0)
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

int GetVABIndex(const PrimitiveData *pd,const AnsiString &name)
{
    if(!pd)return(-1);
    if(!pd->vil)return(-1);
    if(name.IsEmpty())return(-1);

    return pd->vil->GetIndex(name);
}

const VABAccess *GetVAB(const PrimitiveData *pd,const AnsiString &name)
{
    if(!pd)return(nullptr);    
    if(name.IsEmpty())return(nullptr);

    const int index=GetVABIndex(pd,name);

    if(index==-1)
        return(nullptr);

    return pd->vab_access+index;
}

VABAccess *SetVAB(PrimitiveData *pd,const AnsiString &name,VAB *vab,VkDeviceSize start=0)
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