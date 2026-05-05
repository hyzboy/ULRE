#pragma once
#include<hgl/vk/VKFormat.h>
#include<hgl/vk/VKVertexInputLayout.h>
#include<hgl/type/OrderedSet.h>
#include<hgl/vk/VKVertexInputAttribute.h>

#include<array>

namespace hgl::graph{
class IGPUBuffer;

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
};

class VertexInput
{
    VertexInputConfig vic;

    bool pulling_enabled=false;

    struct AttribStream { const IGPUBuffer *buffer=nullptr; uint32_t byte_offset=0; uint32_t stride=0; };
    std::array<AttribStream,size_t(uint32_t(VertexAttrib::RANGE_SIZE))> streams{};

public:

    VertexInput(const VIAArray &);
    VertexInput(const VertexInput &)=delete;
    ~VertexInput();

    const uint      GetCount()const{return vic.GetCount();}

    void            SetPullingEnabled(bool v){pulling_enabled=v;}
    bool            IsPullingEnabled()const{return pulling_enabled;}

    bool SetAttribStream(const VertexAttrib attrib,const IGPUBuffer *buffer,uint32_t byte_offset,uint32_t stride)
    {
        const uint32_t binding = uint32_t(attrib);
        if(binding>=streams.size())
            return false;

        auto &st=streams[size_t(binding)];
        st.buffer=buffer; st.byte_offset=byte_offset; st.stride=stride;
        return true;
    }
    bool SetVertexStreamBinding(const uint32_t binding,const IGPUBuffer *buffer,uint32_t byte_offset,uint32_t stride)
    {
        if(binding>=streams.size())
            return false;

        auto &st=streams[size_t(binding)];
        st.buffer=buffer; st.byte_offset=byte_offset; st.stride=stride;
        return true;
    }

    const AttribStream *GetAttribStream(const VertexAttrib attrib)const
    {
        const uint32_t binding = uint32_t(attrib);
        if(binding>=streams.size())
            return nullptr;

        return &streams[size_t(binding)];
    }

    const AttribStream *GetVertexStreamBinding(const uint32_t binding)const
    {
        if(binding>=streams.size())
            return nullptr;

        return &streams[size_t(binding)];
    }
};//class VertexInput

VertexInput *GetVertexInput(const VIAArray &);
void ReleaseVertexInput(VertexInput *);
}//namespace hgl::graph
