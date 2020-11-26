#include<hgl/graph/VKVertexAttributeBinding.h>
#include<hgl/graph/VKShaderModule.h>

VK_NAMESPACE_BEGIN
VertexAttributeBinding::VertexAttributeBinding(const uint32_t count,const VkVertexInputBindingDescription *bind_list,const VkVertexInputAttributeDescription *attr_list)
{
    attr_count=count;

    if(attr_count<=0)
    {
        binding_list=nullptr;
        attribute_list=nullptr;
        return;
    }

    binding_list=hgl_copy_new(attr_count,bind_list);
    attribute_list=hgl_copy_new(attr_count,attr_list);
}

VertexAttributeBinding::~VertexAttributeBinding()
{
    delete[] attribute_list;
    delete[] binding_list;
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
VK_NAMESPACE_END
