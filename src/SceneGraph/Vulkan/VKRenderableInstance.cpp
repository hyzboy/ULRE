#include<hgl/graph/VKRenderableInstance.h>
#include<hgl/graph/VKMaterialInstance.h>
#include<hgl/graph/VKMaterialParameters.h>
#include<hgl/graph/VKMaterial.h>
#include<hgl/graph/VKVertexAttribBuffer.h>
#include<hgl/util/hash/Hash.h>

VK_NAMESPACE_BEGIN
using namespace util;

RenderableInstance::RenderableInstance(Renderable *r,MaterialInstance *mi,Pipeline *p,const uint32_t count,VkBuffer *bl,VkDeviceSize *bs)
{
    render_obj=r;
    pipeline=p;
    mat_inst=mi;

    buffer_count=count;
    buffer_list=bl;
    buffer_size=bs;

    if(buffer_count>0)
        CountHash<HASH::Adler32>(buffer_list,buffer_count*sizeof(VkBuffer),(void *)&buffer_hash);
    else
        buffer_hash=0;
}
 
RenderableInstance::~RenderableInstance()
{
    //需要在这里添加删除pipeline/desc_sets/render_obj引用计数的代码

    delete[] buffer_list;
    delete[] buffer_size;
}

RenderableInstance *CreateRenderableInstance(Renderable *r,MaterialInstance *mi,Pipeline *p)
{
    if(!r||!mi||!p)return(nullptr);

    const VAB *vab=mi->GetVAB();
    const int input_count=vab->GetVertexAttrCount();
    const UTF8String &mtl_name=mi->GetMaterial()->GetName();

    if(r->GetBufferCount()<input_count)        //小于材质要求的数量？那自然是不行的
    {
        LOG_ERROR("[FATAL ERROR] input buffer count of Renderable lesser than Material, Material name: "+mtl_name);

        return(nullptr);
    }

    AutoDeleteArray<VkBuffer> buffer_list(input_count);
    AutoDeleteArray<VkDeviceSize> buffer_size(input_count);

    VBO *vbo;
    const AnsiString **                         name_list=vab->GetVertexNameList();
    const VkVertexInputBindingDescription *     bind_list=vab->GetVertexBindingList();
    const VkVertexInputAttributeDescription *   attr_list=vab->GetVertexAttributeList();

    for(int i=0;i<input_count;i++)
    {
        vbo=r->GetVBO(**name_list,buffer_size+i);

        if(!vbo)
        {
            LOG_ERROR("[FATAL ERROR] can't find VBO \""+**name_list+"\" in Material: "+mtl_name);
            return(nullptr);
        }

        if(vbo->GetFormat()!=attr_list->format)
        {
            LOG_ERROR(  "[FATAL ERROR] VBO \""+**name_list+
                        UTF8String("\" format can't match Renderable, Material(")+mtl_name+
                        UTF8String(") Format(")+GetVulkanFormatName(attr_list->format)+
                        UTF8String("), VBO Format(")+GetVulkanFormatName(vbo->GetFormat())+
                        ")");
            return(nullptr);
        }

        if(vbo->GetStride()!=bind_list->stride)
        {
            LOG_ERROR(  "[FATAL ERROR] VBO \""+**name_list+
                        UTF8String("\" stride can't match Renderable, Material(")+mtl_name+
                        UTF8String(") stride(")+UTF8String::valueOf(bind_list->stride)+
                        UTF8String("), VBO stride(")+UTF8String::valueOf(vbo->GetStride())+
                        ")");
            return(nullptr);
        }

        buffer_list[i]=vbo->GetBuffer();

        ++name_list;
        ++bind_list;
        ++attr_list;
    }

    RenderableInstance *ri=new RenderableInstance(r,mi,p,input_count,buffer_list,buffer_size);
    buffer_list.Discard();
    buffer_size.Discard();
    return ri;
}
VK_NAMESPACE_END
