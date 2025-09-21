#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/shadergen/ShaderDescriptorInfo.h>
#include<hgl/graph/mtl/UBOCommon.h>
#include<hgl/graph/VKDeviceAttribute.h>
#include"common/MFCommon.h"
#include"common/MFGetPosition.h"
#include"ShaderLibrary.h"

using namespace hgl;
using namespace hgl::graph;

STD_MTL_NAMESPACE_BEGIN
MaterialCreateInfo::MaterialCreateInfo(const MaterialCreateConfig *mc)
{
    config=mc;

    if(hasVertex    ())shader_map.Add(vert=new ShaderCreateInfoVertex  (&mdi));else vert=nullptr;
    if(hasGeometry  ())shader_map.Add(geom=new ShaderCreateInfoGeometry(&mdi));else geom=nullptr;
    if(hasFragment  ())shader_map.Add(frag=new ShaderCreateInfoFragment(&mdi));else frag=nullptr;

    ubo_range=0;
    ssbo_range=0;

    {
        mi_data_bytes=0;
        mi_shader_stage=0;
        mi_max_count=0;
        mi_ubo=nullptr;
    }

    has_l2w_matrix=mc->local_to_world;
}

bool MaterialCreateInfo::AddStruct(const AnsiString &struct_name,const AnsiString &codes)
{
    if(struct_name.IsEmpty()||codes.IsEmpty())
        return(false);

    return mdi.AddStruct(struct_name,codes);
}

bool MaterialCreateInfo::AddUBO(const ShaderStage flag_bit,const DescriptorSetType set_type,const AnsiString &struct_name,const AnsiString &name)
{
    if(!shader_map.ContainsKey(flag_bit))
        return(false);

    if(!mdi.hasStruct(struct_name))
        return(false);

    ShaderCreateInfo *sc=shader_map[flag_bit];

    if(!sc)
        return(false);

    UBODescriptor *ubo=mdi.GetUBO(name);

    if(ubo)
    {
        if(ubo->type!=struct_name)
            return(false);

        ubo->stage_flag|=(uint32_t)flag_bit;

        return sc->AddUBO(set_type,ubo);
    }
    else
    {
        ubo=new UBODescriptor();

        ubo->type=struct_name;
        hgl::strcpy(ubo->name,DESCRIPTOR_NAME_MAX_LENGTH,name);

        return sc->AddUBO(set_type,mdi.AddUBO((uint32_t)flag_bit,set_type,ubo));
    }
}

bool MaterialCreateInfo::AddUBO(const uint32_t flag_bits,const DescriptorSetType &set_type,const AnsiString &struct_name,const AnsiString &name)
{
    if(flag_bits==0)return(false);          //没有任何SHADER用?

    if(!mdi.hasStruct(struct_name))
        return(false);
    
    uint result=0;
    ShaderStage bit;

    for(int i=0;i<shader_map.GetCount();i++)
    {
        shader_map.GetKey(i,bit);

        if(flag_bits&(uint32_t)bit)
            if(AddUBO(bit,set_type,struct_name,name))
                ++result;
    }

    return(result==shader_map.GetCount());
}

bool MaterialCreateInfo::AddUBOStruct(const uint32_t flag_bits,const ShaderBufferSource &ss)
{
    if(!AddStruct(ss.struct_name,ss.codes))
        return(false);

    return AddUBO(flag_bits,ss.set_type,ss.struct_name,ss.name);
}

bool MaterialCreateInfo::AddSampler(const ShaderStage flag_bit,const DescriptorSetType set_type,const SamplerType &st,const AnsiString &name)
{
    if(!shader_map.ContainsKey(flag_bit))
        return(false);

    RANGE_CHECK_RETURN_FALSE(st);

    ShaderCreateInfo *sc=shader_map[flag_bit];

    if(!sc)
        return(false);

    TextureSamplerDescriptor *image_sampler=mdi.GetTextureSampler(name);

    AnsiString st_name=GetSamplerTypeName(st);

    if(image_sampler)
    {
        if(image_sampler->type!=st_name)
            return(false);

        image_sampler->stage_flag|=(uint32_t)flag_bit;

        return sc->AddTextureSampler(set_type,image_sampler);
    }
    else
    {
        image_sampler=new TextureSamplerDescriptor();

        image_sampler->type=st_name;
        hgl::strcpy(image_sampler->name,DESCRIPTOR_NAME_MAX_LENGTH,name);

        return sc->AddTextureSampler(set_type,mdi.AddTextureSampler((uint32_t)flag_bit,set_type,image_sampler));
    }
}

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

    mdi.AddUBO(shader_stage_flag_bits,SBS_MaterialInstance.set_type,mi_ubo);

    const AnsiString MI_MAX_COUNT_STRING=AnsiString::numberOf(mi_max_count);

    auto *it=shader_map.GetDataList();

    for(int i=0;i<shader_map.GetCount();i++)
    {
        if(uint32_t((*it)->key)&shader_stage_flag_bits)
        {
            (*it)->value->AddDefine("MI_MAX_COUNT",MI_MAX_COUNT_STRING);
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

    l2w_max_count=hgl_min<uint32_t>(ubo_range/sizeof(Matrix4f),HGL_U16_MAX);

    mdi.AddStruct(SBS_LocalToWorld);

    l2w_ubo=CreateUBODescriptor(SBS_LocalToWorld,shader_stage_flag_bits);

    mdi.AddUBO(shader_stage_flag_bits,SBS_LocalToWorld.set_type,l2w_ubo);

    const AnsiString L2W_MAX_COUNT_STRING=AnsiString::numberOf(l2w_max_count);

    for(auto it:shader_map)
    {
        if(uint32_t(it->key)&shader_stage_flag_bits)
        {
            it->value->AddDefine("L2W_MAX_COUNT",L2W_MAX_COUNT_STRING);
        }
    }

    l2w_shader_stage=shader_stage_flag_bits;

    return(true);
}
//
//bool MaterialCreateInfo::SetWorldPosition(const uint32_t shader_stage_flag_bits)
//{
//    if(shader_stage_flag_bits==0)return(false);
//
//    {
//        vert->AddOutput(SVT_VEC4,"WorldPosition");
//
//        if(l2w_shader_stage)
//        {
//            vert->AddFunction(func::GetWorldPosition3DL2W_VS);
//        }
//        else
//        {
//            vert->AddFunction(func::GetWorldPosition3D_VS);
//        }
//    }
//
//    if(shader_stage_flag_bits&VK_SHADER_STAGE_GEOMETRY_BIT)
//    {
//        geom->AddOutput(SVT_VEC4,"WorldPosition");
//
//        geom->AddFunction(func::GetWorldPosition3D_Other);
//    }
//
//    if(shader_stage_flag_bits&VK_SHADER_STAGE_FRAGMENT_BIT)
//    {
//        geom->AddFunction(func::GetWorldPosition3D_Other);
//    }
//
//    return(true);
//}

void MaterialCreateInfo::SetDevice(const VulkanDevAttr *dev_attr)
{
    ubo_range=dev_attr->physical_device->GetUBORange();              //Mali-T系/G71为16k，nVidia和Mali-G系列除G71外为64k，Intel/PowerVR为128M，AMD无限制。
    ssbo_range=dev_attr->physical_device->GetSSBORange();
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

        if(static_cast<uint32_t>(sc->GetShaderStage())<mi_shader_stage)
            sc->AddMaterialInstanceOutput();

        sc->CreateShader(last);

        last=sc;
    }

    return(true);
}
STD_MTL_NAMESPACE_END
