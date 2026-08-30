#include<hgl/mtl/ShaderBuildContext.h>
#include<hgl/mtl/ShaderCreateInfo.h>
#include<hgl/mtl/contract/ShaderGenContract.h>
#include<hgl/mtl/DescriptorResourceCatalog.h>
#include<hgl/graph/ShaderBufferSources.h>
#include<hgl/math/Matrix.h>
#include<string>
#include<limits>
using namespace hgl;
using namespace hgl::graph;

namespace hgl::graph::mtl{
    using namespace hgl::graph::mtl;

/// S1-T1.6：按语义从资源目录取固定资源行——消除调用点手写 "SBS_X + int(XBinding::Y)" 配对。
/// 未登记 / 无固定 SBS / 无固定绑定号（per-material 动态）三种情况均为**编译错误**。
template<DescriptorSemantic SEMANTIC>
static constexpr const DescriptorResourceCatalogEntry &CatalogFixedRow() noexcept
{
    constexpr const DescriptorResourceCatalogEntry *row=FindResourceCatalogEntry(SEMANTIC);

    static_assert(row!=nullptr,
                  "该语义未在 kDescriptorResourceCatalog 登记——先在资源目录加一行");
    static_assert(row->sbs!=nullptr,
                  "该语义无固定 SBS 行（动态命名资源不可走此路径）");
    static_assert(row->binding>=0,
                  "该语义无固定绑定号（per-material 动态分配，binding=-1）");

    return *row;
}

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
    if(has_mesh      ())shader_map.Add(new ShaderCreateInfo(ShaderStage::Mesh));
    if(has_fragment  ())shader_map.Add(new ShaderCreateInfo(ShaderStage::Fragment));

    ubo_range=0;
    ssbo_range=0;

    // Phase 7：补齐初始化——两成员曾未初始化，其值会经 SetLocalToWorld/
    // GetLocalToWorld 相关路径进入未定义行为
    local_to_world_max_count=0;
    local_to_world_stage_bits=0;
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

// ── AddUBO 家族已删除（Phase 7）：Scene UBO 全局化（P1）后引擎无 per-material
//    UBO 注册路径，全部实现为死代码 ──

bool ShaderBuildContext::AddSSBOCore(const ShaderStage flag_bit,const DescriptorSetType set_type,const std::string &struct_name,const std::string &name,const int preferred_binding)
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
            return AddSSBOCore(stage,set_type,struct_name,name,preferred_binding);
        });
}

bool ShaderBuildContext::AddSSBOStruct(const uint32_t flag_bits,const ShaderBufferSource &ss,const int preferred_binding)
{
    if(!AddStruct(ss.struct_name,""))
        return(false);

    return AddSSBO(flag_bits,ss.set_type,ss.struct_name,ss.name,preferred_binding);
}

// —— 语义化 SSBO 注册（MeshShader 方向：按用途明确区分）——
bool ShaderBuildContext::AddSSBOVertex(const uint32_t flag_bits,const ShaderBufferSource &ss,const int preferred_binding)
{
    return AddSSBOStruct(flag_bits,ss,preferred_binding);
}

bool ShaderBuildContext::AddSSBOVertexIndex(const uint32_t flag_bits)
{
    const DescriptorResourceCatalogEntry &row=CatalogFixedRow<DescriptorSemantic::VertexIndex>();

    return AddSSBOStruct(flag_bits,*row.sbs,row.binding);
}

bool ShaderBuildContext::AddSSBOMaterialPrivateData(const uint32_t flag_bits,const std::string &struct_name,const std::string &name,const int material_private_data_slot)
{
    return AddSSBO(flag_bits,DescriptorSetType::Material,struct_name,name,material_private_data_slot);
}

bool ShaderBuildContext::AddSSBOMaterialPrivateDataIndex(const uint32_t flag_bits)
{
    const DescriptorResourceCatalogEntry &row=CatalogFixedRow<DescriptorSemantic::MaterialPrivateDataIndex>();

    return AddSSBO(flag_bits,row.set_type,row.sbs->struct_name,row.sbs->name,row.binding);
}

bool ShaderBuildContext::AddSSBOTextureLayer(const uint32_t flag_bits,const int binding)
{
    return AddSSBO(flag_bits,DescriptorSetType::Material,SBS_MaterialTextureLayerRows.struct_name,SBS_MaterialTextureLayerRows.name,binding);
}

bool ShaderBuildContext::SetLocalToWorld(const uint32_t shader_stage_flag_bits)
{
    if(shader_stage_flag_bits==0)return(false);

    local_to_world_max_count=std::min<uint32_t>(ssbo_range/sizeof(math::Matrix4f),HGL_U16_MAX);

    const DescriptorResourceCatalogEntry &row=CatalogFixedRow<DescriptorSemantic::LocalToWorld>();

    if(!AddSSBOStruct(shader_stage_flag_bits,*row.sbs,row.binding))
        return(false);

    local_to_world_ssbo=descriptor_allocator.GetSSBO(row.sbs->name);

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
