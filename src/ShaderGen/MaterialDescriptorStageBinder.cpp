#include<hgl/shadergen/MaterialDescriptorStageBinder.h>

namespace hgl::graph::mtl
{
static bool HasShaderStageBit(const uint32_t flag_bits,const ShaderStage stage)
{
    return (flag_bits & uint32_t(stage)) != 0;
}

template<typename Func>
static void ForEachShaderByStage(
    ShaderStageMap &shader_map,
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
    ShaderStageMap &shader_map,
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
    MaterialDescriptorDB &mdi,
    const ShaderStage flag_bit,
    const DescriptorSetType set_type,
    const std::string &struct_name,
    const std::string &name,
    const UBODescriptorSemantic semantic = UBODescriptorSemantic::Unknown)
{
    if(RangeCheck(semantic))
    {
        UBODescriptor *ubo = mdi.GetUBO(semantic);
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
        ubo->semantic=semantic;
        return mdi.AddUBO((uint32_t)flag_bit,set_type,ubo);
    }

    return nullptr;
}

static const SSBODescriptor *ResolveSSBODescriptor(
    MaterialDescriptorDB &mdi,
    const ShaderStage flag_bit,
    const DescriptorSetType set_type,
    const std::string &struct_name,
    const std::string &name,
    const SSBODescriptorSemantic semantic = SSBODescriptorSemantic::Unknown)
{
    if(RangeCheck(semantic))
    {
        SSBODescriptor *ssbo = mdi.GetSSBO(semantic);
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
        ssbo->semantic=semantic;
        return mdi.AddSSBO((uint32_t)flag_bit,set_type,ssbo);
    }

    return nullptr;
}

static const TextureDescriptor *ResolveTextureDescriptor(
    MaterialDescriptorDB &mdi,
    const ShaderStage flag_bit,
    const DescriptorSetType set_type,
    const std::string &type_name,
    const std::string &name,
    const SamplerSlot slot)
{
    TextureDescriptor *texture=mdi.GetTexture(slot);

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
    texture->slot=slot;

    return mdi.AddTexture((uint32_t)flag_bit,set_type,texture);
}

static const TextureSamplerDescriptor *ResolveTextureSamplerDescriptor(
    MaterialDescriptorDB &mdi,
    const ShaderStage flag_bit,
    const DescriptorSetType set_type,
    const std::string &type_name,
    const std::string &name,
    const SamplerSlot slot,
    const TextureChannelHint channel_hint=TextureChannelHint::RGBA)
{
    TextureSamplerDescriptor *image_sampler=mdi.GetTextureSampler(slot);

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
    image_sampler->slot=slot;
    image_sampler->channel_hint=channel_hint;

    return mdi.AddTextureSampler((uint32_t)flag_bit,set_type,image_sampler);
}

bool MaterialDescriptorStageBinder::AddResolvedUBO(MaterialDescriptorDB &descriptor_db,ShaderStageMap &shader_map,const uint32_t flag_bits,const DescriptorSetType &set_type,const UBODescriptorSemantic semantic,const std::string &struct_name,const std::string &name)
{
    if(flag_bits==0)return(false);

    if(!descriptor_db.hasUBOStruct(semantic))
        return(false);

    uint expected=0;
    uint result=0;

    ForEachShaderByStage(shader_map,flag_bits,
        [&](ShaderCreateInfo &,ShaderStage stage)
        {
            ++expected;
            const UBODescriptor *ubo=ResolveUBODescriptor(descriptor_db,stage,set_type,struct_name,name,semantic);
            if(ubo != nullptr)
                ++result;
        });

    return expected>0&&result==expected;
}

bool MaterialDescriptorStageBinder::AddResolvedSSBO(MaterialDescriptorDB &descriptor_db,ShaderStageMap &shader_map,const uint32_t flag_bits,const DescriptorSetType &set_type,const SSBODescriptorSemantic semantic,const std::string &struct_name,const std::string &name)
{
    if(flag_bits==0)return(false);

    if(!descriptor_db.hasSSBOStruct(semantic))
        return(false);

    uint expected=0;
    uint result=0;

    ForEachShaderByStage(shader_map,flag_bits,
        [&](ShaderCreateInfo &,ShaderStage stage)
        {
            ++expected;
            const SSBODescriptor *ssbo=ResolveSSBODescriptor(descriptor_db,stage,set_type,struct_name,name,semantic);
            if(ssbo != nullptr)
                ++result;
        });

    return expected>0&&result==expected;
}

bool MaterialDescriptorStageBinder::AddTexture(MaterialDescriptorDB &descriptor_db,const ShaderStage flag_bit,const TextureType &tt,const SamplerSlot slot)
{
    RANGE_CHECK_RETURN_FALSE(tt);
    RANGE_CHECK_RETURN_FALSE(slot);

    const std::string st_name(GetTextureTypeName(tt));
    const std::string name=ToDescriptorName(slot);

    const TextureDescriptor *texture=ResolveTextureDescriptor(descriptor_db,flag_bit,SET_TYPE_MATERIAL,st_name,name,slot);
    return texture != nullptr;
}

bool MaterialDescriptorStageBinder::AddTextureSampler(MaterialDescriptorDB &descriptor_db,const ShaderStage flag_bit,const SamplerType &st,const SamplerSlot slot,const TextureChannelHint channel_hint)
{
    RANGE_CHECK_RETURN_FALSE(st);
    RANGE_CHECK_RETURN_FALSE(slot);

    const std::string st_name(GetSamplerTypeName(st));
    const std::string name=ToDescriptorName(slot);

    const TextureSamplerDescriptor *image_sampler=ResolveTextureSamplerDescriptor(descriptor_db,flag_bit,SET_TYPE_MATERIAL,st_name,name,slot,channel_hint);
    return image_sampler != nullptr;
}

bool MaterialDescriptorStageBinder::AddTextureSampler(MaterialDescriptorDB &descriptor_db,ShaderStageMap &shader_map,const uint32_t flag_bits,const SamplerType &st,const SamplerSlot slot,const TextureChannelHint channel_hint)
{
    RANGE_CHECK_RETURN_FALSE(st);
    RANGE_CHECK_RETURN_FALSE(slot);

    return ExecuteOnShadersByStage(shader_map,flag_bits,
        [&](const ShaderStage stage)
        {
            return AddTextureSampler(descriptor_db,stage,st,slot,channel_hint);
        });
}
}//namespace hgl::graph::mtl
