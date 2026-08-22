#include<hgl/mtl/ShaderBuildContext.h>
#include<hgl/mtl/ShaderCreateInfoVertex.h>
#include<hgl/mtl/contract/ShaderGenContract.h>
#include<hgl/graph/ShaderBufferSources.h>
#include<hgl/math/Matrix.h>
#include<string>
#include<limits>
using namespace hgl;
using namespace hgl::graph;

namespace hgl::graph::mtl{
    using namespace hgl::graph::mtl;

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
    DescriptorSetLayoutAllocator &allocator,
    const ShaderStage flag_bit,
    const DescriptorSetType set_type,
    const std::string &struct_name,
    const std::string &name)
{
    UBODescriptor *ubo=allocator.GetUBO(name);

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

    return allocator.AddUBO((uint32_t)flag_bit,set_type,ubo);
}

static const SSBODescriptor *ResolveSSBODescriptor(
    DescriptorSetLayoutAllocator &allocator,
    const ShaderStage flag_bit,
    const DescriptorSetType set_type,
    const std::string &struct_name,
    const std::string &name,
    const int preferred_binding)
{
    SSBODescriptor *ssbo=allocator.GetSSBO(name);

    if(ssbo)
    {
        if(std::strcmp(ssbo->type.c_str()?ssbo->type.c_str():"",struct_name.c_str())!=0)
            return nullptr;

        ssbo->stage_flag|=(uint32_t)flag_bit;
        if(preferred_binding>=0)
            ssbo->preferred_binding=preferred_binding;
        return ssbo;
    }

    ssbo=new SSBODescriptor();
    ssbo->type=struct_name.c_str();
    hgl::strcpy(ssbo->name,DESCRIPTOR_NAME_MAX_LENGTH,name.c_str());
    ssbo->preferred_binding=preferred_binding;

    return allocator.AddSSBO((uint32_t)flag_bit,set_type,ssbo);
}

ShaderBuildContext::ShaderBuildContext(const PrimitiveType primitive_type_value,const uint32_t shader_stage_bits,const bool has_local_to_world_value)
    : primitive_type(primitive_type_value), shader_stage_flag_bits(shader_stage_bits), has_local_to_world(has_local_to_world_value)
{
    if(has_vertex    ())shader_map.Add(new ShaderCreateInfoVertex());
    if(has_mesh      ())shader_map.Add(new ShaderCreateInfo(ShaderStage::Mesh));
    if(has_fragment  ())shader_map.Add(new ShaderCreateInfo(ShaderStage::Fragment));

    ubo_range=0;
    ssbo_range=0;

    local_to_world_ssbo=nullptr;

}

ShaderBuildContext::~ShaderBuildContext()
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

bool ShaderBuildContext::AddStruct(const std::string &struct_name,const std::string &codes)
{
    if(struct_name.empty())
        return(false);

    return descriptor_allocator.AddStruct(struct_name,codes);
}

bool ShaderBuildContext::AddUBO(const ShaderStage flag_bit,const DescriptorSetType set_type,const std::string &struct_name,const std::string &name)
{
    if(!shader_map.ContainsKey(flag_bit))
        return(false);

    if(!descriptor_allocator.hasStruct(struct_name))
        return(false);

    const UBODescriptor *ubo=ResolveUBODescriptor(descriptor_allocator,flag_bit,set_type,struct_name,name);
    return ubo != nullptr;
}

bool ShaderBuildContext::AddUBO(const uint32_t flag_bits,const DescriptorSetType &set_type,const std::string &struct_name,const std::string &name)
{
    if(flag_bits==0)return(false);          //没有任何SHADER用?

    if(!descriptor_allocator.hasStruct(struct_name))
        return(false);

    return ExecuteOnShadersByStage(shader_map,flag_bits,
        [&](const ShaderStage stage)
        {
            return AddUBO(stage,set_type,struct_name,name);
        });
}

bool ShaderBuildContext::AddUBOStruct(const uint32_t flag_bits,const ShaderBufferSource &ss)
{
    if(!AddStruct(ss.struct_name,""))
        return(false);

    return AddUBO(flag_bits,ss.set_type,ss.struct_name,ss.name);
}

bool ShaderBuildContext::AddSSBO(const ShaderStage flag_bit,const DescriptorSetType set_type,const std::string &struct_name,const std::string &name)
{
    return AddSSBO(flag_bit,set_type,struct_name,name,-1);
}

bool ShaderBuildContext::AddSSBO(const ShaderStage flag_bit,const DescriptorSetType set_type,const std::string &struct_name,const std::string &name,const int preferred_binding)
{
    if(!shader_map.ContainsKey(flag_bit))
        return(false);

    if(!descriptor_allocator.hasStruct(struct_name))
        return(false);

    const SSBODescriptor *ssbo=ResolveSSBODescriptor(descriptor_allocator,flag_bit,set_type,struct_name,name,preferred_binding);
    return ssbo != nullptr;
}

bool ShaderBuildContext::AddSSBO(const uint32_t flag_bits,const DescriptorSetType &set_type,const std::string &struct_name,const std::string &name)
{
    return AddSSBO(flag_bits,set_type,struct_name,name,-1);
}

bool ShaderBuildContext::AddSSBO(const uint32_t flag_bits,const DescriptorSetType &set_type,const std::string &struct_name,const std::string &name,const int preferred_binding)
{
    if(flag_bits==0)return(false);          //没有任何SHADER用?

    if(!descriptor_allocator.hasStruct(struct_name))
        return(false);

    return ExecuteOnShadersByStage(shader_map,flag_bits,
        [&](const ShaderStage stage)
        {
            return AddSSBO(stage,set_type,struct_name,name,preferred_binding);
        });
}

bool ShaderBuildContext::AddSSBOStruct(const uint32_t flag_bits,const ShaderBufferSource &ss)
{
    if(!AddStruct(ss.struct_name,""))
        return(false);

    return AddSSBO(flag_bits,ss.set_type,ss.struct_name,ss.name);
}

// —— 语义化 SSBO 注册（MeshShader 方向：按用途明确区分）——
bool ShaderBuildContext::AddSSBOVertex(const uint32_t flag_bits,const ShaderBufferSource &ss)
{
    return AddSSBOStruct(flag_bits,ss);
}

bool ShaderBuildContext::AddSSBOVertexIndex(const uint32_t flag_bits)
{
    return AddSSBOStruct(flag_bits,SBS_VertexIndex);
}

bool ShaderBuildContext::AddSSBOMtlData(const uint32_t flag_bits,const std::string &struct_name,const std::string &name,const int data_slot)
{
    return AddSSBO(flag_bits,DescriptorSetType::Material,struct_name,name,data_slot);
}

bool ShaderBuildContext::AddSSBOMtlIndex(const uint32_t flag_bits)
{
    return AddSSBO(flag_bits,SBS_MaterialDataIndexRows.set_type,SBS_MaterialDataIndexRows.struct_name,SBS_MaterialDataIndexRows.name);
}

bool ShaderBuildContext::AddSSBOTextureLayer(const uint32_t flag_bits,const int binding)
{
    return AddSSBO(flag_bits,DescriptorSetType::Material,SBS_MaterialTextureLayerRows.struct_name,SBS_MaterialTextureLayerRows.name,binding);
}

bool ShaderBuildContext::SetLocalToWorld(const uint32_t shader_stage_flag_bits)
{
    if(shader_stage_flag_bits==0)return(false);

    local_to_world_max_count=std::min<uint32_t>(ssbo_range/sizeof(math::Matrix4f),HGL_U16_MAX);

    if(!AddSSBOStruct(shader_stage_flag_bits,SBS_LocalToWorld))
        return(false);

    local_to_world_ssbo=descriptor_allocator.GetSSBO(SBS_LocalToWorld.name);

    local_to_world_stage_bits=shader_stage_flag_bits;

    return(true);
}
//
void ShaderBuildContext::SetDevice(const contract::PhysicalDeviceProfileLite *profile)
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

bool ShaderBuildContext::CreateShaderDirect()
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
}//namespace hgl::graph::mtl
