#pragma once
#include<hgl/vk/VKFormat.h>
#include<hgl/vk/VKVertexInputLayout.h>
#include<hgl/type/OrderedSet.h>
#include<hgl/vk/VKVertexInputAttribute.h>

namespace hgl::graph{
class VILConfig;

class VertexInputConfig
{
    VIAArray via_array;
    VAType *type_list;
    VertexAttrib *attrib_list;
    uint total_count;

public:

    const uint      GetCount()const{return via_array.count;}
    const VIAArray &GetVIAArray()const{return via_array;}

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

public:

    VertexInput(const VIAArray &);
    VertexInput(const VertexInput &)=delete;
    ~VertexInput();

    const uint      GetCount()const{return vic.GetCount();}
    const VIAArray &GetVIAArray()const{return vic.GetVIAArray();}

    const   VIL *   GetDefaultVIL()const{return default_vil;}
    VIL *           CreateVIL(const VILConfig *format_map=nullptr);
    bool            Release(VIL *);
    const uint32_t  GetInstanceCount()const{return vil_sets.GetCount();}
};//class VertexInput

VertexInput *GetVertexInput(const VIAArray &);
void ReleaseVertexInput(VertexInput *);
}//namespace hgl::graph
