#include"VKVertexInput.h"
#include"VKBuffer.h"

VK_NAMESPACE_BEGIN
bool VertexInput::Add(uint32_t location,VertexBuffer *buf,bool instance,VkDeviceSize offset)
{
    if(!buf)
        return(false);

    const int binding_index=vib_list.GetCount();        //参考opengl vab,binding_index必须从0开始，紧密排列。对应在vkCmdBindVertexBuffer中的缓冲区索引

    VkVertexInputBindingDescription binding;
    VkVertexInputAttributeDescription attrib;

    binding.binding=binding_index;
    binding.stride=buf->GetStride();
    binding.inputRate=instance?VK_VERTEX_INPUT_RATE_INSTANCE:VK_VERTEX_INPUT_RATE_VERTEX;

    attrib.binding=binding_index;
    attrib.location=location;
    attrib.format=buf->GetFormat();
    attrib.offset=offset;

    vib_list.Add(new VertexInputBuffer(binding,attrib,buf));
    buf_list.Add(buf->GetBuffer());
    buf_offset.Add(offset);

    binding_list.Add(binding);
    attribute_list.Add(attrib);

    return(true);
}

const VkPipelineVertexInputStateCreateInfo VertexInput::GetPipelineVertexInputStateCreateInfo()const
{
    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    vertexInputInfo.vertexBindingDescriptionCount = binding_list.GetCount();
    vertexInputInfo.pVertexBindingDescriptions = binding_list.GetData();

    vertexInputInfo.vertexAttributeDescriptionCount = attribute_list.GetCount();
    vertexInputInfo.pVertexAttributeDescriptions = attribute_list.GetData();

    return vertexInputInfo;
}
VK_NAMESPACE_END
