#include"VKVertexAttributeBinding.h"
#include"VKShaderModule.h"

VK_NAMESPACE_BEGIN
VertexAttributeBinding::VertexAttributeBinding(VertexShaderModule *s)
{
    vsm=s;

    attr_count=vsm->GetAttrCount();

    if(attr_count<=0)
    {
        binding_list=nullptr;
        attribute_list=nullptr;
        return;
    }

    binding_list=hgl_copy_new(attr_count,vsm->GetDescList());
    attribute_list=hgl_copy_new(attr_count,vsm->GetAttrList());
}

VertexAttributeBinding::~VertexAttributeBinding()
{
    delete[] attribute_list;
    delete[] binding_list;

    vsm->Release(this);
}

const uint VertexAttributeBinding::GetBinding(const UTF8String &name)
{
    return vsm->GetBinding(name);
}

bool VertexAttributeBinding::SetInstance(const uint index,bool instance)
{
    if(index>=attr_count)return(false);

    binding_list[index].inputRate=instance?VK_VERTEX_INPUT_RATE_INSTANCE:VK_VERTEX_INPUT_RATE_VERTEX;

    return(true);
}

bool VertexAttributeBinding::SetStride(const uint index,const uint32_t &stride)
{
    if(index>=attr_count)return(false);

    binding_list[index].stride=stride;

    return(true);
}

bool VertexAttributeBinding::SetFormat(const uint index,const VkFormat &format)
{
    if(index>=attr_count)return(false);

    attribute_list[index].format=format;

    return(true);
}

bool VertexAttributeBinding::SetOffset(const uint index,const uint32_t offset)
{
    if(index>=attr_count)return(false);

    attribute_list[index].offset=offset;

    return(true);
}

void VertexAttributeBinding::Write(VkPipelineVertexInputStateCreateInfo &vis_create_info) const
{
    vis_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    const uint32_t count=vsm->GetAttrCount();

    vis_create_info.vertexBindingDescriptionCount = count;
    vis_create_info.pVertexBindingDescriptions = binding_list;

    vis_create_info.vertexAttributeDescriptionCount = count;
    vis_create_info.pVertexAttributeDescriptions = attribute_list;
}
VK_NAMESPACE_END
