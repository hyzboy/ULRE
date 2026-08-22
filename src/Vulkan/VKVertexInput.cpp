#include<hgl/vk/VKVertexInput.h>
#include<hgl/type/ObjectManager.h>
#include<cstring>

namespace hgl::graph{
VertexInputConfig::VertexInputConfig(const VIAArray &viaa)
{
    via_array.Clone(&viaa);

    name_list=new const char *[via_array.count];
    type_list=new VAType[via_array.count];

    const VertexInputAttribute *sa=via_array.items;

    for(uint i=0;i<via_array.count;i++)
    {
        name_list[i]            =GetVertexSemanticName(sa->semantic);
        type_list[i].basetype   =VABaseType(sa->basetype);
        type_list[i].vec_size   =sa->vec_size;

        ++sa;
    }
}

VertexInputConfig::~VertexInputConfig()
{
    delete[] name_list;
    delete[] type_list;
}

VertexInput::VertexInput(const VIAArray &sa_array):vic(sa_array)
{
}

VertexInput::~VertexInput()
{
}

namespace
{
    constexpr const uint VertexInputAttributeBytes=sizeof(VertexInputAttribute);
    constexpr const uint VIAIndexLength=(VertexInputAttributeBytes)*16;

    ManagedObjectRegistry<AnsiString,VertexInput> vertex_input_manager;

    //完全没必要的管理

    //VIAArray+VertexInput 就算有1024个，也没多少内存占用。完全没必要搞什么引用计数管理

    void MakeVIIndex(AnsiString &result,const VIAArray &viaa)
    {
        result=AnsiString::numberOf(viaa.count);

        const VertexInputAttribute *via=viaa.items;

        for(uint i=0;i<viaa.count;i++)
        {
            result+="[\"";
            result+=GetVertexSemanticName(via->semantic);
            result+="\",location:";
            result+=AnsiString::numberOf(via->location);
            result+=",type:";
            result+=GetVertexAttribName((VABaseType)via->basetype,via->vec_size);
            result+=",interpolation:";
            result+=GetInterpolationName(via->interpolation);

            result+="]";

            ++via;
        }
    }
}//namespace

VertexInput *GetVertexInput(const VIAArray &saa)
{
    AnsiString index;

    MakeVIIndex(index,saa);

    VertexInput *vi=vertex_input_manager.Get(index);

    if(!vi)
    {
        vi=new VertexInput(saa);

        vertex_input_manager.Add(index,vi);
    }

    return vi;
}

void ReleaseVertexInput(VertexInput *vi)
{
    if(!vi)return;

    vertex_input_manager.Release(vi);
}
}//namespace hgl::graph
