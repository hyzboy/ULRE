#include<hgl/vk/VKVertexInputLayout.h>

namespace hgl::graph{
VertexInputLayout::VertexInputLayout(const uint32_t c)
{
    count=c;

    bind_list=zero_new<VkVertexInputBindingDescription>(count);
    attr_list=zero_new<VkVertexInputAttributeDescription>(count);

    vif_list=zero_new<VertexInputFormat>(count);
}

VertexInputLayout::~VertexInputLayout()
{
    delete[] vif_list;
    delete[] bind_list;
    delete[] attr_list;
}

const int VertexInputLayout::GetIndex(const VertexAttrib attrib)const
{
    for(uint32_t i=0;i<count;i++)
        if(vif_list[i].attrib==attrib)
            return(i);

    return -1;
}
}//namespace hgl::graph
