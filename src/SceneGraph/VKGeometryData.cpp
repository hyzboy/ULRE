#include<hgl/graph/geo/VKGeometryData.h>
#include<hgl/vk/VKVertexAttribBuffer.h>
#include<hgl/vk/VKIndexBuffer.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VertexDataManager.h>
#include<hgl/graph/module/BufferManager.h>

namespace hgl::graph{

GeometryData::GeometryData(const GeometryVertexFormat &gvf,const uint32_t vc)
{
    geometry_vertex_format=gvf;

    vertex_count=vc;
    index_count=0;

    vab_list.resize(geometry_vertex_format.GetCount(),nullptr);

    ibo=nullptr;
}

GeometryData::~GeometryData()
{
    // 注意：这里并不释放 VAB，在派生类中释放
}

const uint32_t GeometryData::GetVABCount()const
{
    return geometry_vertex_format.GetCount();
}

const int GeometryData::GetVABIndex(const VertexSemantic semantic) const
{
    if(semantic==VertexSemantic::Unknown)
        return -1;

    const uint32_t count=geometry_vertex_format.GetCount();

    for(uint32_t i=0;i<count;i++)
    {
        const GeometryVertexAttributeFormat *attribute=geometry_vertex_format.Get(i);

        if(attribute&&attribute->semantic==semantic)
            return int(i);
    }

    return -1;
}

int GeometryData::DeclareVertexAttribute(const VertexSemantic semantic,const VkFormat format,const uint8_t vec_size,const uint32_t stride)
{
    if(semantic==VertexSemantic::Unknown||format==VK_FORMAT_UNDEFINED)
        return -1;

    const int exist_index=GetVABIndex(semantic);
    if(exist_index>=0)
    {
        const GeometryVertexAttributeFormat *attribute=geometry_vertex_format.Get(exist_index);

        if(!attribute)
            return -1;

        if(attribute->format!=format)
            return -1;

        return exist_index;
    }

    if(!geometry_vertex_format.Add(semantic,format,vec_size,stride))
        return -1;

    vab_list.push_back(nullptr);
    return int(vab_list.size()-1);
}

int GeometryData::DeclareVertexAttribute(const VertexInputFormat *vif)
{
    if(!vif)
        return -1;

    return DeclareVertexAttribute(vif->semantic,vif->format,uint8_t(vif->vec_size),uint32_t(vif->stride));
}

bool GeometryData::CreateAllVAB(const AnsiString &geometry_name)
{
    const uint32_t count=GetVABCount();

    for(uint32_t i=0;i<count;i++)
    {
        if(vab_list[i])continue;

        const GeometryVertexAttributeFormat *attribute=geometry_vertex_format.Get(i);
        if(!attribute)
            return false;

        const char *semantic_name=GetVertexSemanticName(attribute->semantic);
        const AnsiString semantic_tag=semantic_name?semantic_name:"Unknown";
        AnsiString vab_name = geometry_name + ":" + semantic_tag;
        vab_list[i]=CreateVAB(i,attribute->format,nullptr,vab_name);

        if(!vab_list[i])
            return(false);
    }

    return(true);
}

VAB *GeometryData::GetVAB(const int index)const
{
    if(index<0||index>=int(vab_list.size()))return(nullptr);

    return vab_list[index];
}

VAB *GeometryData::GetVAB(const VertexSemantic semantic)const
{
    const int index=GetVABIndex(semantic);

    if(index<0||index>=int(vab_list.size()))
        return(nullptr);

    return vab_list[index];
}

VAB *GeometryData::InitVAB(const int vab_index,const void *data,const AnsiString &name)
{
    if(vab_index<0||vab_index>=int(vab_list.size()))
        return(nullptr);

    const GeometryVertexAttributeFormat *attribute=geometry_vertex_format.Get(vab_index);

    if(!attribute)return(nullptr);

    if(!vab_list[vab_index])
    {
        vab_list[vab_index]=CreateVAB(vab_index,attribute->format,data,name);

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

        GeometryDataPrivateBuffer(VulkanDevice *dev,const GeometryVertexFormat &gvf,const uint32_t vc,BufferAllocPolicy p):GeometryData(gvf,vc)
        {
            device=dev;
            policy=p;
        }

        ~GeometryDataPrivateBuffer() override
        {
            for(VAB *vab:vab_list)
            {
                if(vab)
                    delete vab;
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

        GeometryDataPrivateBufferBM(BufferManager *bm,const GeometryVertexFormat &gvf,const uint32_t vc,BufferAllocPolicy p):GeometryData(gvf,vc)
        {
            buffer_manager=bm;
            policy=p;
        }

        ~GeometryDataPrivateBufferBM() override
        {
            for(VAB *vab:vab_list)
            {
                if(vab && buffer_manager)
                    buffer_manager->Release(vab);
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
    * 使用VertexDataManager分配VAB/IBO，在本类析构时归还数据
    */
    class GeometryDataVDM:public GeometryData
    {
        VertexDataManager *vdm;

        BlockAllocator::UserNode *ib_node;
        BlockAllocator::UserNode *vab_node;

    public:

        int32_t             GetVertexOffset ()const override{return vab_node->GetStart();}
        uint32_t            GetFirstIndex   ()const override{return ib_node->GetStart();}
        VertexDataManager * GetVDM          ()const override{return vdm;}                           ///<取得顶点数据管理器

    public:

        GeometryDataVDM(VertexDataManager *_vdm,const GeometryVertexFormat &gvf,const uint32_t vc):GeometryData(gvf,vc)
        {
            vdm=_vdm;

            ib_node=nullptr;
            vab_node=vdm->AcquireVAB(vc);
        }

        ~GeometryDataVDM() override
        {
            if(ib_node)
                vdm->ReleaseIB(ib_node);

            if(vab_node)
                vdm->ReleaseVAB(vab_node);
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
            VAB *vab=vdm->GetVAB(vab_index);

            if(!vab)return(nullptr);

            if(data)
                vab->Write(data,vab_node->GetStart(),vertex_count);

            return vab;
        }
    };//class GeometryDataVDM:public GeometryData
}//namespace

GeometryData *CreateGeometryData(VulkanDevice *dev,const GeometryVertexFormat &gvf,const uint32_t vc)
{
    if(!dev)return(nullptr);
    if(vc<=0)return(nullptr);
    if(gvf.GetCount()==0)return(nullptr);

    // TODO: Route VAB/IBO creation through BufferManager while keeping GeometryData API stable.
    return(new GeometryDataPrivateBuffer(dev,gvf,vc,BufferAllocPolicy::GPUOnly));
}

GeometryData *CreateGeometryData(VulkanDevice *dev,const GeometryVertexFormat &gvf,const uint32_t vc,BufferAllocPolicy policy)
{
    if(!dev)return(nullptr);
    if(vc<=0)return(nullptr);
    if(gvf.GetCount()==0)return(nullptr);

    // TODO: Route VAB/IBO creation through BufferManager while keeping GeometryData API stable.
    return(new GeometryDataPrivateBuffer(dev,gvf,vc,policy));
}

GeometryData *CreateGeometryData(BufferManager *bm,const GeometryVertexFormat &gvf,const uint32_t vc)
{
    if(!bm)return(nullptr);
    if(vc<=0)return(nullptr);
    if(gvf.GetCount()==0)return(nullptr);

    return(new GeometryDataPrivateBufferBM(bm,gvf,vc,BufferAllocPolicy::GPUOnly));
}

GeometryData *CreateGeometryData(BufferManager *bm,const GeometryVertexFormat &gvf,const uint32_t vc,BufferAllocPolicy policy)
{
    if(!bm)return(nullptr);
    if(vc<=0)return(nullptr);
    if(gvf.GetCount()==0)return(nullptr);

    return(new GeometryDataPrivateBufferBM(bm,gvf,vc,policy));
}

GeometryData *CreateGeometryData(VertexDataManager *vdm,const GeometryVertexFormat &gvf,const uint32_t vc)
{
    if(!vdm)return(nullptr);
    if(vc<=0)return(nullptr);
    if(gvf.GetCount()==0)return(nullptr);

    return(new GeometryDataVDM(vdm,gvf,vc));
}
}//namespace hgl::graph
