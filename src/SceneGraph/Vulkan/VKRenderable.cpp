#include<hgl/graph/VKRenderable.h>
#include<hgl/graph/VKMaterialInstance.h>
#include<hgl/graph/VKMaterialParameters.h>
#include<hgl/graph/VKMaterial.h>
#include<hgl/graph/VKVertexAttribBuffer.h>
#include<hgl/log/LogInfo.h>

VK_NAMESPACE_BEGIN
VertexInputData::VertexInputData(const uint32_t c,const uint32_t vc,const IBAccess *iba)
{
    binding_count=c;

    buffer_list=new VkBuffer[binding_count];
    buffer_offset=new VkDeviceSize[binding_count];

    vertex_count=vc;

    if(!iba||!iba->buffer)
        ib_access=nullptr;
    else
        ib_access=iba;
}

VertexInputData::~VertexInputData()
{
    delete[] buffer_list;
    delete[] buffer_offset;
}

Renderable::Renderable(Primitive *r,MaterialInstance *mi,Pipeline *p,VertexInputData *vi)
{
    primitive=r;
    pipeline=p;
    mat_inst=mi;

    vertex_input=vi;
}
 
Renderable::~Renderable()
{
    //需要在这里添加删除pipeline/desc_sets/primitive引用计数的代码

    delete vertex_input;
}

Renderable *CreateRenderable(Primitive *prim,MaterialInstance *mi,Pipeline *p)
{
    if(!prim||!mi||!p)return(nullptr);

    const VIL *vil=mi->GetVIL();
    const uint32_t input_count=vil->GetCount(VertexInputGroup::Basic);       //不统计Bone/LocalToWorld组的
    const UTF8String &mtl_name=mi->GetMaterial()->GetName();

    if(prim->GetVACount()<input_count)        //小于材质要求的数量？那自然是不行的
    {
        LOG_ERROR("[FATAL ERROR] input buffer count of Renderable lesser than Material, Material name: "+mtl_name);

        return(nullptr);
    }

    VAB *vab;

    VertexInputData *vid=new VertexInputData(input_count,prim->GetVertexCount(),prim->GetIBAccess());

    const VertexInputFormat *vif=vil->GetVIFList(VertexInputGroup::Basic);
    VABAccess vad;

    for(uint i=0;i<input_count;i++)
    {
        //注: VIF来自于材质，但VAB来自于Primitive。
        //    两个并不一定一样，排序也不一定一样。所以不能让PRIMTIVE直接提供BUFFER_LIST/OFFSET来搞一次性绑定。

        if(!prim->GetVABAccess(vif->name,&vad))
        {
            LOG_ERROR("[FATAL ERROR] not found VAB \""+AnsiString(vif->name)+"\" in Material: "+mtl_name);
            return(nullptr);
        }

        vab=vad.vab;

        if(vab->GetFormat()!=vif->format)
        {
            LOG_ERROR(  "[FATAL ERROR] VAB \""+UTF8String(vif->name)+
                        UTF8String("\" format can't match Renderable, Material(")+mtl_name+
                        UTF8String(") Format(")+GetVulkanFormatName(vif->format)+
                        UTF8String("), VAB Format(")+GetVulkanFormatName(vab->GetFormat())+
                        ")");
            return(nullptr);
        }

        if(vab->GetStride()!=vif->stride)
        {
            LOG_ERROR(  "[FATAL ERROR] VAB \""+UTF8String(vif->name)+
                        UTF8String("\" stride can't match Renderable, Material(")+mtl_name+
                        UTF8String(") stride(")+UTF8String::numberOf(vif->stride)+
                        UTF8String("), VAB stride(")+UTF8String::numberOf(vab->GetStride())+
                        ")");
            return(nullptr);
        }

        vid->buffer_offset[i]=vad.start;
        vid->buffer_list[i]=vab->GetBuffer();
        ++vif;
    }

    return(new Renderable(prim,mi,p,vid));
}
VK_NAMESPACE_END
