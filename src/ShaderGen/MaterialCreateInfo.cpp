#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/shadergen/ShaderDescriptorInfo.h>
#include<hgl/shadergen/ShaderCreateInfoVertex.h>
#include<hgl/shadergen/contract/ShaderGenContract.h>
#include<hgl/mtl/UBOCommon.h>
#include<hgl/math/Matrix.h>
#include<string>
#include<limits>

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

static bool ResolveDescriptorSemanticMetaForKind(
    const DescriptorSemantic semantic,
    const DescriptorKind expected_kind,
    const DescriptorSemanticMeta *&meta)
{
    if(!IsBuiltinDescriptorSemantic(semantic))
        return false;

    const DescriptorSemanticMeta &candidate = GetDescriptorSemanticMeta(semantic);

    if(candidate.default_kind != expected_kind)
        return false;

    if(!candidate.struct_name || !*candidate.struct_name)
        return false;

    if(!candidate.name || !*candidate.name)
        return false;

    meta = &candidate;
    return true;
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
    if(hasVertex    ())shader_map.Add(new ShaderCreateInfoVertex(&descriptor_db));
    if(hasFragment  ())shader_map.Add(new ShaderCreateInfo(new FragmentShaderDescriptorInfo(),&descriptor_db));

    ubo_range=0;
    ssbo_range=0;

    {
        material_instance_stride=0;
        material_instance_stage_bits=0;
        material_instance_max_count=0;
        material_instance_ssbo=nullptr;
    }

    local_to_world_ssbo=nullptr;

    has_local_to_world=config.local_to_world;
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
    if(struct_name.empty())
        return(false);

    return descriptor_db.AddStruct(struct_name,codes);
}

bool MaterialCreateInfo::AddUBO(const ShaderStage flag_bit,const DescriptorSetType set_type,const std::string &struct_name,const std::string &name)
{
    if(!shader_map.ContainsKey(flag_bit))
        return(false);

    if(!descriptor_db.hasStruct(struct_name))
        return(false);

    ShaderCreateInfo *sc=shader_map[flag_bit];

    if(!sc)
        return(false);

    const UBODescriptor *ubo=ResolveUBODescriptor(descriptor_db,flag_bit,set_type,struct_name,name);
    if(!ubo)
        return false;

    return sc->AddUBO(set_type,ubo);
}

bool MaterialCreateInfo::AddUBO(const ShaderStage flag_bit,const DescriptorSemantic semantic)
{
    const DescriptorSemanticMeta *meta = nullptr;

    if(!ResolveDescriptorSemanticMetaForKind(semantic,DescriptorKind::UBO,meta))
        return false;

    return AddUBOStruct(uint32_t(flag_bit),semantic);
}

bool MaterialCreateInfo::AddUBO(const uint32_t flag_bits,const DescriptorSemantic semantic)
{
    const DescriptorSemanticMeta *meta = nullptr;

    if(!ResolveDescriptorSemanticMetaForKind(semantic,DescriptorKind::UBO,meta))
        return false;

    return AddUBOStruct(flag_bits,semantic);
}

bool MaterialCreateInfo::AddUBO(const uint32_t flag_bits,const DescriptorSetType &set_type,const std::string &struct_name,const std::string &name)
{
    if(flag_bits==0)return(false);          //没有任何SHADER用?

    if(!descriptor_db.hasStruct(struct_name))
        return(false);

    return ExecuteOnShadersByStage(shader_map,flag_bits,
        [&](const ShaderStage stage)
        {
            return AddUBO(stage,set_type,struct_name,name);
        });
}

bool MaterialCreateInfo::AddUBOStruct(const uint32_t flag_bits,const ShaderBufferSource &ss)
{
    if(!AddStruct(ss.struct_name,""))
        return(false);

    return AddUBO(flag_bits,ss.set_type,ss.struct_name,ss.name);
}

bool MaterialCreateInfo::AddUBOStruct(const uint32_t flag_bits,const DescriptorSemantic semantic)
{
    const DescriptorSemanticMeta *meta = nullptr;

    if(!ResolveDescriptorSemanticMetaForKind(semantic,DescriptorKind::UBO,meta))
        return false;

    if(!AddStruct(meta->struct_name,""))
        return false;

    return AddUBO(flag_bits,meta->set_type,meta->struct_name,meta->name);
}

bool MaterialCreateInfo::AddSSBO(const ShaderStage flag_bit,const DescriptorSetType set_type,const std::string &struct_name,const std::string &name)
{
    if(!shader_map.ContainsKey(flag_bit))
        return(false);

    if(!descriptor_db.hasStruct(struct_name))
        return(false);

    ShaderCreateInfo *sc=shader_map[flag_bit];

    if(!sc)
        return(false);

    const SSBODescriptor *ssbo=ResolveSSBODescriptor(descriptor_db,flag_bit,set_type,struct_name,name);
    if(!ssbo)
        return false;

    return sc->AddSSBO(set_type,ssbo);
}

bool MaterialCreateInfo::AddSSBO(const ShaderStage flag_bit,const DescriptorSemantic semantic)
{
    const DescriptorSemanticMeta *meta = nullptr;

    if(!ResolveDescriptorSemanticMetaForKind(semantic,DescriptorKind::SSBO,meta))
        return false;

    return AddSSBOStruct(uint32_t(flag_bit),semantic);
}

bool MaterialCreateInfo::AddSSBO(const uint32_t flag_bits,const DescriptorSemantic semantic)
{
    const DescriptorSemanticMeta *meta = nullptr;

    if(!ResolveDescriptorSemanticMetaForKind(semantic,DescriptorKind::SSBO,meta))
        return false;

    return AddSSBOStruct(flag_bits,semantic);
}

bool MaterialCreateInfo::AddSSBO(const uint32_t flag_bits,const DescriptorSetType &set_type,const std::string &struct_name,const std::string &name)
{
    if(flag_bits==0)return(false);          //没有任何SHADER用?

    if(!descriptor_db.hasStruct(struct_name))
        return(false);

    return ExecuteOnShadersByStage(shader_map,flag_bits,
        [&](const ShaderStage stage)
        {
            return AddSSBO(stage,set_type,struct_name,name);
        });
}

bool MaterialCreateInfo::AddSSBOStruct(const uint32_t flag_bits,const ShaderBufferSource &ss)
{
    if(!AddStruct(ss.struct_name,""))
        return(false);

    return AddSSBO(flag_bits,ss.set_type,ss.struct_name,ss.name);
}

bool MaterialCreateInfo::AddSSBOStruct(const uint32_t flag_bits,const DescriptorSemantic semantic)
{
    const DescriptorSemanticMeta *meta = nullptr;

    if(!ResolveDescriptorSemanticMetaForKind(semantic,DescriptorKind::SSBO,meta))
        return false;

    if(!AddStruct(meta->struct_name,""))
        return false;

    return AddSSBO(flag_bits,meta->set_type,meta->struct_name,meta->name);
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

    const TextureDescriptor *texture=ResolveTextureDescriptor(descriptor_db,flag_bit,set_type,st_name,name);
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

    const TextureSamplerDescriptor *image_sampler=ResolveTextureSamplerDescriptor(descriptor_db,flag_bit,set_type,st_name,name);
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
    if(material_instance_stride>0)return(false);           //已经有数据了

    if(shader_stage_flag_bits==0)return(false);

    if(data_bytes>0&&glsl_codes.size()<4)return(false);

    material_instance_stride=data_bytes;

    if(data_bytes>0)
        material_instance_glsl=glsl_codes;

    descriptor_db.AddStruct(MaterialInstanceStruct,material_instance_glsl); //外部指定的 struct MaterialInstance代码

    descriptor_db.AddStruct(SBS_MaterialInstance);            //MaterialInstance mi[...];

    material_instance_max_count=std::min<uint32_t>(ssbo_range/data_bytes,HGL_U16_MAX);

    material_instance_ssbo=CreateSSBODescriptor(SBS_MaterialInstance,shader_stage_flag_bits);

    descriptor_db.AddSSBO(shader_stage_flag_bits,SBS_MaterialInstance.set_type,material_instance_ssbo);

    ForEachShaderByStage(shader_map,shader_stage_flag_bits,
        [&](ShaderCreateInfo &shader,ShaderStage)
        {
        shader.SetMaterialInstance(material_instance_ssbo);
        });

    material_instance_stage_bits=shader_stage_flag_bits;

    return(true);
}

bool MaterialCreateInfo::SetLocalToWorld(const uint32_t shader_stage_flag_bits)
{
    if(shader_stage_flag_bits==0)return(false);

    local_to_world_max_count=std::min<uint32_t>(ssbo_range/sizeof(math::Matrix4f),HGL_U16_MAX);

    if(!AddSSBOStruct(shader_stage_flag_bits,SBS_LocalToWorld))
        return(false);

    local_to_world_ssbo=descriptor_db.GetSSBO(SBS_LocalToWorld.name);

    local_to_world_stage_bits=shader_stage_flag_bits;

    return(true);
}
//
void MaterialCreateInfo::SetDevice(const contract::PhysicalDeviceProfileLite *profile)
{
    if(!profile)
    {
        ubo_range=0;
        ssbo_range=0;
        return;
    }

    const uint64_t max_u32=std::numeric_limits<uint32_t>::max();
    const uint64_t profile_ubo=profile->limits.max_uniform_buffer_range;
    const uint64_t profile_ssbo=profile->limits.max_storage_buffer_range;

    ubo_range=static_cast<uint32_t>((profile_ubo>max_u32)?max_u32:profile_ubo);
    ssbo_range=static_cast<uint32_t>((profile_ssbo>max_u32)?max_u32:profile_ssbo);
}

bool MaterialCreateInfo::CreateShaderDirect()
{
    if(shader_map.IsEmpty())
        return(false);

    descriptor_db.Resort();

    for(auto& kv : shader_map)
    {
        ShaderCreateInfo *sc = kv.second;

        if(!sc->CompileFinalGLSLToSPV())
            return(false);
    }

    return(true);
}
}//namespace hgl::graph::mtl
