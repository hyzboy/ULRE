#include<hgl/graph/PrimitiveCreater.h>
#include<hgl/graph/VKShaderModule.h>
#include<hgl/graph/VKVertexAttribBuffer.h>
#include<hgl/graph/VKPrimitive.h>
#include<hgl/graph/VertexDataManager.h>

VK_NAMESPACE_BEGIN
PrimitiveCreater::PrimitiveCreater(RenderResource *sdb,const VIL *v)
{
    device          =sdb->GetDevice();
    phy_device      =device->GetPhysicalDevice();

    vdm             =nullptr;
    db              =sdb;
    vil             =v;

    vertices_number =0;
    index_number    =0;
    ibo             =nullptr;
    ibo_map         =nullptr;
}

PrimitiveCreater::PrimitiveCreater(VertexDataManager *_vdm)
{
    device          =_vdm->GetDevice();
    phy_device      =device->GetPhysicalDevice();

    vdm             =_vdm;
    db              =nullptr;
    vil             =vdm->GetVIL();

    vertices_number =0;
    index_number    =0;
    ibo             =nullptr;
    ibo_map         =nullptr;
}

PrimitiveCreater::~PrimitiveCreater()
{
    if(ibo)
    {
        ibo->Unmap();
        delete ibo;
    }
}

bool PrimitiveCreater::Init(const uint32 vertex_count,const uint32 index_count,IndexType it)
{
    if(vertex_count<=0)return(false);

    vertices_number=vertex_count;

    if(index_count>0)
    {
        index_number=index_count;

        if(vdm)
        {
        }
        else
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
            //ibo=db->CreateIBO(it,index_count);
            if(!ibo)return(false);
        }

        ibo_map=ibo->Map();
    }

    return(true);
}

bool PrimitiveCreater::AcquirePVB(VABAccess *vad,const AnsiString &name,const void *data)
{
    if(!vad)return(false);
    if(!vil)return(false);
    if(name.IsEmpty())return(false);
            
    const VertexInputFormat *vif=vil->GetConfig(name);

    if(!vif)
        return(false);

    if(vab_map.Get(name,*vad))
        return true;

    vad->vab    =db->CreateVAB(vif->format,vertices_number,data);

    if(!data)
        vad->map_ptr=vad->vab->Map();
    else
        vad->map_ptr=nullptr;

    vab_map.Add(name,*vad);

    return true;
}

bool PrimitiveCreater::WriteVAB(const AnsiString &name,const void *data,const uint32_t bytes)
{
    if(!vil)return(false);
    if(name.IsEmpty())return(false);
    if(!data)return(false);
    if(!bytes)return(false);
            
    const VertexInputFormat *vif=vil->GetConfig(name);

    if(!vif)
        return(false);

    if(vif->stride*vertices_number!=bytes)
        return(false);

    VABAccess vad;

    return AcquirePVB(&vad,name,data);
}

void PrimitiveCreater::ClearAllData()
{
    if(vab_map.GetCount()>0)
    {
        const auto *sp=vab_map.GetDataList();
        for(int i=0;i<vab_map.GetCount();i++)
        {
            if((*sp)->value.vab)
            {
                (*sp)->value.vab->Unmap();
                delete (*sp)->value.vab;
            }
                
            ++sp;
        }
    }

    if(ibo)
    {
        ibo->Unmap();
        delete ibo;
        ibo=nullptr;
    }
}

Primitive *PrimitiveCreater::Finish(const AnsiString &prim_name)
{
    const uint si_count=vil->GetCount(VertexInputGroup::Basic);

    if(vab_map.GetCount()!=si_count)
        return(nullptr);

    Primitive *primitive=db->CreatePrimitive(prim_name,vertices_number);

    const auto *sp=vab_map.GetDataList();
    for(uint i=0;i<si_count;i++)
    {
        if((*sp)->value.vab)
        {
            if((*sp)->value.map_ptr)
                (*sp)->value.vab->Unmap();

            primitive->SetVAB((*sp)->key,(*sp)->value.vab);
        }
        else
        {
            //ClearAllData();
            return(nullptr);
        }

        ++sp;
    }

    if(ibo)
    {
        ibo->Unmap();
        primitive->SetIndex(ibo,0,index_number);

        db->Add(ibo);
        ibo=nullptr;    //避免释构函数删除
    }

    db->Add(primitive);

    return primitive;
}
VK_NAMESPACE_END
