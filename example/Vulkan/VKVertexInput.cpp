#include"VKVertexInput.h"

VK_NAMESPACE_BEGIN

//struct VertexInputBuffer
//{
//    //按API，可以一个binding绑多个attrib，但我们仅支持1v1
//
//    VkVertexInputBindingDescription binding;
//    VkVertexInputAttributeDescription attrib;
//    Buffer *buf;
//};

bool VertexInput::Add(VertexBuffer *buf)
{
    if(!buf)
        return(false);

    const int binding_index=vib_list.GetCount();      //参考opengl vab,binding_index必须从0开始，紧密排列，但是否必须这样，待以后测试

    VkVertexInputBindingDescription binding;
    VkVertexInputAttributeDescription attrib;

    binding.binding=binding_index;
    binding.stride=buf->GetStride();
    binding.inputRate=VK_VERTEX_INPUT_RATE_VERTEX;      //还有一种是INSTANCE，暂时未知

    attrib.binding=binding_index;
    attrib.location=0;
    attrib.format=buf->GetFormat();
    attrib.offset=0;

    vib_list.Add(new VertexInputBuffer(binding,attrib,buf));
    buf_list.Add(buf->GetBuffer());

    return(true);
}
VK_NAMESPACE_END
