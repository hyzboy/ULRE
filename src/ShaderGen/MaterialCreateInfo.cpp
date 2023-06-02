#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/shadergen/ShaderDescriptorInfo.h>
#include<hgl/graph/mtl/UBOCommon.h>
#include<hgl/graph/VKDeviceAttribute.h>
#include"common/MFCommon.h"

using namespace hgl;
using namespace hgl::graph;

STD_MTL_NAMESPACE_BEGIN
MaterialCreateInfo::MaterialCreateInfo(const MaterialCreateConfig *mc)
{
    config=mc;

    if(hasVertex    ())shader_map.Add(vert=new ShaderCreateInfoVertex  (&mdi));else vert=nullptr;
    if(hasGeometry  ())shader_map.Add(geom=new ShaderCreateInfoGeometry(&mdi));else geom=nullptr;
    if(hasFragment  ())shader_map.Add(frag=new ShaderCreateInfoFragment(&mdi));else frag=nullptr;

    mi_data_bytes=0;
    mi_shader_stage=0;
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

    for(int i=0;i<shader_map.GetCount();i++)
    {
        shader_map.GetKey(i,bit);

        if(flag_bits&bit)
            if(AddUBO(bit,set_type,ss.struct_name,ss.name))
                ++result;
    }

    return(result==shader_map.GetCount());
}

/**
* 设置材质实例代码与数据长度
* @param glsl_codes     材质实例GLSL代码
* @param data_bytes     单个材质实例数据长度
* @param shader_stage   具体使用材质实例的shader
* @return 是否设置成功
*/
bool MaterialCreateInfo::SetMaterialInstance(const AnsiString &glsl_codes,const uint32_t data_bytes,const uint32_t shader_stage)
{
    if(mi_data_bytes>0)return(false);           //已经有数据了

    if(shader_stage==0)return(false);

    if(data_bytes>0&&glsl_codes.Length()<4)return(false);

    mi_data_bytes=data_bytes;

    if(data_bytes>0)
        mi_codes=glsl_codes;

    const uint32_t ubo_range=config->dev_attr->physical_device->GetUBORange();
    AnsiString MI_MAX_COUNT=AnsiString::numberOf(ubo_range/data_bytes);

    mdi.AddStruct(MaterialInstanceStruct,mi_codes);
    mdi.AddStruct(SBS_MaterialInstanceData);

    UBODescriptor *ubo=new UBODescriptor();

    ubo->type=SBS_MaterialInstanceData.struct_name;
    hgl::strcpy(ubo->name,DESCRIPTOR_NAME_MAX_LENGTH,SBS_MaterialInstanceData.name);
    ubo->stage_flag=shader_stage;

    mdi.AddUBO(shader_stage,DescriptorSetType::PerMaterial,ubo);

    auto *it=shader_map.GetDataList();

    for(int i=0;i<shader_map.GetCount();i++)
    {
        if((*it)->key&shader_stage)
        {
            (*it)->value->AddDefine("MI_MAX_COUNT",MI_MAX_COUNT);
            (*it)->value->SetMaterialInstance(ubo,mi_codes);
        }

        ++it;
    }

    vert->AddMaterialInstanceID();           //增加一个材质实例ID

    mi_shader_stage=shader_stage;

    return(true);
}

bool MaterialCreateInfo::CreateShader(const GPUDeviceAttribute *dev_attr)
{
    if(shader_map.IsEmpty())
        return(false);

    mdi.Resort();

    ShaderCreateInfo *sc,*last=nullptr;

    for(int i=0;i<shader_map.GetCount();i++)
    {
        if(!shader_map.GetValue(i,sc))
            return(false);

        if(sc->GetShaderStage()<mi_shader_stage)
        {
            sc->AddOutput(VAT_UINT,VAN::MaterialInstanceID,Interpolation::Flat);
            sc->AddFunction(mtl::func::HandoverMI);
        }

        sc->CreateShader(last);

        last=sc;
    }

    return(true);
}
STD_MTL_NAMESPACE_END
