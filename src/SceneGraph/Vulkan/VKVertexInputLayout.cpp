#include<hgl/graph/VKVertexInputLayout.h>

VK_NAMESPACE_BEGIN
VertexInputLayout::VertexInputLayout(const uint32_t c)
{
    count=c;

    bind_list=hgl_zero_new<VkVertexInputBindingDescription>(count);
    attr_list=hgl_zero_new<VkVertexInputAttributeDescription>(count);

    vif_list=hgl_zero_new<VertexInputFormat>(count);
    mem_zero(vif_list_by_group);

    mem_zero(count_by_group);
    mem_zero(first_binding);
}

VertexInputLayout::~VertexInputLayout()
{
    delete[] vif_list;
    delete[] bind_list;
    delete[] attr_list;
}

const int VertexInputLayout::GetIndex(const AnsiString &name)const
{
    if(name.IsEmpty())return(-1);

    for(uint32_t i=0;i<count;i++)
        if(name.CaseComp(vif_list[i].name)==0)
            return(i);

    return -1;
}
VK_NAMESPACE_END
