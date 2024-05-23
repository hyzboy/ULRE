#include<hgl/graph/PrimitiveCreater.h>
#include<hgl/graph/VKShaderModule.h>
#include<hgl/graph/VKVertexAttribBuffer.h>
#include<hgl/graph/VKPrimitive.h>
//#include<hgl/graph/VertexDataManager.h>

VK_NAMESPACE_BEGIN

PrimitiveData *CreatePrimitiveData(const VIL *_vil,const VkDeviceSize vc);
void        SetIndexBuffer( PrimitiveData *pd,IndexBuffer *ib,const VkDeviceSize ic);
VABAccess * GetVABAccess(   PrimitiveData *pd,const int);
void        Destory(        PrimitiveData *pd);

PrimitiveCreater::PrimitiveCreater(GPUDevice *dev,const VIL *v,const AnsiString &name)
{
    device          =dev;
    phy_device      =device->GetPhysicalDevice();
    vil             =v;

    prim_name       =name;
    prim_data       =nullptr;

    vertices_number =0;
    vab_proc_count  =0;
    index_number    =0;

    ibo             =nullptr;
    ibo_map         =nullptr;
}

//PrimitiveCreater::PrimitiveCreater(VertexDataManager *_vdm)
//{
//    device          =_vdm->GetDevice();
//    phy_device      =device->GetPhysicalDevice();
//
//    vdm             =_vdm;
//    vil             =vdm->GetVIL();
//
//    vertices_number =0;
//    index_number    =0;
//    ibo             =nullptr;
//    ibo_map         =nullptr;
//}

bool PrimitiveCreater::Init(const VkDeviceSize vertex_count,const VkDeviceSize index_count,IndexType it)
{
    if(vertex_count<=0)return(false);

    prim_data=CreatePrimitiveData(vil,vertex_count);

    if(!prim_data)return(false);

    vertices_number=vertex_count;

    if(index_count>0)
    {
        if(it==IndexType::AUTO)
        {
            it=device->ChooseIndexType(vertex_count);

            if(!IsIndexType(it))
                return(false);
        }
        else
        {
            if(!device->CheckIndexType(it,vertex_count))
                return(false);
        }

        ibo=device->CreateIBO(it,index_count);

        if(!ibo)return(false);

        index_number=index_count;

        SetIndexBuffer(prim_data,ibo,index_count);
        
    #ifdef _DEBUG
        DebugUtils *du=device->GetDebugUtils();

        if(du)
        {
            du->SetBuffer(      ibo->GetBuffer(),   prim_name+":IndexBuffer:Buffer");
            du->SetDeviceMemory(ibo->GetVkMemory(), prim_name+":IndexBuffer:Memory");
        }
    #endif//_DEBUG
    }

    return(true);
}

VABAccess *PrimitiveCreater::AcquirePVB(const AnsiString &name,const VkFormat &acquire_format,const void *data,const VkDeviceSize bytes)
{
    if(!vil)return(nullptr);
    if(name.IsEmpty())return(nullptr);

    const int index=vil->GetIndex(name);

    if(index<0||index>=vil->GetCount())
        return(nullptr);

    const VertexInputFormat *vif=vil->GetConfig(index);

    if(!vif)
        return(nullptr);

    if(vif->format!=acquire_format)
        return(nullptr);
    
    if(data)
        if(vif->stride*vertices_number!=bytes)
            return(nullptr);

    VABAccess *vab_access=GetVABAccess(prim_data,index);

    if(!vab_access)
        return(nullptr);

    if(!vab_access->vab)
    {
        vab_access->vab=device->CreateVAB(vif->format,vertices_number,data);
        vab_access->start=0;
        vab_access->count=vertices_number;
        
    #ifdef _DEBUG
        DebugUtils *du=device->GetDebugUtils();

        if(du)
        {
            du->SetBuffer(      vab_access->vab->GetBuffer(),   prim_name+":VAB:Buffer:"+name);
            du->SetDeviceMemory(vab_access->vab->GetVkMemory(), prim_name+":VAB:Memory:"+name);
        }
    #endif//_DEBUG

        ++vab_proc_count;
    }

    if(data)
        vab_access->vab->Write(data,bytes);

    return vab_access;
}

void PrimitiveCreater::ClearAllData()
{
    if(ibo_map)
    {
        ibo->Unmap();
        ibo_map=nullptr;
    }

    ibo=nullptr;

    if(prim_data)
    {
        Destory(prim_data);
        prim_data=nullptr;
    }
}

Primitive *PrimitiveCreater::Finish(RenderResource *rr)
{
    const uint si_count=vil->GetCount(VertexInputGroup::Basic);

    if(vab_proc_count!=si_count)
        return(nullptr);

    if(ibo_map)
    {
        ibo->Unmap();
        ibo_map=nullptr;
    }

    Primitive *primitive=rr->CreatePrimitive(prim_name,prim_data);

    if(!primitive)
    {
        ClearAllData();
        return(nullptr);
    }

    ibo_map=nullptr;
    ibo=nullptr;
    prim_data=nullptr;

    return primitive;
}
VK_NAMESPACE_END
