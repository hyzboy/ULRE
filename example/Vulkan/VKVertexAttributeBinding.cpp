#include"VKVertexAttributeBinding.h"
#include"VKShader.h"

VK_NAMESPACE_BEGIN
VertexAttributeBinding::VertexAttributeBinding(Shader *s)
{
    shader=s;

    const int count=shader->GetAttrCount();

    if(count<=0)
    {
        binding_list=nullptr;
        return;
    }

    binding_list=hgl_copy_new(count,shader->GetDescList());
}

VertexAttributeBinding::~VertexAttributeBinding()
{
    delete[] binding_list;

    shader->Release(this);
}

bool VertexAttributeBinding::SetInstance(const uint index,bool instance)
{
    if(index>=shader->GetAttrCount())return(false);

    binding_list[index].inputRate=instance?VK_VERTEX_INPUT_RATE_INSTANCE:VK_VERTEX_INPUT_RATE_VERTEX;

    return(true);
}

bool VertexAttributeBinding::SetInstance(const UTF8String &name,bool instance)
{
    return SetInstance(shader->GetBinding(name),instance);
}

void VertexAttributeBinding::Write(VkPipelineVertexInputStateCreateInfo &vis_create_info) const
{
    vis_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    const uint32_t count=shader->GetAttrCount();

    vis_create_info.vertexBindingDescriptionCount = count;
    vis_create_info.pVertexBindingDescriptions = binding_list;

    vis_create_info.vertexAttributeDescriptionCount = count;
    vis_create_info.pVertexAttributeDescriptions = shader->GetAttrList();
}
VK_NAMESPACE_END
