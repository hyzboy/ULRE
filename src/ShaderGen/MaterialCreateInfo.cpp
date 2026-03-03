#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/shadergen/ShaderDescriptorInfo.h>
#include<hgl/graph/mtl/UBOCommon.h>
#include<hgl/vk/VKDeviceAttribute.h>
#include<string>
#include"common/MFCommon.h"
#include"common/MFGetPosition.h"

using namespace hgl;
using namespace hgl::graph;

namespace hgl::graph::mtl{

static bool HasShaderStageBit(const uint32_t flag_bits,const ShaderStage stage)
{
    return (flag_bits & uint32_t(stage)) != 0;
}

template<typename Func>
static void ForEachShaderByStage(
    ShaderCreateInfoMap &shader_map,
    const uint32_t stage_bits,
    Func &&func)
{
    for(auto &kv:shader_map)
    {
        if(HasShaderStageBit(stage_bits,kv.first) && kv.second)
            func(*kv.second,kv.first);
    }
}

template<typename Func>
static bool ExecuteOnShadersByStage(
    ShaderCreateInfoMap &shader_map,
    const uint32_t stage_bits,
    Func &&func)
{
    uint expected=0;
    uint result=0;

    ForEachShaderByStage(shader_map,stage_bits,
        [&](ShaderCreateInfo &,ShaderStage stage)
        {
            ++expected;
            if(func(stage))
                ++result;
        });

    return expected>0&&result==expected;
}

static const UBODescriptor *ResolveUBODescriptor(
    MaterialDescriptorInfo &mdi,
    const ShaderStage flag_bit,
    const DescriptorSetType set_type,
    const std::string &struct_name,
    const std::string &name)
{
    UBODescriptor *ubo=mdi.GetUBO(name);

    if(ubo)
    {
        if(std::strcmp(ubo->type.c_str()?ubo->type.c_str():"",struct_name.c_str())!=0)
            return nullptr;

        ubo->stage_flag|=(uint32_t)flag_bit;
        return ubo;
    }

    ubo=new UBODescriptor();
    ubo->type=struct_name.c_str();
    hgl::strcpy(ubo->name,DESCRIPTOR_NAME_MAX_LENGTH,name.c_str());

    return mdi.AddUBO((uint32_t)flag_bit,set_type,ubo);
}

static const SSBODescriptor *ResolveSSBODescriptor(
    MaterialDescriptorInfo &mdi,
    const ShaderStage flag_bit,
    const DescriptorSetType set_type,
    const std::string &struct_name,
    const std::string &name)
{
    SSBODescriptor *ssbo=mdi.GetSSBO(name);

    if(ssbo)
    {
        if(std::strcmp(ssbo->type.c_str()?ssbo->type.c_str():"",struct_name.c_str())!=0)
            return nullptr;

        ssbo->stage_flag|=(uint32_t)flag_bit;
        return ssbo;
    }

    ssbo=new SSBODescriptor();
    ssbo->type=struct_name.c_str();
    hgl::strcpy(ssbo->name,DESCRIPTOR_NAME_MAX_LENGTH,name.c_str());

    return mdi.AddSSBO((uint32_t)flag_bit,set_type,ssbo);
}

static const TextureDescriptor *ResolveTextureDescriptor(
    MaterialDescriptorInfo &mdi,
    const ShaderStage flag_bit,
    const DescriptorSetType set_type,
    const std::string &type_name,
    const std::string &name)
{
    TextureDescriptor *texture=mdi.GetTexture(name);

    if(texture)
    {
        if(std::strcmp(texture->type.c_str()?texture->type.c_str():"",type_name.c_str())!=0)
            return nullptr;

        texture->stage_flag|=(uint32_t)flag_bit;
        return texture;
    }

    texture=new TextureDescriptor();
    texture->type=type_name.c_str();
    hgl::strcpy(texture->name,DESCRIPTOR_NAME_MAX_LENGTH,name.c_str());

    return mdi.AddTexture((uint32_t)flag_bit,set_type,texture);
}

static const TextureSamplerDescriptor *ResolveTextureSamplerDescriptor(
    MaterialDescriptorInfo &mdi,
    const ShaderStage flag_bit,
    const DescriptorSetType set_type,
    const std::string &type_name,
    const std::string &name)
{
    TextureSamplerDescriptor *image_sampler=mdi.GetTextureSampler(name);

    if(image_sampler)
    {
        if(std::strcmp(image_sampler->type.c_str()?image_sampler->type.c_str():"",type_name.c_str())!=0)
            return nullptr;

        image_sampler->stage_flag|=(uint32_t)flag_bit;
        return image_sampler;
    }

    image_sampler=new TextureSamplerDescriptor();
    image_sampler->type=type_name.c_str();
    hgl::strcpy(image_sampler->name,DESCRIPTOR_NAME_MAX_LENGTH,name.c_str());

    return mdi.AddTextureSampler((uint32_t)flag_bit,set_type,image_sampler);
}

MaterialCreateInfo::MaterialCreateInfo(const MaterialCreateConfig *mc)
    : config(*mc)
{
    if(hasVertex    ())shader_map.Add(vert=new ShaderCreateInfoVertex  (&mdi));else vert=nullptr;
    if(hasGeometry  ())shader_map.Add(geom=new ShaderCreateInfoGeometry(&mdi));else geom=nullptr;
    if(hasFragment  ())shader_map.Add(frag=new ShaderCreateInfoFragment(&mdi));else frag=nullptr;

    ubo_range=0;
    ssbo_range=0;

    {
        mi_data_bytes=0;
        mi_shader_stage=0;
        mi_max_count=0;
    #if defined(HGL_MI_USE_SSBO) && HGL_MI_USE_SSBO
        mi_ssbo=nullptr;
    #else
        mi_ubo=nullptr;
    #endif
    }

#if defined(HGL_L2W_USE_SSBO)
    l2w_ssbo=nullptr;
#else
    l2w_ubo=nullptr;
#endif

    has_l2w_matrix=config.local_to_world;
}

MaterialCreateInfo::~MaterialCreateInfo()
{
    // Explicitly clear the shader_map to properly clean up ShaderCreateInfo objects
    // This ensures proper destructor ordering and prevents crashes with UnorderedMap
    for(auto [stage, sc] : shader_map)
    {
        if(sc)
            delete sc;
    }
    shader_map.Clear();
}

bool MaterialCreateInfo::AddStruct(const std::string &struct_name,const std::string &codes)
{
    if(struct_name.empty()||codes.empty())
        return(false);

    return mdi.AddStruct(struct_name,codes);
}

bool MaterialCreateInfo::AddUBO(const ShaderStage flag_bit,const DescriptorSetType set_type,const std::string &struct_name,const std::string &name)
{
    if(!shader_map.ContainsKey(flag_bit))
        return(false);

    if(!mdi.hasStruct(struct_name))
        return(false);

    ShaderCreateInfo *sc=shader_map[flag_bit];

    if(!sc)
        return(false);

    const UBODescriptor *ubo=ResolveUBODescriptor(mdi,flag_bit,set_type,struct_name,name);
    if(!ubo)
        return false;

    return sc->AddUBO(set_type,ubo);
}

bool MaterialCreateInfo::AddUBO(const uint32_t flag_bits,const DescriptorSetType &set_type,const std::string &struct_name,const std::string &name)
{
    if(flag_bits==0)return(false);          //没有任何SHADER用?

    if(!mdi.hasStruct(struct_name))
        return(false);

    return ExecuteOnShadersByStage(shader_map,flag_bits,
        [&](const ShaderStage stage)
        {
            return AddUBO(stage,set_type,struct_name,name);
        });
}

bool MaterialCreateInfo::AddUBOStruct(const uint32_t flag_bits,const ShaderBufferSource &ss)
{
    if(!AddStruct(ss.struct_name,ss.codes))
        return(false);

    return AddUBO(flag_bits,ss.set_type,ss.struct_name,ss.name);
}

bool MaterialCreateInfo::AddSSBO(const ShaderStage flag_bit,const DescriptorSetType set_type,const std::string &struct_name,const std::string &name)
{
    if(!shader_map.ContainsKey(flag_bit))
        return(false);

    if(!mdi.hasStruct(struct_name))
        return(false);

    ShaderCreateInfo *sc=shader_map[flag_bit];

    if(!sc)
        return(false);

    const SSBODescriptor *ssbo=ResolveSSBODescriptor(mdi,flag_bit,set_type,struct_name,name);
    if(!ssbo)
        return false;

    return sc->AddSSBO(set_type,ssbo);
}

bool MaterialCreateInfo::AddSSBO(const uint32_t flag_bits,const DescriptorSetType &set_type,const std::string &struct_name,const std::string &name)
{
    if(flag_bits==0)return(false);          //没有任何SHADER用?

    if(!mdi.hasStruct(struct_name))
        return(false);

    return ExecuteOnShadersByStage(shader_map,flag_bits,
        [&](const ShaderStage stage)
        {
            return AddSSBO(stage,set_type,struct_name,name);
        });
}

bool MaterialCreateInfo::AddSSBOStruct(const uint32_t flag_bits,const ShaderBufferSource &ss)
{
    if(!AddStruct(ss.struct_name,ss.codes))
        return(false);

    return AddSSBO(flag_bits,ss.set_type,ss.struct_name,ss.name);
}

bool MaterialCreateInfo::AddTexture(const ShaderStage flag_bit,const DescriptorSetType set_type,const TextureType &tt,const std::string &name)
{
    if(!shader_map.ContainsKey(flag_bit))
        return(false);

    RANGE_CHECK_RETURN_FALSE(tt);

    ShaderCreateInfo *sc = shader_map[flag_bit];

    if(!sc)
        return(false);

    const std::string st_name(GetTextureTypeName(tt));        //这里可能需要根据纹理类型，在前面增加i/u的前缀

    const TextureDescriptor *texture=ResolveTextureDescriptor(mdi,flag_bit,set_type,st_name,name);
    if(!texture)
        return false;

    return sc->AddTexture(set_type,texture);
}

bool MaterialCreateInfo::AddTextureSampler(const ShaderStage flag_bit,const DescriptorSetType set_type,const SamplerType &st,const std::string &name)
{
    if(!shader_map.ContainsKey(flag_bit))
        return(false);

    RANGE_CHECK_RETURN_FALSE(st);

    ShaderCreateInfo *sc=shader_map[flag_bit];

    if(!sc)
        return(false);

    const std::string st_name(GetSamplerTypeName(st));      //这里可能需要根据纹理类型，在前面增加i/u的前缀

    const TextureSamplerDescriptor *image_sampler=ResolveTextureSamplerDescriptor(mdi,flag_bit,set_type,st_name,name);
    if(!image_sampler)
        return false;

    return sc->AddTextureSampler(set_type,image_sampler);
}

/**
* 设置材质实例代码与数据长度
* @param glsl_codes     材质实例GLSL代码
* @param data_bytes     单个材质实例数据长度
* @param shader_stage_flag_bits   具体使用材质实例的shader
* @return 是否设置成功
*/
bool MaterialCreateInfo::SetMaterialInstance(const std::string &glsl_codes,const uint32_t data_bytes,const uint32_t shader_stage_flag_bits)
{
    if(mi_data_bytes>0)return(false);           //已经有数据了

    if(shader_stage_flag_bits==0)return(false);

    if(data_bytes>0&&glsl_codes.size()<4)return(false);

    mi_data_bytes=data_bytes;

    if(data_bytes>0)
        mi_codes=glsl_codes;

    mdi.AddStruct(MaterialInstanceStruct,mi_codes); //外部指定的 struct MaterialInstance代码

    mdi.AddStruct(SBS_MaterialInstance);            //MaterialInstance mi[...];

#if defined(HGL_MI_USE_SSBO) && HGL_MI_USE_SSBO
    mi_max_count=std::min<uint32_t>(ssbo_range/data_bytes,HGL_U16_MAX);

    mi_ssbo=CreateSSBODescriptor(SBS_MaterialInstance,shader_stage_flag_bits);

    mdi.AddSSBO(shader_stage_flag_bits,SBS_MaterialInstance.set_type,mi_ssbo);
#else
    mi_max_count=std::min<uint32_t>(ubo_range/data_bytes,HGL_U16_MAX);

    mi_ubo=CreateUBODescriptor(SBS_MaterialInstance,shader_stage_flag_bits);

    mdi.AddUBO(shader_stage_flag_bits,SBS_MaterialInstance.set_type,mi_ubo);
#endif

    const std::string MI_MAX_COUNT_STRING=std::to_string(mi_max_count);

    ForEachShaderByStage(shader_map,shader_stage_flag_bits,
        [&](ShaderCreateInfo &shader,ShaderStage)
        {
            shader.AddDefine("MI_MAX_COUNT",MI_MAX_COUNT_STRING);
#if defined(HGL_MI_USE_SSBO) && HGL_MI_USE_SSBO
            shader.SetMaterialInstance(mi_ssbo,mi_codes);
#else
            shader.SetMaterialInstance(mi_ubo,mi_codes);
#endif
        });

    mi_shader_stage=shader_stage_flag_bits;

    return(true);
}

bool MaterialCreateInfo::SetLocalToWorld(const uint32_t shader_stage_flag_bits)
{
    if(shader_stage_flag_bits==0)return(false);

#if defined(HGL_L2W_USE_SSBO)
    l2w_max_count=std::min<uint32_t>(ssbo_range/sizeof(math::Matrix4f),HGL_U16_MAX);

    if(!AddSSBOStruct(shader_stage_flag_bits,SBS_LocalToWorld))
        return(false);

    l2w_ssbo=mdi.GetSSBO(SBS_LocalToWorld.name);
#else
    l2w_max_count=std::min<uint32_t>(ubo_range/sizeof(math::Matrix4f),HGL_U16_MAX);

    mdi.AddStruct(SBS_LocalToWorld);

    l2w_ubo=CreateUBODescriptor(SBS_LocalToWorld,shader_stage_flag_bits);

    mdi.AddUBO(shader_stage_flag_bits,SBS_LocalToWorld.set_type,l2w_ubo);
#endif

    const std::string L2W_MAX_COUNT_STRING=std::to_string(l2w_max_count);

    ForEachShaderByStage(shader_map,shader_stage_flag_bits,
        [&](ShaderCreateInfo &shader,ShaderStage)
        {
            shader.AddDefine("L2W_MAX_COUNT",L2W_MAX_COUNT_STRING);
// #if defined(HGL_L2W_USE_SSBO)
//             shader.AddDefine("L2W_USE_SSBO","1");
// #endif
        });

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
    if(!dev_attr||!dev_attr->physical_device)
    {
        ubo_range=0;
        ssbo_range=0;
        return;
    }

    ubo_range=dev_attr->physical_device->GetUBORange();              //Mali-T系/G71为16k，nVidia和Mali-G系列除G71外为64k，Intel/PowerVR为128M，AMD无限制。
    ssbo_range=dev_attr->physical_device->GetSSBORange();
}

bool MaterialCreateInfo::CreateShader()
{
    if(shader_map.IsEmpty())
        return(false);

    mdi.Resort();

    ShaderCreateInfo *last=nullptr;

    for(auto& kv : shader_map)
    {
        ShaderCreateInfo *sc = kv.second;

        if(static_cast<uint32_t>(sc->GetShaderStage())<mi_shader_stage)
            sc->AddMaterialInstanceOutput();

        sc->CreateShader(last);

        last=sc;
    }

    return(true);
}
}//namespace hgl::graph::mtl
