﻿#include<hgl/graph/VKRenderable.h>
#include<hgl/graph/VKMaterialInstance.h>
#include<hgl/graph/VKMaterialParameters.h>
#include<hgl/graph/VKMaterial.h>
#include<hgl/graph/VKVertexAttribBuffer.h>
#include<hgl/graph/VKIndexBuffer.h>
#include<hgl/log/LogInfo.h>

VK_NAMESPACE_BEGIN
PrimitiveDataBuffer::PrimitiveDataBuffer(const uint32_t c,const uint32_t vc, IndexBuffer *ib)
{
    vab_count=c;

    vab_list=hgl_zero_new<VkBuffer>(vab_count);
    ibo=ib;
}

PrimitiveDataBuffer::~PrimitiveDataBuffer()
{
    delete[] vab_list;
}

const bool PrimitiveDataBuffer::Comp(const PrimitiveDataBuffer *prb)const
{
    if(!prb)return(false);

    if(vab_count!=prb->vab_count)return(false);

    for(uint32_t i=0;i<vab_count;i++)
    {
        if(vab_list[i]!=prb->vab_list[i])return(false);
    }

    if(ibo!=prb->ibo)
        return(false);

    return(true);
}

Renderable::Renderable(Primitive *r,MaterialInstance *mi,Pipeline *p,PrimitiveDataBuffer *prb,PrimitiveRenderData *prd)
{
    primitive=r;
    pipeline=p;
    mat_inst=mi;

    primitive_data_buffer=prb;
    primitive_render_data=prd;
}

PrimitiveRenderData::PrimitiveRenderData(const uint32_t bc,const uint32_t vc)
{
    vab_count=bc;

    vertex_count=vc;
    vertex_offset=0;

    first_index=0;
}

PrimitiveRenderData::~PrimitiveRenderData()
{
}

const bool PrimitiveRenderData::Comp(const PrimitiveRenderData *prd)const
{
    if(!prd)return(false);
    
    return !memcmp(this,prd,sizeof(PrimitiveRenderData));
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

    PrimitiveDataBuffer *prb=new PrimitiveDataBuffer(input_count,prim->GetVertexCount(),prim->GetIBO());
    PrimitiveRenderData *prd=new PrimitiveRenderData(input_count,prim->GetVertexCount());

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

        prb->vab_offset[i]=vab_access->start;
        prb->vab_list[i]=vab_access->vab->GetBuffer();
        ++vif;
    }

    return(new Renderable(prim,mi,p,prb,prd));
}
VK_NAMESPACE_END
