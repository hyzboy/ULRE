#include"VKPrimitiveData.h"
#include<hgl/graph/VKVertexInputLayout.h>
#include<hgl/graph/VKVertexAttribBuffer.h>
#include<hgl/graph/VKIndexBuffer.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VertexDataManager.h>

VK_NAMESPACE_BEGIN

PrimitiveData::PrimitiveData(const VIL *_vil,const uint32_t vc)
{
    vil=_vil;

    vertex_count=vc;
    index_count=0;

    vab_list=hgl_zero_new<VAB *>(_vil->GetVertexAttribCount());
    vab_map_list=new VABMap[_vil->GetVertexAttribCount()];

    ibo=nullptr;
}

PrimitiveData::~PrimitiveData()
{
    delete[] vab_map_list;
    delete[] vab_list;       //注意：这里并不释放VAB，在派生类中释放
}

const uint32_t PrimitiveData::GetVABCount()const
{
    return vil->GetVertexAttribCount();
}

const int PrimitiveData::GetVABIndex(const AnsiString &name) const
{
    if(name.IsEmpty())return(-1);

    return vil->GetIndex(name);
}

VAB *PrimitiveData::GetVAB(const int index)
{
    if(index<0||index>=vil->GetVertexAttribCount())return(nullptr);

    return vab_list[index];
}

VAB *PrimitiveData::InitVAB(const int vab_index,const void *data)
{
    if(!vil)return(nullptr);

    if(vab_index<0||vab_index>=vil->GetVertexAttribCount())
        return(nullptr);

    const VertexInputFormat *vif=vil->GetConfig(vab_index);

    if(!vif)return(nullptr);

    if(!vab_list[vab_index])
    {
        vab_list[vab_index]=CreateVAB(vab_index,vif->format,data);

        if(!vab_list[vab_index])
            return(nullptr);
    }
    else
    {
        vab_map_list[vab_index].Write(data,vertex_count);
    }

    return vab_list[vab_index];
}

VABMap *PrimitiveData::GetVABMap(const int vab_index)
{
    if(vab_index<0||vab_index>=vil->GetVertexAttribCount())return nullptr;

    VABMap *vab_map=vab_map_list+vab_index;

    if(!vab_map->IsValid())
    {
        if(!vab_list[vab_index])
            return(nullptr);

        vab_map->SetVAB(vab_list[vab_index],GetVertexOffset(),vertex_count);
    }

    return vab_map;
}

IndexBuffer *PrimitiveData::InitIBO(const int ic,IndexType it)
{
    if(ibo)delete ibo;

    ibo=CreateIBO(ic,it);

    if(!ibo)
        return(nullptr);

    index_count=ic;

    ibo_map.SetIBO(ibo,GetFirstIndex(),index_count);

    return(ibo);
}

void PrimitiveData::UnmapAll()
{
    for(uint32_t i=0;i<vil->GetVertexAttribCount();i++)
        vab_map_list[i].Unmap();

    ibo_map.Unmap();
}

namespace
{
    /**
    * 直接使用VulkanDevice创建VAB/IBO,并在释构时释放
    */
    class PrimitiveDataPrivateBuffer:public PrimitiveData
    {
        VulkanDevice *device;

    public:
        
        int32_t  GetVertexOffset ()const override{return 0;}
        uint32_t GetFirstIndex   ()const override{return 0;}

        VertexDataManager * GetVDM()const override{return nullptr;}                           ///<取得顶点数据管理器

    public:

        PrimitiveDataPrivateBuffer(VulkanDevice *dev,const VIL *_vil,const uint32_t vc):PrimitiveData(_vil,vc)
        {
            device=dev;
        }

        ~PrimitiveDataPrivateBuffer() override
        {
            VAB **vab=vab_list;

            for(uint i=0;i<vil->GetVertexAttribCount();i++)
            {
                if(*vab)
                    delete *vab;

                ++vab;
            }

            if(ibo)
                delete ibo;
        }

        IndexBuffer *CreateIBO(const uint32_t ic,const IndexType &it) override
        {
            if(!device)return(nullptr);

            return device->CreateIBO(it,ic);
        }

        VAB *CreateVAB(const int vab_index,const VkFormat format,const void *data) override
        {
            if(!device)return(nullptr);

            return device->CreateVAB(format,vertex_count,data);
        }
    };//class PrimitiveDataPrivateBuffer:public PrimitiveData

    /**
    * 使用VertexDataBuffer分配VAB/IBO，在本类析构时归还数据
    */
    class PrimitiveDataVDM:public PrimitiveData
    {
        VertexDataManager *vdm;

        DataChain::UserNode *ib_node;
        DataChain::UserNode *vab_node;

    public:

        int32_t             GetVertexOffset ()const override{return vab_node->GetStart();}
        uint32_t            GetFirstIndex   ()const override{return ib_node->GetStart();}
        VertexDataManager * GetVDM          ()const override{return vdm;}                           ///<取得顶点数据管理器

    public:

        PrimitiveDataVDM(VertexDataManager *_vdm,const uint32_t vc):PrimitiveData(_vdm->GetVIL(),vc)
        {
            vdm=_vdm;

            ib_node=nullptr;
            vab_node=vdm->AcquireVAB(vc);
        }

        ~PrimitiveDataVDM() override
        {
            if(ib_node)
                vdm->ReleaseIB(ib_node);

            if(vab_node)
                vdm->ReleaseVAB(vab_node);
        }

        IndexBuffer *CreateIBO(const uint32_t ic,const IndexType &it) override
        {
            if(!vdm)
                return(nullptr);

            if(!ib_node)
            {
                ib_node=vdm->AcquireIB(ic);

                if(!ib_node)
                    return(nullptr);
            }

            return vdm->GetIBO();
        }

        VAB *CreateVAB(const int vab_index,const VkFormat format,const void *data) override
        {
            VAB *vab=vdm->GetVAB(vab_index);

            if(!vab)return(nullptr);

            if(data)
                vab->Write(data,vab_node->GetStart(),vertex_count);

            return vab;
        }
    };//class PrimitiveDataVDM:public PrimitiveData
}//namespace

PrimitiveData *CreatePrimitiveData(VulkanDevice *dev,const VIL *_vil,const uint32_t vc)
{
    if(!dev)return(nullptr);
    if(!_vil)return(nullptr);
    if(vc<=0)return(nullptr);

    return(new PrimitiveDataPrivateBuffer(dev,_vil,vc));
}

PrimitiveData *CreatePrimitiveData(VertexDataManager *vdm,const uint32_t vc)
{
    if(!vdm)return(nullptr);
    if(vc<=0)return(nullptr);

    return(new PrimitiveDataVDM(vdm,vc));
}
VK_NAMESPACE_END
