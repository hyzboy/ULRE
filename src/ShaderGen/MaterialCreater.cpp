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

    return mdm.AddUBOStruct(ubo_typename,codes);
}

bool MaterialCreater::AddUBO(const VkShaderStageFlagBits flag_bit,const DescriptorSetType set_type,const AnsiString &type_name,const AnsiString &name)
{
    if(!shader_map.KeyExist(flag_bit))
        return(false);

    if(!mdm.hasUBOStruct(type_name))
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

void MaterialCreater::SetContext()
{
    //ShaderMap使用ObjectMap保存,ObjectMap本质附带排序功能，所以这里无需再排序，直接设定prev,next即可

    LOG_INFO("Resort Shader.");

    ShaderCreater *prev,*cur,*next;

    auto *it=shader_map.GetDataList();

    const int count=shader_map.GetCount();

    for(int i=0; i<count; i++)
    {
        cur=(*it)->right;
        ++it;

        if(i>0)
            cur->sdm.SetPrevShader(prev->GetShaderStage());

        if(i<count-1)
        {
            next=(*it)->right;
            cur->sdm.SetNextShader(next->GetShaderStage());
        }

#ifdef _DEBUG
        cur->sdm.DebugOutput(i);
#endif//_DEBUG
            
        prev=cur;
    }
}

bool MaterialCreater::CompileShader()
{
    if(shader_map.IsEmpty())
        return(false);

    SetContext();       //设定上下文

    ShaderCreater *sc;

    for(int i=0;i<shader_map.GetCount();i++)
    {
        if(!shader_map.GetValue(i,sc))
            return(false);

        if(!sc->CompileToSPV())
            return(false);
    }

    return(true);
    
}
SHADERGEN_NAMESPACE_END
