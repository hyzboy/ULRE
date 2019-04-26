#include"VKVertexInput.h"
#include"VKBuffer.h"
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

VertexInput::VertexInput(const Shader *s)
{
    shader=s;

    if(!shader)
        return;

    buf_count=shader->GetAttrCount();

    buf_list=hgl_zero_new<VkBuffer>(buf_count);
    buf_offset=hgl_zero_new<VkDeviceSize>(buf_count);
}

VertexInput::~VertexInput()
{
    delete[] buf_offset;
    delete[] buf_list;
}

bool VertexInput::Set(const int index,VertexBuffer *buf,VkDeviceSize offset)
{
    if(index<0||index>=buf_count)return(false);

    const VkVertexInputBindingDescription *desc=shader->GetDesc(index);   
    const VkVertexInputAttributeDescription *attr=shader->GetAttr(index);

    if(buf->GetFormat()!=attr->format)return(false);
    if(buf->GetStride()!=desc->stride)return(false);

    buf_list[index]=*buf;
    buf_offset[index]=offset;

    return(true);
}

bool VertexInput::Set(const UTF8String &name,VertexBuffer *vb,VkDeviceSize offset)
{
    return Set(shader->GetBinding(name),vb,offset);
}
VK_NAMESPACE_END
