#include<hgl/graph/vulkan/VKRenderable.h>
#include<hgl/graph/vulkan/VKBuffer.h>
#include<hgl/graph/vulkan/VKShaderModule.h>

VK_NAMESPACE_BEGIN
Renderable::Renderable(const VertexShaderModule *vsm)
{
    vertex_sm=vsm;

    buf_count=vertex_sm->GetAttrCount();

    buf_list=hgl_zero_new<VkBuffer>(buf_count);
    buf_offset=hgl_zero_new<VkDeviceSize>(buf_count);
}

Renderable::~Renderable()
{
    delete[] buf_offset;
    delete[] buf_list;
}

bool Renderable::Set(const int stage_input_binding,VertexBuffer *vbo,VkDeviceSize offset)
{
    if(stage_input_binding<0||stage_input_binding>=buf_count||!vbo)return(false);

    const VkVertexInputBindingDescription *desc=vertex_sm->GetDesc(stage_input_binding);
    const VkVertexInputAttributeDescription *attr=vertex_sm->GetAttr(stage_input_binding);

    if(vbo->GetFormat()!=attr->format)return(false);
    if(vbo->GetStride()!=desc->stride)return(false);

    //format信息来自于shader，实际中可以不一样。但那样需要为每一个格式产生一个同样shader的material instance，不同的格式又需要不同的pipeline，我们不支持这种行为

    buf_list[stage_input_binding]=*vbo;
    buf_offset[stage_input_binding]=offset;

    return(true);
}

bool Renderable::Set(const UTF8String &name,VertexBuffer *vbo,VkDeviceSize offset)
{
    return Set(vertex_sm->GetStageInputBinding(name),vbo,offset);
}
VK_NAMESPACE_END
