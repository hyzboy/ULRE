#include<hgl/graph/VKRenderable.h>
#include<hgl/graph/VKMaterialInstance.h>
#include<hgl/graph/VKMaterialParameters.h>
#include<hgl/graph/VKMaterial.h>
#include<hgl/graph/VKVertexAttribBuffer.h>

VK_NAMESPACE_BEGIN
VertexInputDataGroup::VertexInputDataGroup(const VIL *vd)
{
    vil=vd;

    uint first_binding=0;

    for(uint i=0;i<size_t(VertexInputGroup::RANGE_SIZE);i++)
    {
        vid[i].binding_count=vil->GetCount(VertexInputGroup(i));

        if(vid[i].binding_count>0)
        {
            vid[i].first_binding=vil->GetFirstBinding(VertexInputGroup(i));

            vid[i].buffer_list=new VkBuffer[vid[i].binding_count];
            vid[i].buffer_offset=new VkDeviceSize[vid[i].binding_count];
        }
    }
}

VertexInputDataGroup::~VertexInputDataGroup()
{
    for(uint i=0;i<size_t(VertexInputGroup::RANGE_SIZE);i++)
        if(vid[i].binding_count>0)
        {
            delete[] vid[i].buffer_list;
            delete[] vid[i].buffer_offset;
        }
}

Renderable::Renderable(Primitive *r,MaterialInstance *mi,Pipeline *p,VertexInputDataGroup *vidg)
{
    primitive=r;
    pipeline=p;
    mat_inst=mi;

    vid_group=vidg;
}
 
Renderable::~Renderable()
{
    //需要在这里添加删除pipeline/desc_sets/primitive引用计数的代码

    delete vid_group;
}

Renderable *CreateRenderable(Primitive *prim,MaterialInstance *mi,Pipeline *p)
{
    if(!prim||!mi||!p)return(nullptr);

    const VIL *vil=mi->GetVIL();
    const uint input_count=vil->GetCount(VertexInputGroup::Basic);       //不统计Bone/LocalToWorld组的
    const UTF8String &mtl_name=mi->GetMaterial()->GetName();

    if(prim->GetBufferCount()<input_count)        //小于材质要求的数量？那自然是不行的
    {
        LOG_ERROR("[FATAL ERROR] input buffer count of Renderable lesser than Material, Material name: "+mtl_name);

        return(nullptr);
    }

    VBO *vbo;

    VertexInputDataGroup *vid_group=new VertexInputDataGroup(vil);

    VertexInputData &vid=vid_group->vid[size_t(VertexInputGroup::Basic)];

    const VertexInputFormat *vif=vil->GetFormatList(VertexInputGroup::Basic);

    for(uint i=0;i<input_count;i++)
    {
        vbo=prim->GetVBO(vif->name,vid.buffer_offset+i);

        if(!vbo)
        {
            LOG_ERROR("[FATAL ERROR] not found VBO \""+AnsiString(vif->name)+"\" in Material: "+mtl_name);
            return(nullptr);
        }

        if(vbo->GetFormat()!=vif->format)
        {
            LOG_ERROR(  "[FATAL ERROR] VBO \""+UTF8String(vif->name)+
                        UTF8String("\" format can't match Renderable, Material(")+mtl_name+
                        UTF8String(") Format(")+GetVulkanFormatName(vif->format)+
                        UTF8String("), VBO Format(")+GetVulkanFormatName(vbo->GetFormat())+
                        ")");
            return(nullptr);
        }

        if(vbo->GetStride()!=vif->stride)
        {
            LOG_ERROR(  "[FATAL ERROR] VBO \""+UTF8String(vif->name)+
                        UTF8String("\" stride can't match Renderable, Material(")+mtl_name+
                        UTF8String(") stride(")+UTF8String::numberOf(vif->stride)+
                        UTF8String("), VBO stride(")+UTF8String::numberOf(vbo->GetStride())+
                        ")");
            return(nullptr);
        }

        vid.buffer_list[i]=vbo->GetBuffer();
        ++vif;
    }

    return(new Renderable(prim,mi,p,vid_group));
}
VK_NAMESPACE_END
