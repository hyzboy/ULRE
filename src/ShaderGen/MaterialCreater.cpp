#include<hgl/shadergen/MaterialCreater.h>

using namespace hgl;
using namespace hgl::graph;

SHADERGEN_NAMESPACE_BEGIN
MaterialCreater::MaterialCreater(const uint rc,const uint32 ss)
{
    rt_count=rc;
    shader_stage=ss;

    if(hasVertex    ())shader_map.Add(vert=new ShaderCreaterVertex  );else vert=nullptr;
    if(hasGeometry  ())shader_map.Add(geom=new ShaderCreaterGeometry);else geom=nullptr;
    if(hasFragment  ())shader_map.Add(frag=new ShaderCreaterFragment);else frag=nullptr;
}

bool MaterialCreater::AddUBOStruct(const AnsiString &ubo_typename,const AnsiString &codes)
{
    if(ubo_typename.IsEmpty()||codes.IsEmpty())
        return(false);

    return MDM.AddUBOStruct(ubo_typename,codes);
}

bool MaterialCreater::AddUBO(const VkShaderStageFlagBits flag_bits,const DescriptorSetType set_type,const AnsiString &type_name,const AnsiString &name)
{
    if(!shader_map.KeyExist(flag_bits))
        return(false);

    if(!MDM.hasUBOStruct(type_name))
        return(false);

    ShaderCreater *sc=shader_map[flag_bits];

    if(!sc)
        return(false);

    UBODescriptor *ubo=MDM.GetUBO(name);

    if(ubo)
    {
        if(ubo->type!=type_name)
            return(false);

        ubo->stage_flag|=flag_bits;

        return sc->sdm.AddUBO(set_type,ubo);
    }
    else
    {
        ubo=new UBODescriptor();

        ubo->type=type_name;
        ubo->name=name;

        return sc->sdm.AddUBO(set_type,MDM.AddUBO(flag_bits,set_type,ubo));
    }
}

bool MaterialCreater::AddSampler(const VkShaderStageFlagBits flag_bits,const DescriptorSetType set_type,const SamplerType &st,const AnsiString &name)
{
    if(!shader_map.KeyExist(flag_bits))
        return(false);

    RANGE_CHECK_RETURN_FALSE(st);

    ShaderCreater *sc=shader_map[flag_bits];

    if(!sc)
        return(false);

    SamplerDescriptor *sampler=MDM.GetSampler(name);

    AnsiString st_name=GetSamplerTypeName(st);

    if(sampler)
    {
        if(sampler->type!=st_name)
            return(false);

        sampler->stage_flag|=flag_bits;

        return sc->sdm.AddSampler(set_type,sampler);
    }
    else
    {
        sampler=new SamplerDescriptor();

        sampler->type=st_name;
        sampler->name=name;

        return sc->sdm.AddSampler(set_type,MDM.AddSampler(flag_bits,set_type,sampler));
    }
}
SHADERGEN_NAMESPACE_END
