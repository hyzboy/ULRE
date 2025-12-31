#pragma once
#include<hgl/graph/VKVertexInputFormat.h>
#include<hgl/type/String.h>
VK_NAMESPACE_BEGIN
/**
* 顶点输入布局<br>
* 本对象用于传递给Material,用于已经确定好顶点格式的情况下，依然可修改部分设定(如instance)。
*/
class VertexInputLayout:public Comparator<VertexInputLayout>
{
private:

    uint32_t count;

    VkVertexInputBindingDescription *bind_list;
    VkVertexInputAttributeDescription *attr_list;
    VertexInputFormat *vif_list;
    VertexInputFormat *vif_list_by_group[size_t(VertexInputGroup::RANGE_SIZE)];

    uint count_by_group[size_t(VertexInputGroup::RANGE_SIZE)];
    uint first_binding[size_t(VertexInputGroup::RANGE_SIZE)];

private:

    friend class VertexInputConfig;

    VertexInputLayout(const uint32_t);

public:

    ~VertexInputLayout();

    const uint32_t                              GetVertexAttribCount()const{return count;}
    const uint32_t                              GetVertexAttribCount(const VertexInputGroup &vig)const
    {
        RANGE_CHECK_RETURN(vig,0)

        return count_by_group[size_t(vig)];
    }

    const uint32_t                              GetFirstBinding     (const VertexInputGroup &vig)const
    {
        RANGE_CHECK_RETURN(vig,0)

        return first_binding[size_t(vig)];
    }

    const int                                   GetIndex            (const AnsiString &name)const;

    const VertexInputFormat *                   GetVIFList          ()const{return vif_list;}
    const VertexInputFormat *                   GetVIFList          (const VertexInputGroup &vig)const
    {
        RANGE_CHECK_RETURN_NULLPTR(vig)

        return vif_list_by_group[size_t(vig)];
    }

    const VkFormat      GetVulkanFormat(const AnsiString &name)const
    {
        const int index=GetIndex(name);

        return index==-1?VK_FORMAT_UNDEFINED:vif_list[index].format;
    }

    const VertexInputFormat *GetConfig(const uint index)const{return (index>=count)?nullptr:vif_list+index;}
    const VertexInputFormat *GetConfig(const AnsiString &name)const{return GetConfig(GetIndex(name));}

public:

    VkVertexInputBindingDescription *           NewBindListCopy()const{return hgl_new_copy(bind_list,count);}
    VkVertexInputAttributeDescription *         NewAttrListCopy()const{return hgl_new_copy(attr_list,count);}

    const int compare(const VertexInputLayout &vil)const override;
};//class VertexInputLayout

using VIL=VertexInputLayout;
VK_NAMESPACE_END
