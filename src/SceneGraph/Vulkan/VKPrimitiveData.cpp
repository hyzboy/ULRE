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

    vab_list=hgl_zero_new<VAB *>(_vil->GetVertexAttribCount());

    ibo=nullptr;
}

PrimitiveData::~PrimitiveData()
{
    delete[] vab_list;       //注意：这里并不释放VAB，在派生类中释放
}

const int PrimitiveData::GetVABCount()const
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

namespace
{
    /**
    * 直接使用GPUDevice创建VAB/IBO,并在释构时释放
    */
    class PrimitiveDataPrivateBuffer:public PrimitiveData
    {
        GPUDevice *device;

    public:
        
        int32_t  GetVertexOffset ()const override{return 0;}
        uint32_t GetFirstIndex   ()const override{return 0;}

    public:

        PrimitiveDataPrivateBuffer(GPUDevice *dev,const VIL *_vil,const uint32_t vc):PrimitiveData(_vil,vc)
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

        IndexBuffer *InitIBO(const uint32_t ic,IndexType it) override
        {
            if(!device)return(nullptr);

            if(ibo)delete ibo;

            ibo=device->CreateIBO(it,ic);

            if(!ibo)
                return(nullptr);

            index_count=ic;

            return(ibo);
        }
        
        VAB *InitVAB(const int vab_index,const VkFormat &format,const void *data)
        {
            if(!device)return(nullptr);
            if(!vil)return(nullptr);

            if(vab_index<0||vab_index>=vil->GetVertexAttribCount())
                return(nullptr);

            const VertexInputFormat *vif=vil->GetConfig(vab_index);

            if(!vif)return(nullptr);

            if(vif->format!=format)
                return(nullptr);

            if(!vab_list[vab_index])
            {
                vab_list[vab_index]=device->CreateVAB(format,vertex_count,data);

                if(!vab_list[vab_index])
                    return(nullptr);
            }
            
            if(vab_list[vab_index]&&data)
            {
                vab_list[vab_index]->Write(data,vertex_count);
            }

            return vab_list[vab_index];
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

        int32_t  GetVertexOffset()const override { return vab_node->GetStart(); }
        uint32_t GetFirstIndex  ()const override { return ib_node->GetStart(); }

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
        
        IndexBuffer *InitIBO(const uint32_t ic,IndexType it) override
        {
            if(ic<=0)return(nullptr);
            if(!vdm)return(nullptr);

            if(!ib_node)
            {
                ib_node=vdm->AcquireIB(ic);

                if(!ib_node)
                    return(nullptr);

                ibo=vdm->GetIBO();
            }

            index_count=ic;

            return ibo;
        }
        
        VAB *InitVAB(const int vab_index,const VkFormat &format,const void *data)
        {
            if(!vdm)return(nullptr);
            if(!vil)return(nullptr);

            if (vab_index<0||vab_index>=vil->GetVertexAttribCount())
                return(nullptr);

            const VertexInputFormat *vif=vil->GetConfig(vab_index);

            if(!vif)return(nullptr);

            if(vif->format!=format)
                return(nullptr);

            if (!vab_list[vab_index])
            {
                vab_list[vab_index]=vdm->GetVAB(vab_index);

                if(!vab_list[vab_index])
                    return(nullptr);
            }
            
            if(vab_list[vab_index]&&data)
                vab_list[vab_index]->Write(data,vab_node->GetStart(),vertex_count);

            return vab_list[vab_index];
        }
    };//class PrimitiveDataVDM:public PrimitiveData
}//namespace

PrimitiveData *CreatePrimitiveData(GPUDevice *dev,const VIL *_vil,const uint32_t vc)
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
