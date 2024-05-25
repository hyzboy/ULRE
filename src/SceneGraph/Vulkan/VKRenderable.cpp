#include<hgl/graph/VKRenderable.h>
#include<hgl/graph/VKMaterialInstance.h>
#include<hgl/graph/VKMaterialParameters.h>
#include<hgl/graph/VKMaterial.h>
#include<hgl/graph/VKVertexAttribBuffer.h>
#include<hgl/graph/VKIndexBuffer.h>
#include<hgl/log/LogInfo.h>

VK_NAMESPACE_BEGIN
VertexInputData::VertexInputData(const uint32_t c,const uint32_t vc,const IBAccess *iba)
{
    vab_count=c;

    vab_list=hgl_zero_new<VkBuffer>(vab_count);

    if(iba&&iba->buffer)
        ibo=iba->buffer;
    else
        ibo=nullptr;
}

VertexInputData::~VertexInputData()
{
    delete[] vab_list;
}

const bool VertexInputData::Comp(const VertexInputData *vid)const
{
    if(!vid)return(false);

    if(vab_count!=vid->vab_count)return(false);

    for(uint32_t i=0;i<vab_count;i++)
    {
        if(vab_list[i]!=vid->vab_list[i])return(false);
    }

    if(ibo!=vid->ibo)return(false);

    return(true);
}

Renderable::Renderable(Primitive *r,MaterialInstance *mi,Pipeline *p,VertexInputData *vid,DrawData *dd)
{
    primitive=r;
    pipeline=p;
    mat_inst=mi;

    vertex_input_data=vid;
    draw_data=dd;
}

DrawData::DrawData(const uint32_t bc,const VkDeviceSize vc,const IBAccess *iba)
{
    vab_count=bc;

    vab_offset=new VkDeviceSize[vab_count];

    vertex_count=vc;

    if(iba&&iba->buffer)
    {
        index_start=iba->start;
        index_count=iba->count;
    }
}

DrawData::~DrawData()
{
    delete[] vab_offset;
}

const bool DrawData::Comp(const DrawData *dd)const
{
    if(!dd)return(false);
    
    if(vab_count!=dd->vab_count)return(false);

    for(uint i=0;i<vab_count;i++)
    {
        if(vab_offset[i]!=dd->vab_offset[i])return(false);
    }

    if(vertex_count!=dd->vertex_count)return(false);

    if(index_start!=dd->index_start)return(false);
    if(index_count!=dd->index_count)return(false);

    return(true);
}
 
Renderable *CreateRenderable(Primitive *prim,MaterialInstance *mi,Pipeline *p)
{
    if(!prim||!mi||!p)return(nullptr);

    const VIL *vil=mi->GetVIL();
    const uint32_t input_count=vil->GetCount(VertexInputGroup::Basic);       //不统计Bone/LocalToWorld组的
    const UTF8String &mtl_name=mi->GetMaterial()->GetName();

    if(prim->GetVABCount()<input_count)        //小于材质要求的数量？那自然是不行的
    {
        LOG_ERROR("[FATAL ERROR] input buffer count of Renderable lesser than Material, Material name: "+mtl_name);

        return(nullptr);
    }

    const IBAccess *iba=prim->GetIBAccess();

    VertexInputData *vid=new VertexInputData(input_count,prim->GetVertexCount(),iba);
    DrawData *dd=new DrawData(input_count,prim->GetVertexCount(),iba);

    const VertexInputFormat *vif=vil->GetVIFList(VertexInputGroup::Basic);
    VABAccess *vab_access;

    for(uint i=0;i<input_count;i++)
    {
        //注: VIF来自于材质，但VAB来自于Primitive。
        //    两个并不一定一样，排序也不一定一样。所以不能让PRIMTIVE直接提供BUFFER_LIST/OFFSET来搞一次性绑定。

        vab_access=prim->GetVABAccess(vif->name);

        if(!vab_access||!vab_access->vab)
        {
            LOG_ERROR("[FATAL ERROR] not found VAB \""+AnsiString(vif->name)+"\" in Material: "+mtl_name);
            return(nullptr);
        }

        if(vab_access->vab->GetFormat()!=vif->format)
        {
            LOG_ERROR(  "[FATAL ERROR] VAB \""+UTF8String(vif->name)+
                        UTF8String("\" format can't match Renderable, Material(")+mtl_name+
                        UTF8String(") Format(")+GetVulkanFormatName(vif->format)+
                        UTF8String("), VAB Format(")+GetVulkanFormatName(vab_access->vab->GetFormat())+
                        ")");
            return(nullptr);
        }

        if(vab_access->vab->GetStride()!=vif->stride)
        {
            LOG_ERROR(  "[FATAL ERROR] VAB \""+UTF8String(vif->name)+
                        UTF8String("\" stride can't match Renderable, Material(")+mtl_name+
                        UTF8String(") stride(")+UTF8String::numberOf(vif->stride)+
                        UTF8String("), VAB stride(")+UTF8String::numberOf(vab_access->vab->GetStride())+
                        ")");
            return(nullptr);
        }

        dd->vab_offset[i]=vab_access->start*vif->stride;
        vid->vab_list[i]=vab_access->vab->GetBuffer();
        ++vif;
    }

    return(new Renderable(prim,mi,p,vid,dd));
}
VK_NAMESPACE_END
