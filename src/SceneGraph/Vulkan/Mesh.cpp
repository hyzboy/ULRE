#include<hgl/graph/Mesh.h>
#include<hgl/graph/VKMaterialInstance.h>
#include<hgl/graph/VKMaterialParameters.h>
#include<hgl/graph/VKMaterial.h>
#include<hgl/graph/VKVertexAttribBuffer.h>
#include<hgl/graph/VKIndexBuffer.h>

VK_NAMESPACE_BEGIN
MeshDataBuffer::MeshDataBuffer(const uint32_t c,IndexBuffer *ib,VertexDataManager *_vdm)
{
    vab_count=c;

    vab_list=hgl_zero_new<VkBuffer>(vab_count);
    vab_offset=hgl_zero_new<VkDeviceSize>(vab_count);

    ibo=ib;
    vdm=_vdm;
}

MeshDataBuffer::~MeshDataBuffer()
{
    delete[] vab_offset;
    delete[] vab_list;
}

const int MeshDataBuffer::compare(const MeshDataBuffer &mesh_data_buffer)const
{
    ptrdiff_t off;

    if(vdm&&mesh_data_buffer.vdm)
    {
        off=(ptrdiff_t)vdm-(ptrdiff_t)mesh_data_buffer.vdm;
        if(off)
            return off;
    }

    off=vab_count-mesh_data_buffer.vab_count;
    if(off)
        return off;

    off=hgl_cmp(vab_list,mesh_data_buffer.vab_list,vab_count);
    if(off)
        return off;

    off=hgl_cmp(vab_offset,mesh_data_buffer.vab_offset,vab_count);
    if(off)
        return off;

    off=ibo-mesh_data_buffer.ibo;

    return off;
}

void MeshRenderData::Set(const Primitive *prim)
{
    if(!prim)
    {
        data_vertex_count = 0;
        data_index_count = 0;
        vertex_count = 0;
        index_count = 0;
        vertex_offset = 0;
        first_index = 0;
        return;
    }

    // data counts come from primitive (buffer capacity)
    data_vertex_count = (uint32_t)prim->GetVertexCount();
    data_index_count  = prim->GetIndexCount();

    // initialize draw counts to data counts by default
    vertex_count    = data_vertex_count;
    index_count     = data_index_count;

    vertex_offset   = prim->GetVertexOffset();
    first_index     = prim->GetFirstIndex();
}

SubMesh::SubMesh(Primitive *r,MaterialInstance *mi,Pipeline *p,MeshDataBuffer *mesh_data_buffer)
{
    primitive=r;
    pipeline=p;
    mat_inst=mi;

    data_buffer=mesh_data_buffer;
    render_data.Set(primitive);
}

bool SubMesh::UpdatePrimitive()
{
    render_data.Set(primitive);

    // Clamp draw counts if previously set larger than new data counts
    if(render_data.vertex_count>render_data.data_vertex_count)
        render_data.vertex_count = render_data.data_vertex_count;

    if(render_data.index_count>render_data.data_index_count)
        render_data.index_count = render_data.data_index_count;

    return data_buffer->Update(primitive,mat_inst->GetVIL());
}

SubMesh *DirectCreateSubMesh(Primitive *prim,MaterialInstance *mi,Pipeline *p)
//用Direct这个前缀是为了区别于MeshManager/WorkObject/RenderFramework的::CreateSubMesh()
{
    if(!prim||!mi||!p)return(nullptr);

    const VIL *vil=mi->GetVIL();

    if(*vil!=*p->GetVIL())
        return(nullptr);

    const uint32_t input_count=vil->GetVertexAttribCount(VertexInputGroup::Basic);       //不统计Bone/LocalToWorld组的
    const AnsiString &mtl_name=mi->GetMaterial()->GetName();

    if(prim->GetVABCount()<input_count)        //小于材质要求的数量？那自然是不行的
    {
        GLogError("[FATAL ERROR] input buffer count of SubMesh lesser than Material, Material name: "+mtl_name);

        return(nullptr);
    }

    MeshDataBuffer *mesh_data_buffer=new MeshDataBuffer(input_count,prim->GetIBO(),prim->GetVDM());

    const VertexInputFormat *vif=vil->GetVIFList(VertexInputGroup::Basic);

    VAB *vab;

    for(uint i=0;i<input_count;i++)
    {
        //注: VIF来自于材质，但VAB来自于Primitive。
        //    两个并不一定一样，排序也不一定一样。所以不能让PRIMTIVE直接提供BUFFER_LIST/OFFSET来搞一次性绑定。

        vab=prim->GetVAB(vif->name);

        if(!vab)
        {
            GLogError("[FATAL ERROR] not found VAB \""+AnsiString(vif->name)+"\" in Material: "+mtl_name);
            return(nullptr);
        }

        if(vab->GetFormat()!=vif->format)
        {
            GLogError(  "[FATAL ERROR] VAB \""+AnsiString(vif->name)+
                        AnsiString("\" format can't match SubMesh, Material(")+mtl_name+
                        AnsiString(") Format(")+GetVulkanFormatName(vif->format)+
                        AnsiString(") , VAB Format(")+GetVulkanFormatName(vab->GetFormat())+
                        ")");
            return(nullptr);
        }

        if(vab->GetStride()!=vif->stride)
        {
            GLogError(  "[FATAL ERROR] VAB \""+AnsiString(vif->name)+
                        AnsiString("\" stride can't match SubMesh, Material(")+mtl_name+
                        AnsiString(") stride(")+AnsiString::numberOf(vif->stride)+
                        AnsiString(") , VAB stride(")+AnsiString::numberOf(vab->GetStride())+
                        ")");
            return(nullptr);
        }

        mesh_data_buffer->vab_list[i]=vab->GetBuffer();
        mesh_data_buffer->vab_offset[i]=0;
        ++vif;
    }

    return(new SubMesh(prim,mi,p,mesh_data_buffer));
}

bool MeshDataBuffer::Update(const Primitive *prim,const VIL *vil)
{
    if(!prim||!vil)
        return(false);

    ibo=prim->GetIBO();
    vdm=prim->GetVDM();

    const VertexInputFormat *vif=vil->GetVIFList(VertexInputGroup::Basic);

    for(uint i=0;i<vab_count;i++)
    {
        vab_list[i]=prim->GetVkBuffer(vif->name);
        vab_offset[i]=0;

        ++vif;
    }

    return(true);
}

// SubMesh draw control APIs
bool SubMesh::SetDrawCounts(uint32_t draw_vertex_count,uint32_t draw_index_count)
{
    // only clamp, do not change offsets
    if(draw_vertex_count>render_data.data_vertex_count)
        draw_vertex_count = render_data.data_vertex_count;

    if(draw_index_count>render_data.data_index_count)
        draw_index_count = render_data.data_index_count;

    render_data.vertex_count = draw_vertex_count;
    render_data.index_count  = draw_index_count;

    return true;
}

bool SubMesh::SetDrawRange(int32_t vertex_offset,uint32_t first_index,uint32_t draw_vertex_count,uint32_t draw_index_count)
{
    // set offsets
    render_data.vertex_offset = vertex_offset;
    render_data.first_index   = first_index;

    // clamp counts to data counts
    if(draw_vertex_count>render_data.data_vertex_count)
        draw_vertex_count = render_data.data_vertex_count;

    if(draw_index_count>render_data.data_index_count)
        draw_index_count = render_data.data_index_count;

    render_data.vertex_count = draw_vertex_count;
    render_data.index_count  = draw_index_count;

    return true;
}

VK_NAMESPACE_END
