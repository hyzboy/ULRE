#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/shadergen/ShaderDescriptorInfo.h>

using namespace hgl;
using namespace hgl::graph;

namespace hgl{namespace graph{
MaterialCreateInfo::MaterialCreateInfo(const AnsiString &n,const RenderTargetOutputConfig &cfg,const uint32 ss)
{
    shader_name=n;

    rto_cfg=cfg;

    shader_stage=ss;

    if(hasVertex    ())shader_map.Add(vert=new ShaderCreateInfoVertex  (&mdi));else vert=nullptr;
    if(hasGeometry  ())shader_map.Add(geom=new ShaderCreateInfoGeometry(&mdi));else geom=nullptr;
    if(hasFragment  ())shader_map.Add(frag=new ShaderCreateInfoFragment(&mdi));else frag=nullptr;

    mi_length=0;
}

bool MaterialCreateInfo::AddStruct(const AnsiString &struct_name,const AnsiString &codes)
{
    if(struct_name.IsEmpty()||codes.IsEmpty())
        return(false);

    return mdi.AddStruct(struct_name,codes);
}

bool MaterialCreateInfo::AddUBO(const VkShaderStageFlagBits flag_bit,const DescriptorSetType set_type,const AnsiString &type_name,const AnsiString &name)
{
    if(!shader_map.KeyExist(flag_bit))
        return(false);

    if(!mdi.hasStruct(type_name))
        return(false);

    ShaderCreateInfo *sc=shader_map[flag_bit];

    if(!sc)
        return(false);

    UBODescriptor *ubo=mdi.GetUBO(name);

    if(ubo)
    {
        if(ubo->type!=type_name)
            return(false);

        ubo->stage_flag|=flag_bit;

        return sc->sdm->AddUBO(set_type,ubo);
    }
    else
    {
        ubo=new UBODescriptor();

        ubo->type=type_name;
        hgl::strcpy(ubo->name,DESCRIPTOR_NAME_MAX_LENGTH,name);

        return sc->sdm->AddUBO(set_type,mdi.AddUBO(flag_bit,set_type,ubo));
    }
}

bool MaterialCreateInfo::AddSampler(const VkShaderStageFlagBits flag_bit,const DescriptorSetType set_type,const SamplerType &st,const AnsiString &name)
{
    if(!shader_map.KeyExist(flag_bit))
        return(false);

    RANGE_CHECK_RETURN_FALSE(st);

    ShaderCreateInfo *sc=shader_map[flag_bit];

    if(!sc)
        return(false);

    SamplerDescriptor *sampler=mdi.GetSampler(name);

    AnsiString st_name=GetSamplerTypeName(st);

    if(sampler)
    {
        if(sampler->type!=st_name)
            return(false);

        sampler->stage_flag|=flag_bit;

        return sc->sdm->AddSampler(set_type,sampler);
    }
    else
    {
        sampler=new SamplerDescriptor();

        sampler->type=st_name;
        hgl::strcpy(sampler->name,DESCRIPTOR_NAME_MAX_LENGTH,name);

        return sc->sdm->AddSampler(set_type,mdi.AddSampler(flag_bit,set_type,sampler));
    }
}

bool MaterialCreateInfo::AddUBO(const uint32_t flag_bits,const DescriptorSetType &set_type,const ShaderBufferSource &ss)
{
    if(flag_bits==0)return(false);          //没有任何SHADER用？

    if(!mdi.hasStruct(ss.struct_name))
        mdi.AddStruct(ss.struct_name,ss.codes);

    uint result=0;
    VkShaderStageFlagBits bit;

    for(uint i=0;i<shader_map.GetCount();i++)
    {
        shader_map.GetKey(i,bit);

        if(flag_bits&bit)
            if(AddUBO(bit,set_type,ss.struct_name,ss.name))
                ++result;
    }

    return(result==shader_map.GetCount());
}

bool MaterialCreateInfo::CreateShader()
{
    if(shader_map.IsEmpty())
        return(false);

    if(mi_length>0)
    {
        AddUBO( shader_stage,
                DescriptorSetType::Global,
                mtl::SBS_MaterialInstance);
    }

    mdi.Resort();

    ShaderCreateInfo *sc,*last=nullptr;

    for(int i=0;i<shader_map.GetCount();i++)
    {
        if(!shader_map.GetValue(i,sc))
            return(false);

        sc->CreateShader(last);

        last=sc;
    }

    return(true);
}
}}//namespace hgl::graph
