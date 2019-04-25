#include"VKVertexInput.h"
#include"VKBuffer.h"

VK_NAMESPACE_BEGIN

int VertexInputState::Add(const UTF8String &name,const uint32_t shader_location,const VkFormat format,uint32_t offset,bool instance)
{
    const int binding_index=binding_list.GetCount();        //参考opengl vab,binding_index必须从0开始，紧密排列。对应在vkCmdBindVertexBuffer中的缓冲区索引

    binding_list.SetCount(binding_index+1);
    attribute_list.SetCount(binding_index+1);

    VkVertexInputBindingDescription *binding=binding_list.GetData()+binding_index;
    VkVertexInputAttributeDescription *attrib=attribute_list.GetData()+binding_index;

    binding->binding=binding_index;
    binding->stride=GetStrideByFormat(format);
    binding->inputRate=instance?VK_VERTEX_INPUT_RATE_INSTANCE:VK_VERTEX_INPUT_RATE_VERTEX;
    
    //实际使用可以一个binding绑多个attrib，但我们仅支持1v1。
    //一个binding是指在vertex shader中，由一个vertex输入流输入数据，attrib指其中的数据成分
    //比如在一个流中传递{pos,color}这样两个数据，就需要两个attrib
    //但我们在一个流中，仅支持一个attrib传递

    attrib->binding=binding_index;
    attrib->location=shader_location;
    attrib->format=format;
    attrib->offset=offset;

    stage_input_locations.Add(name,StageInput(binding_index,shader_location,format));

    return binding_index;
}

const int VertexInputState::GetLocation(const UTF8String &name)const
{
    if(name.IsEmpty())return -1;

    StageInput si;

    if(!stage_input_locations.Get(name,si))
        return -1;

    return si.location;
}

const int VertexInputState::GetBinding(const UTF8String &name)const
{
    if(name.IsEmpty())return -1;

    StageInput si;

    if(!stage_input_locations.Get(name,si))
        return -1;

    return si.binding;
}

void VertexInputState::Write(VkPipelineVertexInputStateCreateInfo &vis) const
{
    vis.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    vis.vertexBindingDescriptionCount = binding_list.GetCount();
    vis.pVertexBindingDescriptions = binding_list.GetData();

    vis.vertexAttributeDescriptionCount = attribute_list.GetCount();
    vis.pVertexAttributeDescriptions = attribute_list.GetData();
}

VertexInput::VertexInput(VertexInputState *state)
{
    vis=state;

    if(!vis)
        return;

    buf_count=vis->GetCount();

    buf_list=hgl_zero_new<VkBuffer>(buf_count);
    buf_offset=hgl_zero_new<VkDeviceSize>(buf_count);
}

VertexInput::~VertexInput()
{
    delete[] buf_offset;
    delete[] buf_list;
}

bool VertexInput::Set(int index,VertexBuffer *buf,VkDeviceSize offset)
{
    if(index<0||index>=buf_count)return(false);

    VkVertexInputBindingDescription *desc=vis->GetDesc(index);   
    VkVertexInputAttributeDescription *attr=vis->GetAttr(index);

    if(buf->GetFormat()!=attr->format)return(false);
    if(buf->GetStride()!=desc->stride)return(false);

    buf_list[index]=*buf;
    buf_offset[index]=offset;

    return(true);
}
VK_NAMESPACE_END
