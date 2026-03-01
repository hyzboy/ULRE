#include<hgl/shadergen/ShaderCreateInfo.h>
#include<hgl/shadergen/ShaderDescriptorInfo.h>
#include<hgl/graph/mtl/UBOCommon.h>
#include<cstring>
#include<cstdio>
#include<string>

#include"GLSLCompiler.h"
#include"common/MFCommon.h"

namespace hgl{namespace graph{

static bool CStrEq(const char *lhs,const char *rhs)
{
    return lhs&&rhs&&std::strcmp(lhs,rhs)==0;
}

static const char *AsCStr(const AnsiString &text)
{
    return text.c_str()?text.c_str():"";
}

static const char *AsCStr(const std::string &text)
{
    return text.c_str();
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

void ShaderCreateInfo::AddStruct(const AnsiString &name)
{
    return GetSDI()->AddStruct(name);
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
    AddUBO(DescriptorSetType::PerMaterial,ubo);
    AddStruct(mtl::MaterialInstanceStruct);

    AddFunction(shader_stage==ShaderStage::Vertex?mtl::func::MF_GetMI_VS:mtl::func::MF_GetMI_Other);

    mi_codes=mi;
}

void ShaderCreateInfo::SetMaterialInstance(SSBODescriptor *ssbo,const std::string &mi)
{
    AddSSBO(DescriptorSetType::PerMaterial,ssbo);
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
    final_shader+=AsCStr(last_output);

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

    output_struct=GetShaderStageName((VkShaderStageFlagBits)shader_stage);
    output_struct+="_Output\n{\n";

    AnsiString output_fields;
    GetOutputStrcutString(output_fields);
    output_struct+=AsCStr(output_fields);

    output_struct+="}";

    final_shader+="\nlayout(location=0) out ";
    final_shader+=AsCStr(output_struct);
    final_shader+="Output;\n";

    return(true);
}

bool ShaderCreateInfo::ProcStruct()
{
    const AnsiStringList &struct_list=GetSDI()->GetStructList();

    AnsiString codes;
    std::string block;

    for(auto str:struct_list)
    {
        if(!mdi->GetStruct(*str,codes))
            return(false);

        block+="\nstruct ";
        block+=AsCStr(*str);
        block+="\n{";
        block+=AsCStr(codes);
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
    auto ubo_list=GetSDI()->GetUBOList();

    const int count=ubo_list.GetCount();

    if(count<=0)return(true);

    auto ubo=ubo_list.GetData();

    AnsiString struct_codes;
    std::string block;
    block+="\n";

    for(int i=0;i<count;i++)
    {
        block+="layout(set=";
        const std::string ubo_set_str=std::to_string((*ubo)->set);
        block+=ubo_set_str;
        block+=",binding=";
        const std::string ubo_binding_str=std::to_string((*ubo)->binding);
        block+=ubo_binding_str;
        block+=") uniform ";
        block+=AsCStr((*ubo)->type);
        block+="\n{";

        if(!mdi->GetStruct((*ubo)->type,struct_codes))
            return(false);

        block+=AsCStr(struct_codes);

        block+="\n}";
        block+=(*ubo)->name;
        block+=";\n";

        ++ubo;
    }

    final_shader+=block.c_str();

    return(true);
}

bool ShaderCreateInfo::ProcSSBO()
{
    auto ssbo_list=GetSDI()->GetSSBOList();

    const int count=ssbo_list.GetCount();

    if(count<=0)return(true);

    auto ssbo=ssbo_list.GetData();

    AnsiString struct_codes;
    std::string block;
    block+="\n";

    for(int i=0;i<count;i++)
    {
        block+="layout(set=";
        const std::string ssbo_set_str=std::to_string((*ssbo)->set);
        block+=ssbo_set_str;
        block+=",binding=";
        const std::string ssbo_binding_str=std::to_string((*ssbo)->binding);
        block+=ssbo_binding_str;
        const char *ssbo_name = (*ssbo)->name;
        if(CStrEq(ssbo_name,"l2w")||CStrEq(ssbo_name,"mtl"))
            block+=") readonly buffer ";
        else
            block+=") buffer ";
        block+=AsCStr((*ssbo)->type);
        block+="\n{";

        if(!mdi->GetStruct((*ssbo)->type,struct_codes))
            return(false);

        block+=AsCStr(struct_codes);

        block+="\n}";
        block+=(*ssbo)->name;
        block+=";\n";

        ++ssbo;
    }

    final_shader+=block.c_str();

    return(true);
}

bool ShaderCreateInfo::ProcConstantID()
{
    auto const_list=GetSDI()->GetConstList();

    const int count=const_list.GetCount();

    if(count<=0)return(true);

    auto const_data=const_list.GetData();
    std::string block;
    block+="\n";

    for(int i=0;i<count;i++)
    {
        block+="layout(constant_id=";
        const std::string const_id_str=std::to_string((*const_data)->constant_id);
        block+=const_id_str;
        block+=") const ";
        block+=AsCStr((*const_data)->type);
        block+=" ";
        block+=AsCStr((*const_data)->name);
        block+="=";
        block+=AsCStr((*const_data)->value);
        block+=";\n";

        ++const_data;
    }

    final_shader+=block.c_str();

    return(true);
}

bool ShaderCreateInfo::ProcSampler()
{
    auto texture_sampler_list=GetSDI()->GetTextureSamplerList();

    const int count=texture_sampler_list.GetCount();

    if(count<=0)return(true);

    auto sampler=texture_sampler_list.GetData();
    std::string block;
    block+="\n";

    for(int i=0;i<count;i++)
    {
        block+="layout(set=";
        const std::string sampler_set_str=std::to_string((*sampler)->set);
        block+=sampler_set_str;
        block+=",binding=";
        const std::string sampler_binding_str=std::to_string((*sampler)->binding);
        block+=sampler_binding_str;
        block+=") uniform ";
        block+=AsCStr((*sampler)->type);
        block+=" ";
        block+=(*sampler)->name;
        block+=";\n";

        ++sampler;
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
    LogInfo(AnsiString(GetShaderStageName((VkShaderStageFlagBits)shader_stage))+" shader: \n"+AnsiString(final_shader.c_str()));

#endif//_DEBUG

    if(!CompileToSPV())
        return(false);

    return(true);
}

bool ShaderCreateInfo::CompileToSPV()
{
    spv_data=CompileShader((VkShaderStageFlagBits)shader_stage,final_shader.c_str());

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
