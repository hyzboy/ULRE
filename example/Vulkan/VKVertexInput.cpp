#include"VKVertexInput.h"
#include"VKBuffer.h"

VK_NAMESPACE_BEGIN
VertexInputState::~VertexInputState()
{
    if(instance_set.GetCount()>0)
    {
        //还有在用的，这是个错误
    }
}

int VertexInputState::Add(const UTF8String &name,const uint32_t shader_location,const VkFormat format,uint32_t offset,bool instance)
{
    //binding对应在vkCmdBindVertexBuffer中设置的缓冲区序列号，所以这个数字必须从0开始，而且紧密排列。
    //在VertexInput类中，buf_list需要严格按照本类产生的binding为序列号

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

    stage_input_locations.Add(name,attrib);

    return binding_index;
}

const int VertexInputState::GetLocation(const UTF8String &name)const
{
    if(name.IsEmpty())return -1;

    VkVertexInputAttributeDescription *attr;

    if(!stage_input_locations.Get(name,attr))
        return -1;

    return attr->location;
}

const int VertexInputState::GetBinding(const UTF8String &name)const
{
    if(name.IsEmpty())return -1;

    VkVertexInputAttributeDescription *attr;

    if(!stage_input_locations.Get(name,attr))
        return -1;

    return attr->binding;
}

VertexInputStateInstance *VertexInputState::CreateInstance()
{
    VertexInputStateInstance *vis_instance=new VertexInputStateInstance(this);

    instance_set.Add(vis_instance);

    return(vis_instance);
}

bool VertexInputState::Release(VertexInputStateInstance *vis_instance)
{
    return instance_set.Delete(vis_instance);
}

VertexInputStateInstance::VertexInputStateInstance(VertexInputState *_vis)
{
    vis=_vis;

    const int count=vis->GetCount();

    if(count<=0)
    {
        binding_list=nullptr;
        return;
    }

    binding_list=hgl_copy_new(count,vis->GetDescList());
}

VertexInputStateInstance::~VertexInputStateInstance()
{
    delete[] binding_list;

    vis->Release(this);
}

bool VertexInputStateInstance::SetInstance(const uint index,bool instance)
{
    if(index>=vis->GetCount())return(false);

    binding_list[index].inputRate=instance?VK_VERTEX_INPUT_RATE_INSTANCE:VK_VERTEX_INPUT_RATE_VERTEX;

    return(true);
}

void VertexInputStateInstance::Write(VkPipelineVertexInputStateCreateInfo &vis_create_info) const
{
    vis_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    const uint32_t count=vis->GetCount();

    vis_create_info.vertexBindingDescriptionCount = count;
    vis_create_info.pVertexBindingDescriptions = binding_list;

    vis_create_info.vertexAttributeDescriptionCount = count;
    vis_create_info.pVertexAttributeDescriptions = vis->GetAttrList();
}

VertexInput::VertexInput(const VertexInputState *state)
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

bool VertexInput::Set(const int index,VertexBuffer *buf,VkDeviceSize offset)
{
    if(index<0||index>=buf_count)return(false);

    const VkVertexInputBindingDescription *desc=vis->GetDesc(index);   
    const VkVertexInputAttributeDescription *attr=vis->GetAttr(index);

    if(buf->GetFormat()!=attr->format)return(false);
    if(buf->GetStride()!=desc->stride)return(false);

    buf_list[index]=*buf;
    buf_offset[index]=offset;

    return(true);
}
VK_NAMESPACE_END
