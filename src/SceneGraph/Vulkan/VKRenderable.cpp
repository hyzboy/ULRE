#include<hgl/graph/VKRenderable.h>
#include<hgl/graph/VKMaterialInstance.h>
#include<hgl/graph/VKMaterialParameters.h>
#include<hgl/graph/VKMaterial.h>
#include<hgl/graph/VKVertexAttribBuffer.h>
#include<hgl/util/hash/Hash.h>

VK_NAMESPACE_BEGIN
using namespace util;

VertexInputData::VertexInputData(const VIL *vil)
{
    count=vil->GetAttrCount();

    name_list=vil->GetNameList();
    bind_list=vil->GetBindingList();
    attr_list=vil->GetAttributeList();

    buffer_list=new VkBuffer[count];
    buffer_offset=new VkDeviceSize[count];
}

VertexInputData::~VertexInputData()
{
    delete[] buffer_list;
    delete[] buffer_offset;
}

Renderable::Renderable(Primitive *r,MaterialInstance *mi,Pipeline *p,VertexInputData *vid)
{
    primitive=r;
    pipeline=p;
    mat_inst=mi;

    vertex_input_data=vid;

    if(vertex_input_data->count>0)
        CountHash<HASH::Adler32>(vertex_input_data->buffer_list,vertex_input_data->count*sizeof(VkBuffer),(void *)&buffer_hash);
    else
        buffer_hash=0;
}
 
Renderable::~Renderable()
{
    //需要在这里添加删除pipeline/desc_sets/primitive引用计数的代码

    delete vertex_input_data;
}

Renderable *CreateRenderable(Primitive *r,MaterialInstance *mi,Pipeline *p)
{
    if(!r||!mi||!p)return(nullptr);

    const VIL *vil=mi->GetVIL();
    const int input_count=vil->GetAttrCount();
    const UTF8String &mtl_name=mi->GetMaterial()->GetName();

    if(r->GetBufferCount()<input_count)        //小于材质要求的数量？那自然是不行的
    {
        LOG_ERROR("[FATAL ERROR] input buffer count of Renderable lesser than Material, Material name: "+mtl_name);

        return(nullptr);
    }

    VBO *vbo;
    const char *name;

    VertexInputData *vid=new VertexInputData(vil);

    for(int i=0;i<input_count;i++)
    {
        vbo=r->GetVBO(vid->name_list[i],vid->buffer_offset+i);

        name=vid->name_list[i];

        if(!vbo)
        {
            LOG_ERROR("[FATAL ERROR] not found VBO \""+AnsiString(vid->name_list[i])+"\" in Material: "+mtl_name);
            return(nullptr);
        }

        if(vbo->GetFormat()!=vid->attr_list[i].format)
        {
            LOG_ERROR(  "[FATAL ERROR] VBO \""+UTF8String(name)+
                        UTF8String("\" format can't match Renderable, Material(")+mtl_name+
                        UTF8String(") Format(")+GetVulkanFormatName(vid->attr_list[i].format)+
                        UTF8String("), VBO Format(")+GetVulkanFormatName(vbo->GetFormat())+
                        ")");
            delete vid;
            return(nullptr);
        }

        if(vbo->GetStride()!=vid->bind_list[i].stride)
        {
            LOG_ERROR(  "[FATAL ERROR] VBO \""+UTF8String(name)+
                        UTF8String("\" stride can't match Renderable, Material(")+mtl_name+
                        UTF8String(") stride(")+UTF8String::numberOf(vid->bind_list[i].stride)+
                        UTF8String("), VBO stride(")+UTF8String::numberOf(vbo->GetStride())+
                        ")");
            delete vid;
            return(nullptr);
        }

        vid->buffer_list[i]=vbo->GetBuffer();
    }

    return(new Renderable(r,mi,p,vid));
}
VK_NAMESPACE_END
