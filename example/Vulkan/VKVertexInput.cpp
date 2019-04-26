#include"VKVertexInput.h"
#include"VKBuffer.h"
#include"VKShader.h"

VK_NAMESPACE_BEGIN
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
