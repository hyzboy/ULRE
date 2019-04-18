#include"VKVertexInput.h"

VK_NAMESPACE_BEGIN
bool VertexInput::Add(VertexBuffer *buf,bool instance)
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
    attrib.location=0;
    attrib.format=buf->GetFormat();
    attrib.offset=0;

    vib_list.Add(new VertexInputBuffer(binding,attrib,buf));
    buf_list.Add(buf->GetBuffer());

    return(true);
}
VK_NAMESPACE_END
