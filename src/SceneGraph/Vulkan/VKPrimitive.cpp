#include<hgl/graph/VKPrimitive.h>
#include<hgl/graph/VKBuffer.h>
#include<hgl/graph/VKShaderModule.h>
#include<hgl/graph/VKVertexAttribBuffer.h>
#include<hgl/graph/VKIndexBuffer.h>

#ifdef _DEBUG
#include<hgl/graph/VKDevice.h>
#include<hgl/graph/VKDeviceAttribute.h>
#endif//_DEBUG

VK_NAMESPACE_BEGIN

const   VkDeviceSize    GetVertexCount( PrimitiveData *pd);
const   int             GetVABCount(    PrimitiveData *pd);
        VABAccess *     GetVABAccess(   PrimitiveData *pd,const AnsiString &name);
        IBAccess *      GetIBAccess(    PrimitiveData *pd);
        void            Destory(        PrimitiveData *pd);
        void            Free(           PrimitiveData *pd);

class PrimitivePrivateBuffer:public Primitive
{
public:

    using Primitive::Primitive;

    ~PrimitivePrivateBuffer() override
    {
        Destory(prim_data);
    }
    
    friend Primitive *CreatePrimitivePrivate(const AnsiString &,PrimitiveData *);
};//class PrimitivePrivateBuffer

class PrimitiveVDM:public Primitive
{
    VertexDataManager *vdm;

public:

    PrimitiveVDM(VertexDataManager *_vdm,const AnsiString &pn,PrimitiveData *pd):Primitive(pn,pd)
    {
        vdm=_vdm;
    }

    ~PrimitiveVDM() override
    {


        Free(prim_data);
    }

    friend Primitive *CreatePrimitivePrivate(VertexDataManager *,const AnsiString &,PrimitiveData *);
};//class PrimitiveVDM;

Primitive *CreatePrimitivePrivate(const AnsiString &name,PrimitiveData *pd)
{
    if(!pd)return(nullptr);
    if(name.IsEmpty())return(nullptr);

    return(new PrimitivePrivateBuffer(name,pd));
}

Primitive *CreatePrimitivePrivate(VertexDataManager *vdm,const AnsiString &name,PrimitiveData *pd)
{
    if(!vdm)return(nullptr);
    if(!pd)return(nullptr);
    if(name.IsEmpty())return(nullptr);

    return(new PrimitiveVDM(vdm,name,pd));
}

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

const VkDeviceSize Primitive::GetVertexCount()const
{
    return VK_NAMESPACE::GetVertexCount(prim_data);
}

const int Primitive::GetVACount()const
{
    return GetVABCount(prim_data);
}

VABAccess *Primitive::GetVABAccess(const AnsiString &name)
{
    return VK_NAMESPACE::GetVABAccess(prim_data,name);
}

IBAccess *Primitive::GetIBAccess()
{
    return VK_NAMESPACE::GetIBAccess(prim_data);
}

VK_NAMESPACE_END
