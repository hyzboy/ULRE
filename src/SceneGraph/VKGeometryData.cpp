#include<hgl/graph/geo/VKGeometryData.h>
#include<hgl/vk/VKVertexInputLayout.h>
#include<hgl/vk/VKVertexAttribBuffer.h>
#include<hgl/vk/VKIndexBuffer.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VertexDataManager.h>
#include<hgl/graph/module/BufferManager.h>
#include<cstring>

namespace hgl::graph{

GeometryData::GeometryData(const VertexFormatMap &format_map,const uint32_t vertex_count_value)
{
    vertex_format_map=format_map;

    vertex_count=vertex_count_value;
    index_count=0;

    vab_list=zero_new<VAB *>(vertex_format_map.size());

    ibo=nullptr;
}

GeometryData::~GeometryData()
{
    delete[] vab_list;       //注意：这里并不释放VAB，在派生类中释放
}

const uint32_t GeometryData::GetVABCount()const
{
    return static_cast<uint32_t>(vertex_format_map.size());
}

const int GeometryData::GetVABIndex(const VertexAttrib attrib) const
{
    int i=0;
    for(const auto &[key, _] : vertex_format_map)
    {
        if(key==attrib)
            return i;

        ++i;
    }

    return -1;
}

VkFormat GeometryData::GetVABFormat(const int index)const
{
    if(index<0||index>=static_cast<int>(GetVABCount()))
        return VK_FORMAT_UNDEFINED;

    int i=0;
    for(const auto &[_, format] : vertex_format_map)
    {
        if(i==index)
            return format;

        ++i;
    }

    return VK_FORMAT_UNDEFINED;
}

bool GeometryData::CreateAllVAB(const AnsiString &geometry_name)
{
    int i=0;
    for(const auto &[attrib, format] : vertex_format_map)
    {
        if(vab_list[i])continue;

        AnsiString vab_name = geometry_name + AnsiString(":") + GetVertexAttribName(attrib);

        vab_list[i]=CreateVAB(i,format,nullptr,vab_name);

        if(!vab_list[i])
            return(false);

        ++i;
    }

    return(true);
}

VAB *GeometryData::GetVABByIndex(const int index)const
{
    if(index<0||index>=static_cast<int>(GetVABCount()))return(nullptr);

    return vab_list[index];
}

VAB *GeometryData::GetVABByAttrib(const VertexAttrib attrib)const
{
    const int index=GetVABIndex(attrib);

    if(index<0||index>=static_cast<int>(GetVABCount()))
        return(nullptr);

    if(index<0||index>=static_cast<int>(GetVABCount()))
        return(nullptr);

    return vab_list[index];
}

VAB *GeometryData::InitVAB(const int vab_index,const void *data,const AnsiString &name)
{
    if(vab_index<0||vab_index>=static_cast<int>(GetVABCount()))
        return(nullptr);

    const VkFormat format=GetVABFormat(vab_index);
    if(format==VK_FORMAT_UNDEFINED)
        return(nullptr);

    if(!vab_list[vab_index])
    {
        vab_list[vab_index]=CreateVAB(vab_index,format,data,name);

        if(!vab_list[vab_index])
            return(nullptr);
    }
    else
    {
        vab_list[vab_index]->Write(data, vertex_count);
    }

    return vab_list[vab_index];
}

IndexBuffer *GeometryData::InitIBO(const int ic,IndexType it,const AnsiString &name)
{
    if(ibo)delete ibo;

    ibo=CreateIBO(ic,it,name);

    if(!ibo)
        return(nullptr);

    index_count=ic;

    return(ibo);
}

void GeometryData::UnmapAll()
{
    // VAB/IBO 访问已由 BufferAccessor 自动管理
}

namespace
{
    /**
    * 直接使用VulkanDevice创建VAB/IBO,并在释构时释放
    */
    class GeometryDataPrivateBuffer:public GeometryData
    {
        VulkanDevice *device;
        BufferAllocPolicy policy;

    public:

        int32_t  GetVertexOffset ()const override{return 0;}
        uint32_t GetFirstIndex   ()const override{return 0;}

        VertexDataManager * GetVDM()const override{return nullptr;}                           ///<取得顶点数据管理器

    public:

        GeometryDataPrivateBuffer(VulkanDevice *dev,const VertexFormatMap &format_map,const uint32_t vertex_count,BufferAllocPolicy p):GeometryData(format_map,vertex_count)
        {
            device=dev;
            policy=p;
        }

        ~GeometryDataPrivateBuffer() override
        {
            VAB **vab=vab_list;

            for(uint i=0;i<GetVABCount();i++)
            {
                if(*vab)
                    delete *vab;

                ++vab;
            }

            if(ibo)
                delete ibo;
        }

        IndexBuffer *CreateIBO(const uint32_t ic,const IndexType &it,const AnsiString &name) override
        {
            if(!device)return(nullptr);

            return device->CreateIBO(ObjectNameBuilder(name),it,ic,nullptr,policy);
        }

        VAB *CreateVAB(const int vab_index,const VkFormat format,const void *data,const AnsiString &name) override
        {
            if(!device)return(nullptr);

            return device->CreateVAB(ObjectNameBuilder(name),format,vertex_count,data,policy);
        }
    };//class GeometryDataPrivateBuffer:public GeometryData

    /**
    * 使用BufferManager创建VAB/IBO,并在析构时通过BufferManager释放
    */
    class GeometryDataPrivateBufferBM:public GeometryData
    {
        BufferManager *buffer_manager;
        BufferAllocPolicy policy;

    public:

        int32_t  GetVertexOffset ()const override{return 0;}
        uint32_t GetFirstIndex   ()const override{return 0;}

        VertexDataManager * GetVDM()const override{return nullptr;}                           ///<取得顶点数据管理器

    public:

        GeometryDataPrivateBufferBM(BufferManager *bm,const VertexFormatMap &format_map,const uint32_t vertex_count,BufferAllocPolicy p):GeometryData(format_map,vertex_count)
        {
            buffer_manager=bm;
            policy=p;
        }

        ~GeometryDataPrivateBufferBM() override
        {
            VAB **vab=vab_list;

            for(uint i=0;i<GetVABCount();i++)
            {
                if(*vab && buffer_manager)
                    buffer_manager->Release(*vab);

                ++vab;
            }

            if(ibo && buffer_manager)
                buffer_manager->Release(ibo);
        }

        IndexBuffer *CreateIBO(const uint32_t ic,const IndexType &it,const AnsiString &name) override
        {
            if(!buffer_manager)return(nullptr);

            return buffer_manager->CreateIBO(ObjectNameBuilder(name),it,ic,nullptr,policy);
        }

        VAB *CreateVAB(const int vab_index,const VkFormat format,const void *data,const AnsiString &name) override
        {
            if(!buffer_manager)return(nullptr);

            return buffer_manager->CreateVAB(ObjectNameBuilder(name),format,vertex_count,data,policy);
        }
    };//class GeometryDataPrivateBufferBM:public GeometryData

    /**
    * 使用VertexDataBuffer分配VAB/IBO，在本类析构时归还数据
    */
    class GeometryDataVDM:public GeometryData, public IVDMClient
    {
        VertexDataManager *vdm;

        BlockAllocator::UserNode *ib_node;
        BlockAllocator::UserNode *vab_node;

    public:

        int32_t             GetVertexOffset ()const override{return vab_node?vab_node->GetStart():0;}
        uint32_t            GetFirstIndex   ()const override{return ib_node?ib_node->GetStart():0;}
        VertexDataManager * GetVDM          ()const override{return vdm;}                           ///<取得顶点数据管理器

    public:

        GeometryDataVDM(VertexDataManager *_vdm,const uint32_t vertex_count):GeometryData(_vdm->GetVertexFormatMap(),vertex_count)
        {
            vdm=_vdm;

            ib_node=nullptr;
            vab_node=vdm->AcquireVAB(vertex_count);

            vdm->RegisterClient(this);
        }

        ~GeometryDataVDM() override
        {
            if(vdm)
            {
                vdm->UnregisterClient(this);

                if(ib_node)
                    vdm->ReleaseIB(ib_node);

                if(vab_node)
                    vdm->ReleaseVAB(vab_node);
            }

            ib_node=nullptr;
            vab_node=nullptr;
            vdm=nullptr;
        }

        // IVDMClient: VDM 即将销毁，清空所有对 VDM 的引用，避免悬空指针
        void OnVDMDestroyed() override
        {
            ib_node=nullptr;
            vab_node=nullptr;
            vdm=nullptr;
        }

        IndexBuffer *CreateIBO(const uint32_t ic,const IndexType &it,const AnsiString &/*name*/) override
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

        VAB *CreateVAB(const int vab_index,const VkFormat format,const void *data,const AnsiString &name) override
        {
            if(!vdm)return(nullptr);

            VAB *vab=vdm->GetVAB(vab_index);

            if(!vab)return(nullptr);

            if(data)
                vab->Write(data,vab_node->GetStart(),vertex_count);

            return vab;
        }
    };//class GeometryDataVDM:public GeometryData
}//namespace

GeometryData *CreateGeometryData(VulkanDevice *dev,const VertexFormatMap &format_map,const uint32_t vertex_count)
{
    return CreateGeometryData(dev, format_map, vertex_count, BufferAllocPolicy::GPUOnly);
}

GeometryData *CreateGeometryData(VulkanDevice *dev,const VertexFormatMap &format_map,const uint32_t vertex_count,BufferAllocPolicy policy)
{
    if(!dev)return(nullptr);
    if(format_map.empty())return(nullptr);
    if(vertex_count<=0)return(nullptr);

    return(new GeometryDataPrivateBuffer(dev,format_map,vertex_count,policy));
}

GeometryData *CreateGeometryData(BufferManager *bm,const VertexFormatMap &format_map,const uint32_t vertex_count)
{
    return CreateGeometryData(bm, format_map, vertex_count, BufferAllocPolicy::GPUOnly);
}

GeometryData *CreateGeometryData(BufferManager *bm,const VertexFormatMap &format_map,const uint32_t vertex_count,BufferAllocPolicy policy)
{
    if(!bm)return(nullptr);
    if(format_map.empty())return(nullptr);
    if(vertex_count<=0)return(nullptr);

    return(new GeometryDataPrivateBufferBM(bm,format_map,vertex_count,policy));
}

GeometryData *CreateGeometryData(VertexDataManager *vdm,const uint32_t vertex_count)
{
    if(!vdm)return(nullptr);
    if(vertex_count<=0)return(nullptr);

    return(new GeometryDataVDM(vdm,vertex_count));
}
}//namespace hgl::graph
