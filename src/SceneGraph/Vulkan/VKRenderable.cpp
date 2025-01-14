#include<hgl/graph/VKRenderable.h>
#include<hgl/graph/VKMaterialInstance.h>
#include<hgl/graph/VKMaterialParameters.h>
#include<hgl/graph/VKMaterial.h>
#include<hgl/graph/VKVertexAttribBuffer.h>
#include<hgl/graph/VKIndexBuffer.h>
#include<hgl/log/LogInfo.h>

VK_NAMESPACE_BEGIN
PrimitiveDataBuffer::PrimitiveDataBuffer(const uint32_t c,IndexBuffer *ib,VertexDataManager *_vdm)
{
    vab_count=c;

    vab_list=hgl_zero_new<VkBuffer>(vab_count);
    vab_offset=hgl_zero_new<VkDeviceSize>(vab_count);
    ibo=ib;

    vdm=_vdm;
}

PrimitiveDataBuffer::~PrimitiveDataBuffer()
{
    delete[] vab_offset;
    delete[] vab_list;
}

const int PrimitiveDataBuffer::compare(const PrimitiveDataBuffer &pdb)const
{
    ptrdiff_t off;

    off=&vdm-&pdb.vdm;
    if(off)
        return off;

    off=vab_count-pdb.vab_count;
    if(off)
        return off;

    off=hgl_cmp(vab_list,pdb.vab_list,vab_count);
    if(off)
        return off;

    off=hgl_cmp(vab_offset,pdb.vab_offset,vab_count);
    if(off)
        return off;

    off=ibo-pdb.ibo;

    return off;
}

Renderable::Renderable(Primitive *r,MaterialInstance *mi,Pipeline *p,PrimitiveDataBuffer *pdb,PrimitiveRenderData *prd)
{
    primitive=r;
    pipeline=p;
    mat_inst=mi;

    primitive_data_buffer=pdb;
    primitive_render_data=prd;
}
 
Renderable *CreateRenderable(Primitive *prim,MaterialInstance *mi,Pipeline *p)
{
    if(!prim||!mi||!p)return(nullptr);

    const VIL *vil=mi->GetVIL();

    if(*vil!=*p->GetVIL())
        return(nullptr);

    const uint32_t input_count=vil->GetVertexAttribCount(VertexInputGroup::Basic);       //不统计Bone/LocalToWorld组的
    const AnsiString &mtl_name=mi->GetMaterial()->GetName();

    if(prim->GetVABCount()<input_count)        //小于材质要求的数量？那自然是不行的
    {
        LOG_ERROR("[FATAL ERROR] input buffer count of Renderable lesser than Material, Material name: "+mtl_name);

        return(nullptr);
    }

    PrimitiveDataBuffer *pdb=new PrimitiveDataBuffer(input_count,prim->GetIBO(),prim->GetVDM());
    PrimitiveRenderData *prd=new PrimitiveRenderData(prim->GetVertexCount(),prim->GetIndexCount(),prim->GetVertexOffset(),prim->GetFirstIndex());

    const VertexInputFormat *vif=vil->GetVIFList(VertexInputGroup::Basic);

    VAB *vab;

    for(uint i=0;i<input_count;i++)
    {
        //注: VIF来自于材质，但VAB来自于Primitive。
        //    两个并不一定一样，排序也不一定一样。所以不能让PRIMTIVE直接提供BUFFER_LIST/OFFSET来搞一次性绑定。

        vab=prim->GetVAB(vif->name);

        if(!vab)
        {
            LOG_ERROR("[FATAL ERROR] not found VAB \""+AnsiString(vif->name)+"\" in Material: "+mtl_name);
            return(nullptr);
        }

        if(vab->GetFormat()!=vif->format)
        {
            LOG_ERROR(  "[FATAL ERROR] VAB \""+AnsiString(vif->name)+
                        AnsiString("\" format can't match Renderable, Material(")+mtl_name+
                        AnsiString(") Format(")+GetVulkanFormatName(vif->format)+
                        AnsiString("), VAB Format(")+GetVulkanFormatName(vab->GetFormat())+
                        ")");
            return(nullptr);
        }

        if(vab->GetStride()!=vif->stride)
        {
            LOG_ERROR(  "[FATAL ERROR] VAB \""+AnsiString(vif->name)+
                        AnsiString("\" stride can't match Renderable, Material(")+mtl_name+
                        AnsiString(") stride(")+AnsiString::numberOf(vif->stride)+
                        AnsiString("), VAB stride(")+AnsiString::numberOf(vab->GetStride())+
                        ")");
            return(nullptr);
        }

        pdb->vab_list[i]=vab->GetBuffer();
        pdb->vab_offset[i]=0;
        ++vif;
    }

    return(new Renderable(prim,mi,p,pdb,prd));
}
VK_NAMESPACE_END
