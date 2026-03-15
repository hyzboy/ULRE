#include<hgl/shadergen/ShaderCreateInfo.h>
#include<hgl/shadergen/ShaderDescriptorInfo.h>
#include<hgl/shadergen/MaterialDescriptorInfo.h>
#include<hgl/mtl/UBOCommon.h>
#include<cstring>
#include<cstdio>
#include<string>

#include"GLSLCompiler.h"
#include"common/MFCommon.h"

namespace hgl{namespace graph{

static const char *GetShaderStageNameByStage(const ShaderStage stage)
{
    switch (stage)
    {
        case ShaderStage::Vertex: return "Vertex";
        case ShaderStage::TessControl: return "TessControl";
        case ShaderStage::TessEval: return "TessEval";
        case ShaderStage::Geometry: return "Geometry";
        case ShaderStage::Fragment: return "Fragment";
        case ShaderStage::Compute: return "Compute";
        case ShaderStage::Task: return "Task";
        case ShaderStage::Mesh: return "Mesh";
        case ShaderStage::ClusterCulling: return "ClusterCulling";
        default: return "Unknown";
    }
}

static bool CStrEq(const char *lhs,const char *rhs)
{
    return lhs&&rhs&&std::strcmp(lhs,rhs)==0;
}

ShaderCreateInfo::ShaderCreateInfo()
{
    mem_zero(shader_stage);
    mdi=nullptr;

    spv_data=nullptr;
}

void ShaderCreateInfo::Init(ShaderDescriptorInfo *sdi,MaterialDescriptorInfo *m)
{
    shader_stage=sdi->GetShaderStage();
    mdi=m;
}

ShaderCreateInfo::~ShaderCreateInfo()
{
    if(spv_data)
        FreeSPVData(spv_data);
}

bool ShaderCreateInfo::AddDefine(const std::string &m,const std::string &v)
{
    if(!define_macro_set.emplace(m).second)
        return false;

    std::string line;
    line.reserve(10u+m.size()+v.size());
    line+="#define ";
    line+=m;
    line+=" ";
    line+=v;
    line+="\n";

    define_line_set.emplace(std::move(line));

    return(true);
}

bool ShaderCreateInfo::ProcDefine()
{
    if(define_line_set.empty())return(true);

    final_shader+="\n";

    for(const auto &line:define_line_set)
    {
        final_shader+=line.c_str();
    }

    return(true);
}

void ShaderCreateInfo::AddStruct(const std::string &name)
{
    return GetSDI()->AddStruct(name.c_str());
}

bool ShaderCreateInfo::AddUBO(DescriptorSetType type,const UBODescriptor *sd)
{
    return GetSDI()->AddUBO(type,sd);
}

bool ShaderCreateInfo::AddSSBO(DescriptorSetType type,const SSBODescriptor *sd)
{
    return GetSDI()->AddSSBO(type,sd);
}


bool ShaderCreateInfo::AddTexture(DescriptorSetType type,const TextureDescriptor *sd)
{
    return GetSDI()->AddTexture(type,sd);
}

bool ShaderCreateInfo::AddTextureSampler(DescriptorSetType type,const TextureSamplerDescriptor *sd)
{
    return GetSDI()->AddTextureSampler(type,sd);
}

void ShaderCreateInfo::SetMaterialInstance(UBODescriptor *ubo,const std::string &mi)
{
    AddUBO(DescriptorSetType::Material,ubo);
    AddStruct(mtl::MaterialInstanceStruct);

    AddFunction(shader_stage==ShaderStage::Vertex?mtl::func::MF_GetMI_VS:mtl::func::MF_GetMI_Other);

    mi_codes=mi;
}

void ShaderCreateInfo::SetMaterialInstance(SSBODescriptor *ssbo,const std::string &mi)
{
    AddSSBO(DescriptorSetType::Material,ssbo);
    AddStruct(mtl::MaterialInstanceStruct);

    AddFunction(shader_stage==ShaderStage::Vertex?mtl::func::MF_GetMI_VS:mtl::func::MF_GetMI_Other);

    mi_codes=mi;
}

bool ShaderCreateInfo::ProcInput(ShaderCreateInfo *last_sc)
{
    if(!last_sc)
        return(false);

    const std::string &last_output=last_sc->GetOutputStruct();

    if(last_output.empty())
    {
        final_shader+="\n";
        return(true);
    }

    final_shader+="\nlayout(location=0) in ";
    final_shader+=last_output;

    if(shader_stage==ShaderStage::Geometry)
        final_shader+="Input[];\n";
    else
        final_shader+="Input;\n";

    return(true);
}

bool ShaderCreateInfo::ProcOutput()
{
    output_struct.clear();

    if(IsEmptyOutput())
        return(true);

    output_struct=GetShaderStageNameByStage(shader_stage);
    output_struct+="_Output\n{\n";

    std::string output_fields;
    GetOutputStrcutString(output_fields);
    output_struct+=output_fields;

    output_struct+="}";

    final_shader+="\nlayout(location=0) out ";
    final_shader+=output_struct;
    final_shader+="Output;\n";

    return(true);
}

bool ShaderCreateInfo::ProcStruct()
{
    std::string codes;
    std::string block;
    const auto struct_names=GetSDI()->GetStructNameList();

    for(const auto &struct_name:struct_names)
    {
        if(!mdi->GetStruct(struct_name,codes))
            return(false);

        block+="\nstruct ";
        block+=struct_name;
        block+="\n{";
        block+=codes;
        block+="};\n";
    }

    final_shader+=block.c_str();

    return(true);
}

bool ShaderCreateInfo::ProcMI()
{
    if(mi_codes.empty())
        return(true);

    std::string block;
    block.reserve(mi_codes.size()+32u);
    block+="\nstruct MaterialInstance\n{\n";
    block+=mi_codes;
    block+="\n};\n";

    final_shader+=block.c_str();
    return(true);
}

bool ShaderCreateInfo::ProcUBO()
{
    const auto &ubo_list=GetSDI()->GetUBOList();

    if(ubo_list.empty())return(true);

    std::string struct_codes;
    std::string block;
    block+="\n";

    for(const auto *ubo:ubo_list)
    {
        block+="layout(set=";
        const std::string ubo_set_str=std::to_string(ubo->set);
        block+=ubo_set_str;
        block+=",binding=";
        const std::string ubo_binding_str=std::to_string(ubo->binding);
        block+=ubo_binding_str;
        block+=") uniform ";
        block+=(ubo->type.c_str()?ubo->type.c_str():"");
        block+="\n{";

        const std::string ubo_type=(ubo->type.c_str()?ubo->type.c_str():"");
        if(!mdi->GetStruct(ubo_type,struct_codes))
            return(false);

        block+=struct_codes;

        block+="\n}";
        block+=ubo->name;
        block+=";\n";
    }

    final_shader+=block.c_str();

    return(true);
}

bool ShaderCreateInfo::ProcSSBO()
{
    const auto &ssbo_list=GetSDI()->GetSSBOList();

    if(ssbo_list.empty())return(true);

    std::string struct_codes;
    std::string block;
    block+="\n";

    for(const auto *ssbo:ssbo_list)
    {
        block+="layout(set=";
        const std::string ssbo_set_str=std::to_string(ssbo->set);
        block+=ssbo_set_str;
        block+=",binding=";
        const std::string ssbo_binding_str=std::to_string(ssbo->binding);
        block+=ssbo_binding_str;
        const char *ssbo_name = ssbo->name;
        if(CStrEq(ssbo_name,"l2w")||CStrEq(ssbo_name,"mtl"))
            block+=") readonly buffer ";
        else
            block+=") buffer ";
        block+=(ssbo->type.c_str()?ssbo->type.c_str():"");
        block+="\n{";

        const std::string ssbo_type=(ssbo->type.c_str()?ssbo->type.c_str():"");
        if(!mdi->GetStruct(ssbo_type,struct_codes))
            return(false);

        block+=struct_codes;

        block+="\n}";
        block+=ssbo->name;
        block+=";\n";
    }

    final_shader+=block.c_str();

    return(true);
}

bool ShaderCreateInfo::ProcConstantID()
{
    const auto &const_list=GetSDI()->GetConstList();

    if(const_list.empty())return(true);

    std::string block;
    block+="\n";

    for(const auto *const_data:const_list)
    {
        block+="layout(constant_id=";
        const std::string const_id_str=std::to_string(const_data->constant_id);
        block+=const_id_str;
        block+=") const ";
        block+=(const_data->type.c_str()?const_data->type.c_str():"");
        block+=" ";
        block+=(const_data->name.c_str()?const_data->name.c_str():"");
        block+="=";
        block+=(const_data->value.c_str()?const_data->value.c_str():"");
        block+=";\n";
    }

    final_shader+=block.c_str();

    return(true);
}

bool ShaderCreateInfo::ProcSampler()
{
    const auto &texture_sampler_list=GetSDI()->GetTextureSamplerList();

    if(texture_sampler_list.empty())return(true);

    std::string block;
    block+="\n";

    for(const auto *sampler:texture_sampler_list)
    {
        block+="layout(set=";
        const std::string sampler_set_str=std::to_string(sampler->set);
        block+=sampler_set_str;
        block+=",binding=";
        const std::string sampler_binding_str=std::to_string(sampler->binding);
        block+=sampler_binding_str;
        block+=") uniform ";
        block+=(sampler->type.c_str()?sampler->type.c_str():"");
        block+=" ";
        block+=sampler->name;
        block+=";\n";
    }

    final_shader+=block.c_str();

    return(true);
}

bool ShaderCreateInfo::CreateShader(ShaderCreateInfo *last_sc)
{
    if(main_function.empty())
        return(false);

    final_shader=R"(
#version 460 core

#define VertexShader        0x01
#define TessControlShader   0x02
#define TeseEvalShader      0x04
#define GeometryShader      0x08
#define FragmentShader      0x10
#define ComputeShader       0x20
#define TaskShader          0x40
#define MeshShader          0x80
)";
//#define Float     float
//#define Float2    vec2
//#define Float3    vec3
//#define Float4    vec4
//
//#define Integer   int
//#define Int2      ivec2
//#define Int3      ivec3
//#define Int4      ivec4
//
//#define UInteger  uint
//#define Uint2     uvec2
//#define Uint3     uvec3
//#define Uint4     uvec4
//
//#define Boolean   bool
//#define Bool2     bvec2
//#define Bool3     bvec3
//#define Bool4     bvec4
//
//#define Double    double
//#define Double2   dvec2
//#define Double3   dvec3
//#define Double4   dvec4
//
//#define Matrix2   mat2
//#define Matrix3   mat3
//#define Matrix4   mat4
//
//#define Matrix2x3 mat2x3
//#define Matrix2x4 mat2x4
//
//#define Matrix3x2 mat3x2
//#define Matrix3x4 mat3x4
//
//#define Matrix4x2 mat4x2
//#define Matrix4x3 mat4x3
//)";

    {
        char ss_hex_str[16];
        std::snprintf(ss_hex_str,sizeof(ss_hex_str),"%08X",uint(shader_stage));

        std::string header_ext;
        header_ext.reserve(40);
        header_ext += "\n#define ShaderStage         0x";
        header_ext += ss_hex_str;
        header_ext += "\n";
        final_shader += header_ext.c_str();
    }

    ProcDefine();

    if(!ProcLayout())
        return(false);

    if(!ProcInput(last_sc))
        return(false);
//    if(!ProcStruct())
//        return(false);

    ProcMI();

    if(!ProcUBO())
        return(false);
    if(!ProcSSBO())
        return(false);
    if(!ProcConstantID())
        return(false);
    if(!ProcSampler())
        return(false);

    ProcOutput();

    {
        std::string tail;

        for(const char *str:user_data_list)
        {
            if(str)
                tail += str;
        }

        for(const char *str:function_list)
        {
            if(str)
                tail += str;
        }

        tail += "\n";
        tail += main_function;

        final_shader += tail.c_str();
    }

#ifdef _DEBUG

    //想办法存成文件或是输出行号，以方便出错了调试
    std::string log_text=GetShaderStageNameByStage(shader_stage);
    log_text+=" shader: \n";
    log_text+=final_shader;
    LogInfo(log_text.c_str());

#endif//_DEBUG

    if(!CompileToSPV())
        return(false);

    return(true);
}

bool ShaderCreateInfo::CreateShaderFromFinalGLSL()
{
    if(final_shader.empty())
        return(false);

#ifdef _DEBUG
    std::string log_text=GetShaderStageNameByStage(shader_stage);
    log_text+=" shader (direct): \n";
    log_text+=final_shader;
    LogInfo(log_text.c_str());
#endif

    if(!CompileToSPV())
        return(false);

    return(true);
}

bool ShaderCreateInfo::CompileToSPV()
{
    spv_data=CompileShader(uint32_t(shader_stage),final_shader.c_str());

    if(!spv_data)
        return(false);

    return(true);
}

const uint32 *ShaderCreateInfo::GetSPVData()const
{
    return spv_data?spv_data->spv_data:nullptr;
}

const size_t ShaderCreateInfo::GetSPVSize()const
{
    return spv_data?spv_data->spv_length:0;
}
}}//namespace hgl::graph
