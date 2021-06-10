#include<hgl/graph/VKRenderableInstance.h>
#include<hgl/graph/VKMaterialInstance.h>
#include<hgl/graph/VKMaterial.h>
#include<hgl/graph/VKVertexAttribBuffer.h>
#include<hgl/util/hash/Hash.h>

VK_NAMESPACE_BEGIN

using namespace util;

RenderableInstance::RenderableInstance(Renderable *r,MaterialInstance *mi,Pipeline *p,const uint32_t count,VkBuffer *bl,VkDeviceSize *bs)
{
    render_obj=r;
    mat_inst=mi;
    pipeline=p;

    descriptor_sets=nullptr;

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
    SAFE_CLEAR(descriptor_sets);
    //需要在这里添加删除pipeline/desc_sets/render_obj引用计数的代码

    delete[] buffer_list;
    delete[] buffer_size;
}

RenderableInstance *CreateRenderableInstance(Renderable *r,MaterialInstance *mi,Pipeline *p)
{
    if(!r||!mi||!p)return(nullptr);

    Material *mtl=mi->GetMaterial();

    if(!mtl)return(nullptr);

    const VertexShaderModule *vsm=mtl->GetVertexShaderModule();
    const ShaderStageList &ssl=vsm->GetStageInputs();
    const int input_count=ssl.GetCount();

    if(r->GetBufferCount()<input_count)        //小于材质要求的数量？那自然是不行的
    {
        LOG_ERROR("[FATAL ERROR] input buffer count of Renderable lesser than Material, Material name: "+mtl->GetName());

        return(nullptr);
    }

    AutoDeleteArray<VkBuffer> buffer_list(input_count);
    AutoDeleteArray<VkDeviceSize> buffer_size(input_count);

    ShaderStage **ss=ssl.GetData();

    VAB *vab;
    const VkVertexInputBindingDescription *desc;
    const VkVertexInputAttributeDescription *attr;

    for(int i=0;i<input_count;i++)
    {
        desc=vsm->GetDesc(i);
        attr=vsm->GetAttr(i);

        vab=r->GetVAB((*ss)->name,buffer_size+i);

        if(!vab)
        {
            LOG_ERROR("[FATAL ERROR] can't find VAB \""+(*ss)->name+"\" in Material: "+mtl->GetName());
            return(nullptr);
        }

        if(vab->GetFormat()!=attr->format)
        {
            LOG_ERROR(  "[FATAL ERROR] VAB \""+(*ss)->name+
                        UTF8String("\" format can't match Renderable, Material(")+mtl->GetName()+
                        UTF8String(") Format(")+GetVulkanFormatName(attr->format)+
                        UTF8String("), VAB Format(")+GetVulkanFormatName(vab->GetFormat())+
                        ")");
            return(nullptr);
        }

        if(vab->GetStride()!=desc->stride)
        {
            LOG_ERROR(  "[FATAL ERROR] VAB \""+(*ss)->name+
                        UTF8String("\" stride can't match Renderable, Material(")+mtl->GetName()+
                        UTF8String(") stride(")+UTF8String::valueOf(desc->stride)+
                        UTF8String("), VAB stride(")+UTF8String::valueOf(vab->GetStride())+
                        ")");
            return(nullptr);
        }

        buffer_list[i]=vab->GetBuffer();

        ++ss;
    }

    RenderableInstance *ri=new RenderableInstance(r,mi,p,input_count,buffer_list,buffer_size);
    buffer_list.Discard();
    buffer_size.Discard();
    return ri;
}
VK_NAMESPACE_END
