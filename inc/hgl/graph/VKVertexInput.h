#pragma once
#include<hgl/graph/VKFormat.h>
#include<hgl/graph/VKVertexInputLayout.h>
#include<hgl/type/SortedSet.h>
#include<hgl/graph/VKVertexInputAttribute.h>

VK_NAMESPACE_BEGIN
class VILConfig;

class VertexInputConfig
{
    VIAArray via_array;
    VAType *type_list;
    const char **name_list;

    uint count_by_group[size_t(VertexInputGroup::RANGE_SIZE)];

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
    
    SortedSet<VIL *> vil_sets;

public:

    VertexInput(const VIAArray &);
    VertexInput(const VertexInput &)=delete;
    ~VertexInput();

    const uint      GetCount()const{return vic.GetCount();}

    const   VIL *   GetDefaultVIL()const{return default_vil;}
    VIL *           CreateVIL(const VILConfig *format_map=nullptr);
    bool            Release(VIL *);
    const uint32_t  GetInstanceCount()const{return vil_sets.GetCount();}
};//class VertexInput

VertexInput *GetVertexInput(const VIAArray &);
void ReleaseVertexInput(VertexInput *);
VK_NAMESPACE_END
