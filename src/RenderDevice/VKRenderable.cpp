#include<hgl/graph/VKRenderable.h>
#include<hgl/graph/VKBuffer.h>
#include<hgl/graph/VKShaderModule.h>

VK_NAMESPACE_BEGIN
//bool Renderable::Set(const int stage_input_binding,VAB *vab,VkDeviceSize offset)
//{
//    if(stage_input_binding<0||stage_input_binding>=buf_count||!vab)return(false);
//
//    const VkVertexInputBindingDescription *desc=vertex_sm->GetDesc(stage_input_binding);
//    const VkVertexInputAttributeDescription *attr=vertex_sm->GetAttr(stage_input_binding);
//
//    if(vab->GetFormat()!=attr->format)return(false);
//    if(vab->GetStride()!=desc->stride)return(false);
//
//    //format信息来自于shader，实际中可以不一样。但那样需要为每一个格式产生一个同样shader的material instance，不同的格式又需要不同的pipeline，我们不支持这种行为
//
//    buf_list[stage_input_binding]=vab->GetBuffer();
//    buf_offset[stage_input_binding]=offset;
//
//    return(true);
//}

bool Renderable::Set(const UTF8String &name,VAB *vab,VkDeviceSize offset)
{
    if(!vab)return(false);
    if(buffer_list.KeyExist(name))return(false);

    BufferData bd;
    
    bd.buf=vab;
    bd.offset=offset;

    buffer_list.Add(name,bd);
    return(true);
}

VAB *Renderable::GetVAB(const UTF8String &name,VkDeviceSize *offset)
{
    if(!offset)return(nullptr);
    if(name.IsEmpty())return(nullptr);

    BufferData bd;

    if(buffer_list.Get(name,bd))
    {
        *offset=bd.offset;
        return bd.buf;
    }

    return(nullptr);
}

VkBuffer Renderable::GetBuffer(const UTF8String &name,VkDeviceSize *offset)
{
    VAB *vab=GetVAB(name,offset);

    if(vab)return vab->GetBuffer();

    return(VK_NULL_HANDLE);
}
VK_NAMESPACE_END
