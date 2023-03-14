#include<hgl/shadergen/MaterialCreater.h>

using namespace hgl;
using namespace hgl::graph;

SHADERGEN_NAMESPACE_BEGIN
MaterialCreater::MaterialCreater(const uint rc,const bool rd,const uint32 ss)
{
    rt_color_count=rc;
    rt_depth=rd;

    shader_stage=ss;

    if(hasVertex    ())shader_map.Add(vert=new ShaderCreaterVertex  (&mdm));else vert=nullptr;
    if(hasGeometry  ())shader_map.Add(geom=new ShaderCreaterGeometry(&mdm));else geom=nullptr;
    if(hasFragment  ())shader_map.Add(frag=new ShaderCreaterFragment(&mdm));else frag=nullptr;
}

bool MaterialCreater::AddStruct(const AnsiString &struct_name,const AnsiString &codes)
{
    if(struct_name.IsEmpty()||codes.IsEmpty())
        return(false);

    return mdm.AddStruct(struct_name,codes);
}

bool MaterialCreater::AddUBO(const VkShaderStageFlagBits flag_bit,const DescriptorSetType set_type,const AnsiString &type_name,const AnsiString &name)
{
    if(!shader_map.KeyExist(flag_bit))
        return(false);

    if(!mdm.hasStruct(type_name))
        return(false);

    ShaderCreater *sc=shader_map[flag_bit];

    if(!sc)
        return(false);

    UBODescriptor *ubo=mdm.GetUBO(name);

    if(ubo)
    {
        if(ubo->type!=type_name)
            return(false);

        ubo->stage_flag|=flag_bit;

        return sc->sdm.AddUBO(set_type,ubo);
    }
    else
    {
        ubo=new UBODescriptor();

        ubo->type=type_name;
        ubo->name=name;

        return sc->sdm.AddUBO(set_type,mdm.AddUBO(flag_bit,set_type,ubo));
    }
}

bool MaterialCreater::AddSampler(const VkShaderStageFlagBits flag_bit,const DescriptorSetType set_type,const SamplerType &st,const AnsiString &name)
{
    if(!shader_map.KeyExist(flag_bit))
        return(false);

    RANGE_CHECK_RETURN_FALSE(st);

    ShaderCreater *sc=shader_map[flag_bit];

    if(!sc)
        return(false);

    SamplerDescriptor *sampler=mdm.GetSampler(name);

    AnsiString st_name=GetSamplerTypeName(st);

    if(sampler)
    {
        if(sampler->type!=st_name)
            return(false);

        sampler->stage_flag|=flag_bit;

        return sc->sdm.AddSampler(set_type,sampler);
    }
    else
    {
        sampler=new SamplerDescriptor();

        sampler->type=st_name;
        sampler->name=name;

        return sc->sdm.AddSampler(set_type,mdm.AddSampler(flag_bit,set_type,sampler));
    }
}

bool MaterialCreater::CreateShader()
{
    if(shader_map.IsEmpty())
        return(false);

    //mdm.Resort();

    ShaderCreater *sc,*last=nullptr;

    //for(int i=0;i<shader_map.GetCount();i++)
    //{
    //    if(!shader_map.GetValue(i,sc))
    //        return(false);

    //    sc->CreateShader(last);

    //    last=sc;
    //}

    //vert->CreateShader(nullptr);
    frag->CreateShader(vert);

    return(true);
}
SHADERGEN_NAMESPACE_END
