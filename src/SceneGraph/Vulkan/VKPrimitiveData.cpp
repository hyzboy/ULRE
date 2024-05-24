#include"VKPrimitiveData.h"
#include<hgl/graph/VKVertexInputLayout.h>
#include<hgl/graph/VKVertexAttribBuffer.h>
#include<hgl/graph/VKIndexBuffer.h>
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VertexDataManager.h>

VK_NAMESPACE_BEGIN

PrimitiveData::PrimitiveData(const VIL *_vil,const VkDeviceSize vc)
{
    vil=_vil;

    vertex_count=vc;

    vab_access=hgl_zero_new<VABAccess>(_vil->GetCount());

    hgl_zero(ib_access);
}

PrimitiveData::~PrimitiveData()
{
    SAFE_CLEAR_ARRAY(vab_access);       //注意：这里并不释放VAB，在派生类中释放
}

const int PrimitiveData::GetVABCount()const
{
    return vil->GetCount();
}

const int PrimitiveData::GetVABIndex(const AnsiString &name) const
{
    if(name.IsEmpty())return(-1);

    return vil->GetIndex(name);
}

VABAccess *PrimitiveData::GetVABAccess(const int index)
{
    if(index<0||index>=vil->GetCount())return(nullptr);

    return vab_access+index;
}

VABAccess *PrimitiveData::GetVABAccess(const AnsiString &name)
{
    if(name.IsEmpty())return(nullptr);

    const int index=vil->GetIndex(name);

    if(index<0)return(nullptr);

    return vab_access+index;
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

//void SetIndexBuffer(PrimitiveData *pd,IndexBuffer *ib,const VkDeviceSize ic)
//{
//    if(!pd)return;
//    
//    pd->ib_access.buffer=ib;
//    pd->ib_access.start=0;
//    pd->ib_access.count=ic;
//}

namespace
{
    /**
    * 直接使用GPUDevice创建VAB/IBO,并在释构时释放
    */
    class PrimitiveDataPrivateBuffer:public PrimitiveData
    {
        GPUDevice *device;

    public:

        PrimitiveDataPrivateBuffer(GPUDevice *dev,const VIL *_vil,const VkDeviceSize vc):PrimitiveData(_vil,vc)
        {
            device=dev;
        }

        ~PrimitiveDataPrivateBuffer()
        {
            VABAccess *vab=vab_access;

            for(uint i=0;i<vil->GetCount();i++)
            {
                if(vab->vab)
                {
                    delete vab->vab;
                    vab->vab=nullptr;
                }

                ++vab;
            }

            if(ib_access.buffer)
            {
                delete ib_access.buffer;
                ib_access.buffer=nullptr;
            }
        }

        IBAccess *InitIBO(const VkDeviceSize index_count,IndexType it) override
        {
            if(!device)return(nullptr);

            if(ib_access.buffer)
            {
                delete ib_access.buffer;
                ib_access.buffer=nullptr;
            }

            ib_access.buffer=device->CreateIBO(it,index_count);

            if(!ib_access.buffer)
                return(nullptr);

            ib_access.start=0;
            ib_access.count=index_count;

            return(&ib_access);
        }
        
        VABAccess *InitVAB(const AnsiString &name,const VkFormat &format,const void *data,const VkDeviceSize bytes)
        {
            if(!device)return(nullptr);
            if(!vil)return(nullptr);
            if(name.IsEmpty())return(nullptr);
            
            const int index=vil->GetIndex(name);

            if(index<0||index>=vil->GetCount())
                return(nullptr);

            const VertexInputFormat *vif=vil->GetConfig(index);

            if(!vif)return(nullptr);

            if(vif->format!=format)
                return(nullptr);

            if(data)
            {
                if(vif->stride*vertex_count!=bytes)
                    return(nullptr);
            }

            VABAccess *vaba=vab_access+index;

            if(!vaba->vab)
            {
                vaba->vab=device->CreateVAB(format,vertex_count,data);

                if(!vaba->vab)
                    return(nullptr);

                vaba->start=0;
                vaba->count=vertex_count;
            }
            else
            {
                vaba->vab->Write(data,vertex_count);
            }

            return vaba;
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

        PrimitiveDataVDM(VertexDataManager *_vdm,const VIL *_vil,const VkDeviceSize vc):PrimitiveData(_vil,vc)
        {
            vdm=_vdm;

            ib_node=nullptr;
            vab_node=vdm->AcquireVAB(vc);
        }

        ~PrimitiveDataVDM()
        {
            if(ib_node)
                vdm->ReleaseIB(ib_node);

            if(vab_node)
                vdm->ReleaseVAB(vab_node);
        }
        
        IBAccess *InitIBO(const VkDeviceSize index_count,IndexType it) override
        {
            if(index_count<=0)return(nullptr);
            if(!vdm)return(nullptr);

            if(!ib_node)
            {
                ib_node=vdm->AcquireIB(index_count);

                if(!ib_node)
                    return(nullptr);

                ib_access.buffer=vdm->GetIBO();
                ib_access.start =ib_node->GetStart();
                ib_access.count =ib_node->GetCount();
            }

            return &ib_access;
        }
        
        VABAccess *InitVAB(const AnsiString &name,const VkFormat &format,const void *data,const VkDeviceSize bytes)
        {
            if(!vdm)return(nullptr);
            if(!vil)return(nullptr);
            if(name.IsEmpty())return(nullptr);
            
            const int index=vil->GetIndex(name);

            if(index<0||index>=vil->GetCount())
                return(nullptr);

            const VertexInputFormat *vif=vil->GetConfig(index);

            if(!vif)return(nullptr);

            if(vif->format!=format)
                return(nullptr);

            if(data)
            {
                if(vif->stride*vertex_count!=bytes)
                    return(nullptr);
            }

            VABAccess *vaba=vab_access+index;

            if(!vaba->vab)
            {
                vaba->vab=vdm->GetVAB(index);

                if(!vaba->vab)
                    return(nullptr);

                vaba->start=vab_node->GetStart();
                vaba->count=vab_node->GetCount();
            }
            
            if(vaba->vab)
                vaba->vab->Write(data,vaba->start,vaba->count);

            return vaba;
        }
    };//class PrimitiveDataVDM:public PrimitiveData
}//namespace

PrimitiveData *CreatePrimitiveData(GPUDevice *dev,const VIL *_vil,const VkDeviceSize vc)
{
    if(!dev)return(nullptr);
    if(!_vil)return(nullptr);
    if(vc<=0)return(nullptr);

    return(new PrimitiveDataPrivateBuffer(dev,_vil,vc));
}

PrimitiveData *CreatePrimitiveData(VertexDataManager *vdm,const VIL *_vil,const VkDeviceSize vc)
{
    if(!vdm)return(nullptr);
    if(!_vil)return(nullptr);
    if(vc<=0)return(nullptr);

    return(new PrimitiveDataVDM(vdm,_vil,vc));
}
VK_NAMESPACE_END
