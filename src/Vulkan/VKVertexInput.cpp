#include<hgl/vk/VKVertexInput.h>
#include<hgl/vk/VKVertexInputConfig.h>
#include<hgl/mtl/VertexAttributeSpec.h>
#include<cassert>
#include<hgl/type/ObjectManager.h>
#include<cstdio>
#include <string>

namespace hgl::graph{
VertexInputConfig::VertexInputConfig(const VIAArray &viaa)
{
    via_array.Clone(&viaa);

    attrib_list=new VertexAttrib[via_array.count];
    type_list=new VAType[via_array.count];

    const VertexInputAttribute *sa=via_array.items;
    total_count=via_array.count;

    for(uint i=0;i<via_array.count;i++)
    {
        attrib_list[i]          =sa->attrib;
        type_list[i].basetype   =VABaseType(sa->basetype);
        type_list[i].vec_size   =sa->vec_size;

        ++sa;
    }
}

VertexInputConfig::~VertexInputConfig()
{
    delete[] attrib_list;
    delete[] type_list;
}

VIL *VertexInputConfig::CreateVIL(const VILConfig *cfg)
{
    VIL *vil=new VIL(via_array.count);

    VkVertexInputBindingDescription *bind_desc=vil->bind_list;
    VkVertexInputAttributeDescription *attr_desc=vil->attr_list;
    VertexInputFormat *vif=vil->vif_list;

    const VertexInputAttribute *via;
    VAConfig vac;
    uint binding=0;
    via=via_array.items;

    for(uint i=0;i<via_array.count;i++)
    {
        //binding对应的是第几个数据输入流
        //实际使用一个binding可以绑定多个attrib
        //比如在一个流中传递{pos,color}这样两个数据，就需要两个attrib
        //但在我们的设计中，仅支持一个流传递一个attrib

        attr_desc->binding   =binding;
        attr_desc->location  =via->location;                 //此值对应shader中的layout(location=

        attr_desc->offset    =0;

        bind_desc->binding   =binding;                      //binding对应在vkCmdBindVertexBuffer中设置的缓冲区的序列号，所以这个数字必须从0开始，而且紧密排列。
                                                            //在Mesh类中，buffer_list必需严格按照本此binding为序列号排列

        ++binding;

        if(!cfg||!cfg->Get(via->attrib,vac))
        {
            // Legacy fallback: derive storage format from VAType when not explicitly specified
            attr_desc->format    =GetVulkanFormat(via);

            bind_desc->inputRate =VkVertexInputRate::VK_VERTEX_INPUT_RATE_VERTEX;
        }
        else
        {
            // Use explicit storage format if specified in config, otherwise legacy fallback derivation
            attr_desc->format    =(vac.format==PF_UNDEFINED?GetVulkanFormat(via):vac.format);

            bind_desc->inputRate =vac.input_rate;
        }

        VAType shader_type;
        shader_type.basetype = VABaseType(via->basetype);
        shader_type.vec_size = via->vec_size;

        if(!mtl::IsStorageFormatCompatibleWithShaderType(shader_type, attr_desc->format, via->attrib))
        {
            // Legacy fallback: try implicit VAType derivation when explicit format is incompatible
            const VkFormat fallback_format = GetVulkanFormat(via);
            if(mtl::IsStorageFormatCompatibleWithShaderType(shader_type, fallback_format, via->attrib))
            {
                std::fprintf(stderr,
                    "[VertexInputConfig] incompatible shader/storage pair, fallback applied: attrib='%s' shader='%s' requested='%s' fallback='%s'\n",
                    GetVertexAttribName(via->attrib),
                    GetVertexAttribName((VABaseType)via->basetype, via->vec_size),
                    GetVulkanFormatName(attr_desc->format),
                    GetVulkanFormatName(fallback_format));

                attr_desc->format = fallback_format;
            }
            else
            {
                std::fprintf(stderr,
                    "[VertexInputConfig] incompatible shader/storage pair, VIL rejected: attrib='%s' shader='%s' requested='%s'\n",
                    GetVertexAttribName(via->attrib),
                    GetVertexAttribName((VABaseType)via->basetype, via->vec_size),
                    GetVulkanFormatName(attr_desc->format));

#ifdef _DEBUG
                assert(false && "VertexInputConfig::CreateVIL incompatible shader/storage pair");
#endif

                delete vil;
                return nullptr;
            }
        }

        bind_desc->stride    =GetStrideByFormat(attr_desc->format);

        vif->format     =attr_desc->format;
        vif->vec_size   =via->vec_size;
        vif->stride     =bind_desc->stride;

        vif->attrib     =via->attrib;
        vif->binding    =attr_desc->binding;
        vif->input_rate =bind_desc->inputRate;

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

    if(!vil_sets.empty())
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

    if(!vil)
        return nullptr;

    vil_sets.insert(vil);

    return vil;
}

bool VertexInput::Release(VIL *vil)
{
    return vil_sets.erase(vil)>0;
}

namespace
{
    constexpr const uint VertexInputAttributeBytes=sizeof(VertexInputAttribute);
    constexpr const uint VIAIndexLength=(VertexInputAttributeBytes)*16;

    ManagedObjectRegistry<std::string,VertexInput> vertex_input_manager;

    //完全没必要的管理

    //VIAArray+VertexInput 就算有1024个，也没多少内存占用。完全没必要搞什么引用计数管理

    void MakeVIIndex(std::string &result,const VIAArray &viaa)
    {
        result=std::to_string(viaa.count);

        const VertexInputAttribute *via=viaa.items;

        for(uint i=0;i<viaa.count;i++)
        {
            result+="[\"";

            result+=GetVertexAttribName(via->attrib);
            result+="\",location:";
            result+=std::to_string(via->location);
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
    std::string index;

    MakeVIIndex(index,saa);

    VertexInput *vi=vertex_input_manager.Get(index);

    if(!vi)
    {
        vi=new VertexInput(saa);

        vertex_input_manager.Add(index,vi);
    }

    return vi;
}

VertexInput *GetVertexInput(const std::vector<VIA> &vias)
{
    VIAArray via_array;

    const uint count=static_cast<uint>(vias.size());
    if(count>0)
    {
        if(!via_array.Init(count))
            return nullptr;

        for(uint i=0;i<count;i++)
            via_array.items[i]=vias[i];
    }

    return GetVertexInput(via_array);
}

void ReleaseVertexInput(VertexInput *vi)
{
    if(!vi)
        return;

    vertex_input_manager.Release(vi);
}
}//namespace hgl::graph
