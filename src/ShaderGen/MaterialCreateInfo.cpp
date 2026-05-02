#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/shadergen/ShaderStageIO.h>
#include<hgl/shadergen/ShaderCreateInfoVertex.h>
#include<hgl/shadergen/device/DeviceProfile.h>
#include<hgl/mtl/UBOCommon.h>
#include<hgl/math/Matrix.h>
#include<hgl/mtl/MaterialVariantKey.h>
#include<hgl/shadergen/AttributeProviderRegistry.h>
#include<hgl/shadergen/PositionProviderRegistry.h>
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

template<typename SemanticT>
static bool ResolveSemanticMeta(
    const SemanticT semantic,
    const DescriptorSemanticMeta *&meta)
{
    if(!RangeCheck(semantic))
        return false;

    const DescriptorSemanticMeta &candidate = GetDescriptorSemanticMeta(semantic);

    if(!candidate.struct_name || !*candidate.struct_name)
        return false;

    if(!candidate.name || !*candidate.name)
        return false;

    meta = &candidate;
    return true;
}

static bool ResolveUBOSemanticMeta(
    const UBODescriptorSemantic semantic,
    const DescriptorSemanticMeta *&meta)
{
    return ResolveSemanticMeta(semantic, meta);
}

static bool ResolveSSBOSemanticMeta(
    const SSBODescriptorSemantic semantic,
    const DescriptorSemanticMeta *&meta)
{
    return ResolveSemanticMeta(semantic, meta);
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
    const mtl::SamplerSlot slot)
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
    const mtl::SamplerSlot slot,
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

MaterialCreateInfo::MaterialCreateInfo(const MaterialCreateConfig *mc)
    : config(*mc)
{
    if(HasVertex    ())shader_map.Add(new ShaderCreateInfoVertex(&descriptor_db));
    if(HasTask      ())shader_map.Add(new ShaderCreateInfo(new ShaderStageIO(ShaderStage::Task),&descriptor_db));
    if(HasMesh      ())shader_map.Add(new ShaderCreateInfo(new ShaderStageIO(ShaderStage::Mesh),&descriptor_db));
    if(HasFragment  ())shader_map.Add(new ShaderCreateInfo(new FragmentShaderStageIO(),&descriptor_db));

    ubo_range=0;
    ssbo_range=0;

    material_instance = MaterialInstanceBlock{};
    local_to_world = LocalToWorldBlock{0, 0, config.local_to_world};
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

bool MaterialCreateInfo::AddResolvedUBO(const uint32_t flag_bits,const DescriptorSetType &set_type,const UBODescriptorSemantic semantic,const std::string &struct_name,const std::string &name)
{
    if(flag_bits==0)return(false);          //没有任何SHADER用?

    if(!descriptor_db.hasUBOStruct(semantic))
        return(false);

    // Execute on all shader stages matching the flag_bits
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

bool MaterialCreateInfo::AddUBOStruct(const uint32_t flag_bits,const UBODescriptorSemantic semantic)
{
    const DescriptorSemanticMeta *meta = nullptr;

    if(!ResolveUBOSemanticMeta(semantic,meta))
        return false;

    if(!descriptor_db.AddUBOStruct(semantic))
        return false;

    return AddResolvedUBO(flag_bits,meta->set_type,semantic,meta->struct_name,meta->name);
}

bool MaterialCreateInfo::AddResolvedSSBO(const uint32_t flag_bits,const DescriptorSetType &set_type,const SSBODescriptorSemantic semantic,const std::string &struct_name,const std::string &name)
{
    if(flag_bits==0)return(false);          //没有任何SHADER用?

    if(!descriptor_db.hasSSBOStruct(semantic))
        return(false);

    // Execute on all shader stages matching the flag_bits
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

bool MaterialCreateInfo::AddSSBOStruct(const uint32_t flag_bits,const SSBODescriptorSemantic semantic)
{
    const DescriptorSemanticMeta *meta = nullptr;

    if(!ResolveSSBOSemanticMeta(semantic,meta))
        return false;

    if(!descriptor_db.AddSSBOStruct(semantic))
        return false;

    return AddResolvedSSBO(flag_bits,meta->set_type,semantic,meta->struct_name,meta->name);
}

bool MaterialCreateInfo::AddTexture(const ShaderStage flag_bit,const TextureType &tt,const SamplerSlot slot)
{
    RANGE_CHECK_RETURN_FALSE(tt);
    RANGE_CHECK_RETURN_FALSE(slot);

    const std::string st_name(GetTextureTypeName(tt));        //这里可能需要根据纹理类型，在前面增加i/u的前缀
    const std::string name=ToDescriptorName(slot);

    const TextureDescriptor *texture=ResolveTextureDescriptor(descriptor_db,flag_bit,SET_TYPE_MATERIAL,st_name,name,slot);
    return texture != nullptr;
}

bool MaterialCreateInfo::AddTextureSampler(const ShaderStage flag_bit,const SamplerType &st,const SamplerSlot slot,const TextureChannelHint channel_hint)
{
    RANGE_CHECK_RETURN_FALSE(st);
    RANGE_CHECK_RETURN_FALSE(slot);

    const std::string st_name(GetSamplerTypeName(st));      //这里可能需要根据纹理类型，在前面增加i/u的前缀
    const std::string name=ToDescriptorName(slot);

    const TextureSamplerDescriptor *image_sampler=ResolveTextureSamplerDescriptor(descriptor_db,flag_bit,SET_TYPE_MATERIAL,st_name,name,slot,channel_hint);
    return image_sampler != nullptr;
}

bool MaterialCreateInfo::AddTextureSampler(const uint32_t flag_bits,const SamplerType &st,const SamplerSlot slot,const TextureChannelHint channel_hint)
{
    RANGE_CHECK_RETURN_FALSE(st);
    RANGE_CHECK_RETURN_FALSE(slot);

    return ExecuteOnShadersByStage(shader_map,flag_bits,
        [&](const ShaderStage stage)
        {
            return AddTextureSampler(stage,st,slot,channel_hint);
        });
}

/**
* 设置材质实例数据长度
* @param data_bytes     单个材质实例数据长度
* @param shader_stage_flag_bits   具体使用材质实例的shader
* @return 是否设置成功
*/
bool MaterialCreateInfo::SetMaterialInstance(const uint32_t data_bytes,const uint32_t shader_stage_flag_bits)
{
    if(material_instance.stride>0)return(false);           //已经有数据了

    if(shader_stage_flag_bits==0)return(false);

    if(data_bytes==0)return(false);

    material_instance.stride=data_bytes;

    if(!descriptor_db.AddSSBOStruct(SSBODescriptorSemantic::MaterialBindingInstanceData))
        return false;

    material_instance.max_count=std::min<uint32_t>(ssbo_range/data_bytes,HGL_U16_MAX);

    SSBODescriptor *mi_ssbo=CreateSSBODescriptor(SSBODescriptorSemantic::MaterialBindingInstanceData,shader_stage_flag_bits);

    descriptor_db.AddSSBO(shader_stage_flag_bits,
                          GetDescriptorSemanticMeta(SSBODescriptorSemantic::MaterialBindingInstanceData).set_type,
                          mi_ssbo);

    material_instance.stage_bits=shader_stage_flag_bits;

    return(true);
}

bool MaterialCreateInfo::SetMaterialInstance(const ShaderDataSchema schema,
                                             const ShaderDataSchemaInfo &schema_info,
                                             const uint32_t shader_stage_flag_bits)
{
    if(schema==ShaderDataSchema::None)
        return false;

    if(schema_info.byte_size==0)
        return false;

    if(!SetMaterialInstance(schema_info.byte_size,shader_stage_flag_bits))
        return false;

    material_instance.schema=schema;
    material_instance.schema_file=schema_info.glsl_schema_file ? schema_info.glsl_schema_file : "";

    return true;
}

void MaterialCreateInfo::BuildBindingContract()
{
    binding_contract = DescriptorBindingSlots{};

    for(size_t i=0;i<UBODescriptorSemanticCount;++i)
    {
        const UBODescriptor *d = descriptor_db.GetUBO(UBODescriptorSemantic(i));
        if(d)
            binding_contract.ubos[i] = d->stage_flag;
    }

    for(size_t i=0;i<SSBODescriptorSemanticCount;++i)
    {
        const SSBODescriptor *d = descriptor_db.GetSSBO(SSBODescriptorSemantic(i));
        if(d)
            binding_contract.ssbos[i] = d->stage_flag;
    }
}

bool MaterialCreateInfo::SetLocalToWorld(const uint32_t shader_stage_flag_bits)
{
    if(shader_stage_flag_bits==0)return(false);

    local_to_world.max_count=std::min<uint32_t>(ssbo_range/sizeof(math::Matrix4f),HGL_U16_MAX);

    if(!AddSSBOStruct(shader_stage_flag_bits,SSBODescriptorSemantic::TransformData))
        return(false);

    local_to_world.stage_bits=shader_stage_flag_bits;
    local_to_world.enabled=true;

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

bool MaterialCreateInfo::CompileSPV()
{
    if(shader_map.IsEmpty())
        return(false);

    for(auto& kv : shader_map)
    {
        ShaderCreateInfo *sc = kv.second;

        if(!sc->CompileFinalGLSLToSPV())
            return(false);
    }

    return(true);
}

void MaterialCreateInfo::AddVertexStreamSSBOs(const MaterialVariantKey &key)
{
    uint32_t stream_stage_bits = uint32_t(ShaderStage::Vertex);
    if (HasMesh())
    {
        stream_stage_bits = uint32_t(ShaderStage::Mesh);
        if (HasTask())
            stream_stage_bits |= uint32_t(ShaderStage::Task);
    }

    // Attribute streams: binding index = AttributeSemantic ordinal (sparse binding).
    // CompositorAssembler emits "#define FETCH_<Tag>_SSBO_BINDING <i>" with the same index.
    for (size_t i = 0; i < key.attribute_providers.size(); ++i)
    {
        const AttributeProviderId pid = key.attribute_providers[i];
        if (pid == AttributeProviderId::None || pid == AttributeProviderId::Constant)
            continue;

        const AttributeProvider *ap = FindBuiltinAttribProvider(pid);
        if (!ap || !ap->needs_ssbo)
            continue;

        auto *sd = new SSBODescriptor();
        sd->semantic = SSBODescriptorSemantic(i);   // index → binding via Resort()
        descriptor_db.AddSSBO(stream_stage_bits, DescriptorSetType::VertexStreams, sd);
    }

    // Position stream: binding = BuiltinCount (= 8).
    // CompositorAssembler emits "#define POSITION_SSBO_BINDING 8".
    if (key.position_provider != PositionProviderId::DirectVec3)
    {
        const PositionProvider *pp = FindBuiltinProvider(key.position_provider);
        if (pp && pp->needs_ssbo)
        {
            auto *sd = new SSBODescriptor();
            // semantic index 8 = BuiltinCount; Resort() maps index → binding = 8
            sd->semantic = SSBODescriptorSemantic(size_t(AttributeSemantic::BuiltinCount));
            descriptor_db.AddSSBO(stream_stage_bits, DescriptorSetType::VertexStreams, sd);
        }
    }
}

void MaterialCreateInfo::AddMeshShaderStreamSSBOs(const MeshShaderStreamContract &contract)
{
    uint32_t stage_bits = uint32_t(ShaderStage::Mesh);
    if (HasTask())
        stage_bits |= uint32_t(ShaderStage::Task);

    auto add_stream_binding = [this](const uint32_t stage_bits,const uint32_t binding)
    {
        if(binding >= kVertexStreamBindingCountWithMesh)
            return;

        auto *sd = new SSBODescriptor();
        sd->semantic = SSBODescriptorSemantic(binding);  // VertexStreams: semantic index == binding
        descriptor_db.AddSSBO(stage_bits, DescriptorSetType::VertexStreams, sd);
    };

    add_stream_binding(stage_bits, contract.index.binding);

    if(contract.enable_meshlet_stream)
        add_stream_binding(stage_bits, contract.meshlet.binding);

    if(contract.enable_task_payload_stream)
        add_stream_binding(stage_bits, contract.task_payload.binding);
}
}//namespace hgl::graph::mtl
