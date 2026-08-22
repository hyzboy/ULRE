#pragma once
#include<hgl/vk/VKFormat.h>
#include<hgl/type/OrderedSet.h>
#include<hgl/vk/VKVertexInputAttribute.h>

namespace hgl::graph{
class VertexInputConfig
{
    VIAArray via_array;
    VAType *type_list;
    const char **name_list;

public:

    const uint GetCount()const{return via_array.count;}

public:

    VertexInputConfig(const VertexInputAttributeArray &sa_array);
    ~VertexInputConfig();
};

class VertexInput
{
    VertexInputConfig vic;

public:

    VertexInput(const VIAArray &);
    VertexInput(const VertexInput &)=delete;
    ~VertexInput();

    const uint      GetCount()const{return vic.GetCount();}
};

VertexInput *GetVertexInput(const VIAArray &);
void ReleaseVertexInput(VertexInput *);
}//namespace hgl::graph
