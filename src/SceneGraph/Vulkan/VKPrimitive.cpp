#include<hgl/graph/VKPrimitive.h>
#include<hgl/graph/VKBuffer.h>
#include<hgl/graph/VKShaderModule.h>
#include<hgl/graph/VKVertexAttribBuffer.h>

#ifdef _DEBUG
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKDeviceAttribute.h>
#endif//_DEBUG

VK_NAMESPACE_BEGIN
//bool Renderable::Set(const int stage_input_binding,VIL *vil,VkDeviceSize offset)
//{
//    if(stage_input_binding<0||stage_input_binding>=buf_count||!vil)return(false);
//
//    const VkVertexInputBindingDescription *desc=vertex_sm->GetDesc(stage_input_binding);
//    const VkVertexInputAttributeDescription *attr=vertex_sm->GetAttr(stage_input_binding);
//
//    if(vil->GetFormat()!=attr->format)return(false);
//    if(vil->GetStride()!=desc->stride)return(false);
//
//    //format信息来自于shader，实际中可以不一样。但那样需要为每一个格式产生一个同样shader的material instance，不同的格式又需要不同的pipeline，我们不支持这种行为
//
//    buf_list[stage_input_binding]=vil->GetBuffer();
//    buf_offset[stage_input_binding]=offset;
//
//    return(true);
//}

bool Primitive::Set(const AnsiString &name,VAB *vab,VkDeviceSize offset)
{
    if(!vab)return(false);
    if(buffer_list.KeyExist(name))return(false);

    VABAccess vad;

    vad.vab=vab;
    vad.offset=offset;
    vad.size=vab->GetBytes();

    buffer_list.Add(name,vad);

#ifdef _DEBUG
    DebugUtils *du=device->GetDebugUtils();

    if(du)
    {
        du->SetBuffer(vab->GetBuffer(),prim_name+":VAB:Buffer:"+name);
        du->SetDeviceMemory(vab->GetVkMemory(),prim_name+":VAB:Memory:"+name);
    }
#endif//_DEBUG

    return(true);
}

bool Primitive::GetVABAccess(const AnsiString &name,VABAccess *vad)
{
    if(name.IsEmpty())return(false);
    if(!vad)return(false);

    return buffer_list.Get(name,*vad);
}

bool Primitive::Set(IndexBuffer *ib,VkDeviceSize offset)
{
    if(!ib)return(false);

    index_buffer_data.buffer=ib;
    index_buffer_data.offset=offset;

#ifdef _DEBUG
    DebugUtils *du=device->GetDebugUtils();

    if(du)
    {
        du->SetBuffer(ib->GetBuffer(),prim_name+":IBO:Buffer");
        du->SetDeviceMemory(ib->GetVkMemory(),prim_name+":IBO:Memory");
    }
#endif//_DEBUG
    return(true);
}
VK_NAMESPACE_END
