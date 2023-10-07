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

    ubo_range=config->dev_attr->physical_device->GetUBORange();              //Mali-T系/G71为16k，nVidia和Mali-G系列除G71外为64k，Intel/PowerVR为128M，AMD无限制。
    ssbo_range=config->dev_attr->physical_device->GetSSBORange();

    {
        mi_data_bytes=0;
        mi_shader_stage=0;
        mi_max_count=0;
        mi_ubo=nullptr;
    }

    {
        l2w_max_count=hgl_min<uint32_t>(ubo_range/sizeof(Matrix4f),HGL_U16_MAX);

        l2w_shader_stage=0;
        l2w_ubo=nullptr;
    }
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

namespace
{
    UBODescriptor *CreateUBODescriptor(const ShaderBufferSource &sbs,const uint32_t flag_bits)
    {
        UBODescriptor *ubo=new UBODescriptor;

        ubo->type=sbs.struct_name;

        hgl::strcpy(ubo->name,DESCRIPTOR_NAME_MAX_LENGTH,sbs.name);

        ubo->stage_flag=flag_bits;

        return ubo;
    }
}//namespace

/**
* 设置材质实例代码与数据长度
* @param glsl_codes     材质实例GLSL代码
* @param data_bytes     单个材质实例数据长度
* @param shader_stage_flag_bits   具体使用材质实例的shader
* @return 是否设置成功
*/
bool MaterialCreateInfo::SetMaterialInstance(const AnsiString &glsl_codes,const uint32_t data_bytes,const uint32_t shader_stage_flag_bits)
{
    if(mi_data_bytes>0)return(false);           //已经有数据了

    if(shader_stage_flag_bits==0)return(false);

    if(data_bytes>0&&glsl_codes.Length()<4)return(false);

    mi_data_bytes=data_bytes;

    if(data_bytes>0)
        mi_codes=glsl_codes;

    mi_max_count=hgl_min<uint32_t>(ubo_range/data_bytes,HGL_U16_MAX);

    mdi.AddStruct(MaterialInstanceStruct,mi_codes);
    mdi.AddStruct(SBS_MaterialInstance);

    mi_ubo=CreateUBODescriptor(SBS_MaterialInstance,shader_stage_flag_bits);

    mdi.AddUBO(shader_stage_flag_bits,DescriptorSetType::PerMaterial,mi_ubo);

    const AnsiString MI_MAX_COUNT=AnsiString::numberOf(mi_max_count);

    auto *it=shader_map.GetDataList();

    for(int i=0;i<shader_map.GetCount();i++)
    {
        if((*it)->key&shader_stage_flag_bits)
        {
            (*it)->value->AddDefine("MI_MAX_COUNT",MI_MAX_COUNT);
            (*it)->value->SetMaterialInstance(mi_ubo,mi_codes);
        }

        ++it;
    }

    mi_shader_stage=shader_stage_flag_bits;

    return(true);
}

bool MaterialCreateInfo::SetLocalToWorld(const uint32_t shader_stage_flag_bits)
{
    if(shader_stage_flag_bits==0)return(false);

    mdi.AddStruct(SBS_LocalToWorld);

    l2w_ubo=CreateUBODescriptor(SBS_LocalToWorld,shader_stage_flag_bits);

    mdi.AddUBO(shader_stage_flag_bits,DescriptorSetType::PerFrame,l2w_ubo);

    const AnsiString L2W_MAX_COUNT=AnsiString::numberOf(l2w_max_count);

    auto *it=shader_map.GetDataList();

    for(int i=0;i<shader_map.GetCount();i++)
    {
        if((*it)->key&shader_stage_flag_bits)
        {
            (*it)->value->AddDefine("L2W_MAX_COUNT",L2W_MAX_COUNT);
            (*it)->value->SetLocalToWorld(l2w_ubo);
        }

        ++it;
    }

    l2w_shader_stage=shader_stage_flag_bits;

    return(true);
}

bool MaterialCreateInfo::CreateShader()
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
            sc->AddOutput(VAT_UINT,mtl::func::MaterialInstanceID,Interpolation::Flat);

            sc->AddFunction(mtl::func::HandoverMI);
        }

        sc->CreateShader(last);

        last=sc;
    }

    return(true);
}
STD_MTL_NAMESPACE_END
