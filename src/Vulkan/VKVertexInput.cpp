#include<hgl/vk/VKVertexInput.h>
#include<hgl/vk/VKVertexInputConfig.h>
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
        name_list[i]            =sa->name;
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

VIL *VertexInputConfig::CreateVIL(const VILConfig *cfg)
{
    VIL *vil=new VIL(via_array.count);

    VkVertexInputBindingDescription *bind_desc=vil->bind_list;
    VkVertexInputAttributeDescription *attr_desc=vil->attr_list;
    VertexInputFormat *vif=vil->vif_list;

    const VertexInputAttribute *via=via_array.items;
    VAConfig vac;
    uint binding=0;

    for(uint i=0;i<via_array.count;i++)
    {
        attr_desc->binding   =binding;
        attr_desc->location  =via->location;
        attr_desc->offset    =0;

        bind_desc->binding   =binding;

        ++binding;

        const bool is_joint_id=(std::strcmp(via->name,VAN::JointID)==0);
        const bool is_joint_weight=(std::strcmp(via->name,VAN::JointWeight)==0);

        if(is_joint_id)
        {
            attr_desc->format   =VK_FORMAT_R8G8B8A8_UINT;
            bind_desc->stride   =4;
        }
        else
        if(is_joint_weight)
        {
            attr_desc->format   =VK_FORMAT_R8G8B8A8_UNORM;
            bind_desc->stride   =4;
        }
        else
        {
            if(!cfg||!cfg->Get(via->name,vac))
            {
                attr_desc->format    =GetVulkanFormat(via);
            }
            else
            {
                attr_desc->format    =(vac.format==PF_UNDEFINED?GetVulkanFormat(via):vac.format);
                bind_desc->inputRate =vac.input_rate;
            }

            bind_desc->stride=GetStrideByFormat(attr_desc->format);
        }

        vif->format     =attr_desc->format;
        vif->vec_size   =via->vec_size;
        vif->stride     =bind_desc->stride;
        vif->name       =via->name;
        vif->binding    =attr_desc->binding;

        ++vif;
        ++attr_desc;
        ++bind_desc;
        ++via;
    }

    return(vil);
}

VertexInput::VertexInput(const VIAArray &sa_array):vic(sa_array)
{
    default_vil=vic.CreateVIL(nullptr);
}

VertexInput::~VertexInput()
{
    delete default_vil;

    if(vil_sets.GetCount()>0)
    {
        //还有在用的，这是个错误
    }
}

VIL *VertexInput::CreateVIL(const VILConfig *cfg)
{
    if(!cfg)
        return(default_vil);

    //原本是想在这里做根据VILConfig的Map缓冲管理，避免重复创建VIL。
    //但VILConfig的复制与比较过于复杂，而且这种使用情况极少。所以放弃做这个事情，如未来真正产生这种需求时再做。

    VIL *vil=vic.CreateVIL(cfg);

    vil_sets.Add(vil);

    return vil;
}

bool VertexInput::Release(VIL *vil)
{
    return vil_sets.Delete(vil);
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

            result+=via->name;
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
