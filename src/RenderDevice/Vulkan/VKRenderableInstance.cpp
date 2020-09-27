#include<hgl/graph/vulkan/VKRenderableInstance.h>
#include<hgl/graph/vulkan/VKMaterialInstance.h>
#include<hgl/graph/vulkan/VKMaterial.h>

VK_NAMESPACE_BEGIN
RenderableInstance::RenderableInstance(Renderable *r,MaterialInstance *mi,Pipeline *p,const uint32_t count,VkBuffer *bl,VkDeviceSize *bs)
{
    render_obj=r;
    mat_inst=mi;
    pipeline=p;

    buffer_count=count;
    buffer_list=bl;
    buffer_size=bs;
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

    Material *mtl=mi->GetMaterial();

    if(!mtl)return(nullptr);

    const VertexShaderModule *vsm=mtl->GetVertexShaderModule();
    const ShaderStageList &ssl=vsm->GetStageInputs();
    const int input_count=ssl.GetCount();

    if(r->GetBufferCount()<input_count)        //小于材质要求的数量？那自然是不行的
        return(nullptr);

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

        if(!vab)return(nullptr);

        if(vab->GetFormat()!=attr->format)return(nullptr);
        if(vab->GetStride()!=desc->stride)return(nullptr);

        buffer_list[i]=vab->GetBuffer();

        ++ss;
    }

    RenderableInstance *ri=new RenderableInstance(r,mi,p,input_count,buffer_list,buffer_size);
    buffer_list.Discard();
    buffer_size.Discard();
    return ri;
}
VK_NAMESPACE_END