#pragma once
#include<vulkan/vulkan.h>
#include<hgl/vk/VKVertexInputFormat.h>
#include<hgl/type/String.h>

namespace hgl::graph{
/**
* 顶点输入布局<br>
* 本对象用于传递给Material,用于已经确定好顶点格式的情况下，依然可修改部分设定(如instance)。
*/
class VertexInputLayout
{
private:

    uint32_t count;

    VkVertexInputBindingDescription *bind_list;
    VkVertexInputAttributeDescription *attr_list;
    VertexInputFormat *vif_list;

private:

    friend class VertexInputConfig;

    VertexInputLayout(const uint32_t);

public:

    ~VertexInputLayout();

    const uint32_t              GetVertexAttribCount()const{return count;}
    const int                   GetIndex(const VertexSemantic semantic)const;

    const VertexInputFormat *   GetVIFList()const{return vif_list;}

    const VkFormat GetVulkanFormat(const VertexSemantic semantic)const
    {
        const int index=GetIndex(semantic);

        return index==-1?VK_FORMAT_UNDEFINED:vif_list[index].format;
    }

    const VertexInputFormat *GetConfig(const uint index)const{return (index>=count)?nullptr:vif_list+index;}
    const VertexInputFormat *GetConfig(const VertexSemantic semantic)const{return GetConfig(GetIndex(semantic));}

public:

    VkVertexInputBindingDescription *   NewBindListCopy()const{return new_copy(bind_list,count);}
    VkVertexInputAttributeDescription * NewAttrListCopy()const{return new_copy(attr_list,count);}

    std::strong_ordering operator<=>(const VertexInputLayout &vil)const
    {
        if(auto cmp = count <=> vil.count; cmp != 0)
            return cmp;

        if(auto cmp = mem_compare_ordering(bind_list, vil.bind_list, count); cmp != 0)
            return cmp;

        return mem_compare_ordering(attr_list, vil.attr_list, count);
    }

    bool operator==(const VertexInputLayout &vil) const
    {
        if(count != vil.count)
            return false;

        if(mem_compare(bind_list, vil.bind_list, count) != 0)
            return false;

        return mem_compare(attr_list, vil.attr_list, count) == 0;
    }

    bool operator!=(const VertexInputLayout &vil) const
    {
        return !(*this == vil);
    }

};//class VertexInputLayout

using VIL=VertexInputLayout;
}//namespace hgl::graph
