#pragma once
#include<hgl/vk/VKFormat.h>
#include<hgl/vk/VKVertexInputLayout.h>
#include<hgl/type/OrderedSet.h>
#include<hgl/vk/VKVertexInputAttribute.h>
#include<hgl/common/AttributeProvider.h>
#include<array>

namespace hgl::graph{
class IGPUBuffer;
class VILConfig;

class VertexInputConfig
{
    VIAArray via_array;
    VAType *type_list;
    VertexAttrib *attrib_list;
    uint total_count;

public:

    const uint      GetCount()const{return via_array.count;}

public:

    VertexInputConfig(const VertexInputAttributeArray &sa_array);
    ~VertexInputConfig();

    VIL *CreateVIL(const VILConfig *format_map=nullptr);
};

class VertexInput
{
    VertexInputConfig vic;

    VIL *default_vil;

    OrderedSet<VIL *> vil_sets;

    bool pulling_enabled=false;

    struct AttribStream { const IGPUBuffer *buffer=nullptr; uint32_t byte_offset=0; uint32_t stride=0; };
    std::array<AttribStream,size_t(AttributeSemantic::BuiltinCount)> streams{};

public:

    VertexInput(const VIAArray &);
    VertexInput(const VertexInput &)=delete;
    ~VertexInput();

    const uint      GetCount()const{return vic.GetCount();}

    void            SetPullingEnabled(bool v){pulling_enabled=v;}
    bool            IsPullingEnabled()const{return pulling_enabled;}

    void SetAttribStream(const AttributeSemantic semantic,const IGPUBuffer *buffer,uint32_t byte_offset,uint32_t stride)
    {
        auto &st=streams[size_t(semantic)];
        st.buffer=buffer; st.byte_offset=byte_offset; st.stride=stride;
    }
    const AttribStream &GetAttribStream(const AttributeSemantic semantic)const
    {
        return streams[size_t(semantic)];
    }

    // Even in pulling mode we keep returning the default VIL sentinel (typically
    // count=0) so legacy call sites that require non-null VIL can proceed.
    const   VIL *   GetDefaultVIL()const{return default_vil;}
    VIL *           CreateVIL(const VILConfig *format_map=nullptr);
    bool            Release(VIL *);
    const uint32_t  GetInstanceCount()const{return vil_sets.GetCount();}
};//class VertexInput

VertexInput *GetVertexInput(const VIAArray &);
void ReleaseVertexInput(VertexInput *);
}//namespace hgl::graph
