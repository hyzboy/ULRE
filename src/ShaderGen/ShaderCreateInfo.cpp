#include<hgl/shadergen/ShaderCreateInfo.h>
#include<hgl/shadergen/ShaderDescriptorInfo.h>
#include<hgl/graph/mtl/UBOCommon.h>
#include<hgl/log/LogInfo.h>

#include"GLSLCompiler.h"
#include"common/MFCommon.h"
#include"ShaderLibrary.h"

namespace hgl{namespace graph{

ShaderCreateInfo::ShaderCreateInfo(VkShaderStageFlagBits ss,MaterialDescriptorInfo *m)
{
    shader_stage=ss;
    mdi=m;
    sdm=new ShaderDescriptorInfo(ss);

    spv_data=nullptr;

    define_macro_max_length=0;
    define_value_max_length=0;
}

ShaderCreateInfo::~ShaderCreateInfo()
{
    if(spv_data)
        FreeSPVData(spv_data);

    delete sdm;
}

bool ShaderCreateInfo::AddDefine(const AnsiString &m,const AnsiString &v)
{
    if(define_macro_list.Find(m)!=-1)
        return(false);

    define_macro_list.Add(m);
    define_value_list.Add(v);

    if(m.Length()>define_macro_max_length)
        define_macro_max_length=m.Length();

    if(v.Length()>define_value_max_length)
        define_value_max_length=v.Length();

    return(true);
}

bool ShaderCreateInfo::ProcDefine()
{
    const uint count=define_macro_list.GetCount();

    if(count<=0)return(true);

    final_shader+="\n";

    constexpr const char GLSL_DEFINE_FRONT[]="#define ";
    constexpr const uint GLSL_DEFINE_FRONT_LENGTH=sizeof(GLSL_DEFINE_FRONT)-1;

    const uint32_t total_length=GLSL_DEFINE_FRONT_LENGTH+define_macro_max_length+define_value_max_length+3;

    char *tmp=new char[total_length];
    char *p;

    memcpy(tmp,GLSL_DEFINE_FRONT,GLSL_DEFINE_FRONT_LENGTH);

    uint macro_length;
    uint value_length;

    AnsiString m;
    AnsiString v;

    for(uint i=0;i<count;i++)
    {
        m=define_macro_list.GetString(i);
        v=define_value_list.GetString(i);

        macro_length=m.Length();
        value_length=v.Length();

        p=tmp+GLSL_DEFINE_FRONT_LENGTH;

        memcpy(p,m.c_str(),macro_length);

        p+=macro_length;

        *p=' ';
        ++p;

        memcpy(p,v.c_str(),value_length);

        p+=value_length;
        *p='\n';

        final_shader.Strcat(tmp,p-tmp+1);
    }

    delete[] tmp;

    return(true);
}

int ShaderCreateInfo::AddOutput(const VAT &type,const AnsiString &name,Interpolation inter)
{
    ShaderAttribute *ss=new ShaderAttribute;

    hgl::strcpy(ss->name,sizeof(ss->name),name.c_str());

    ss->basetype        =(uint8)type.basetype;
    ss->vec_size        =       type.vec_size;
    ss->interpolation   =       inter;

    return sdm->AddOutput(ss);
}

int ShaderCreateInfo::AddOutput(const AnsiString &type,const AnsiString &name,Interpolation inter)
{
    VAT vat;

    if(name.IsEmpty())
        return -1;

    if(!ParseVertexAttribType(&vat,type))
        return -2;

    return AddOutput(vat,name,inter);
}

bool ShaderCreateInfo::ProcSubpassInput()
{
    auto sil=sdm->GetSubpassInputList();

    if(sil.IsEmpty())
        return(true);

    final_shader+="\n";

    auto si=sil.GetData();
    int si_count=sil.GetCount();

    for(int i=0;i<si_count;i++)
    {
        final_shader+="layout(input_attachment_index=";
        final_shader+=AnsiString::numberOf((*si)->input_attachment_index);
        final_shader+=", binding=";
        final_shader+=AnsiString::numberOf((*si)->binding);
        final_shader+=") uniform subpassInput ";
        final_shader+=(*si)->name;
        final_shader+=";\n";

        ++si;
    }

    return(true);
}

namespace
{
    constexpr const char MF_GetMI_VS    []="\nMaterialInstance GetMI(){return mtl.mi[Assign.y];}\n";
    constexpr const char MF_GetMI_Other []="\nMaterialInstance GetMI(){return mtl.mi[Input.MaterialInstanceID];}\n";

    constexpr const char MF_HandoverMI_VS[]=    "\nvoid HandoverMI(){Output.MaterialInstanceID=Assign.y;}\n";
    constexpr const char MF_HandoverMI_GS[]=    "\nvoid HandoverMI(){Output.MaterialInstanceID=Input[0].MaterialInstanceID;}\n";
    constexpr const char MF_HandoverMI_OTHER[]= "\nvoid HandoverMI(){Output.MaterialInstanceID=Input.MaterialInstanceID;}\n";
}//namespace

void ShaderCreateInfo::SetMaterialInstance(UBODescriptor *ubo,const AnsiString &mi)
{
    sdm->AddUBO(DescriptorSetType::PerMaterial,ubo);
    sdm->AddStruct(mtl::MaterialInstanceStruct);

    AddFunction(shader_stage==VK_SHADER_STAGE_VERTEX_BIT?MF_GetMI_VS:MF_GetMI_Other);

    mi_codes=mi;
}

void ShaderCreateInfo::AddMaterialInstanceOutput()
{
    AddOutput(VAT_UINT,mtl::func::MaterialInstanceID,Interpolation::Flat);

    if(shader_stage==VK_SHADER_STAGE_VERTEX_BIT)    AddFunction(MF_HandoverMI_VS);else
    if(shader_stage==VK_SHADER_STAGE_GEOMETRY_BIT)  AddFunction(MF_HandoverMI_GS);else
                                                    AddFunction(MF_HandoverMI_OTHER);
}

void ShaderCreateInfo::SetLocalToWorld(UBODescriptor *ubo)
{
    sdm->AddUBO(DescriptorSetType::PerFrame,ubo);
}

bool ShaderCreateInfo::ProcInput(ShaderCreateInfo *last_sc)
{
    if(!last_sc)
        return(false);

    AnsiString last_output=last_sc->GetOutputStruct();

    if(last_output.IsEmpty())
    {
        final_shader+="\n";
        return(true);
    }

    final_shader+="\nlayout(location=0) in ";
    final_shader+=last_output;

    if(shader_stage==VK_SHADER_STAGE_GEOMETRY_BIT)
        final_shader+="Input[];\n";
    else
        final_shader+="Input;\n";

    return(true);
}

bool ShaderCreateInfo::ProcOutput()
{
    output_struct.Clear();

    const ShaderAttributeArray &ssd=sdm->GetShaderStageIO().output;

    if(ssd.count<=0)return(true);

    output_struct=GetShaderStageName(shader_stage);
    output_struct+="_Output\n{\n";

    const ShaderAttribute *ss=ssd.items;

    for(uint i=0;i<ssd.count;i++)
    {
        output_struct+="    ";

        if(ss->interpolation!=Interpolation::Smooth)
        {
            output_struct+=InterpolationName[size_t(ss->interpolation)];

            output_struct+=" ";
        }

        output_struct+=GetShaderAttributeTypename(ss);
        output_struct+=" ";
        output_struct+=ss->name;
        output_struct+=";\n";

        ++ss;
    }

    output_struct+="}";

    final_shader+="\nlayout(location=0) out ";
    final_shader+=output_struct;
    final_shader+="Output;\n";

    return(true);
}

bool ShaderCreateInfo::ProcStruct()
{
    const AnsiStringList &struct_list=sdm->GetStructList();

    AnsiString codes;

    for(auto &str:struct_list)
    {
        if(!mdi->GetStruct(*str,codes))
            return(false);

        final_shader+="\nstruct ";
        final_shader+=*str;
        final_shader+="\n{";
        final_shader+=codes;
        final_shader+="};\n";
    }

    return(true);
}

bool ShaderCreateInfo::ProcMI()
{
    if(mi_codes.IsEmpty())
        return(true);

    final_shader+="\nstruct MaterialInstance\n{\n";
    final_shader+=mi_codes;
    final_shader+="\n};\n";
    return(true);
}

bool ShaderCreateInfo::ProcUBO()
{
    auto ubo_list=sdm->GetUBOList();

    const int count=ubo_list.GetCount();

    if(count<=0)return(true);

    final_shader+="\n";

    auto ubo=ubo_list.GetData();

    AnsiString struct_codes;

    for(int i=0;i<count;i++)
    {
        final_shader+="layout(set=";
        final_shader+=AnsiString::numberOf((*ubo)->set);
        final_shader+=",binding=";
        final_shader+=AnsiString::numberOf((*ubo)->binding);
        final_shader+=") uniform ";
        final_shader+=(*ubo)->type;
        final_shader+="\n{";

        if(!mdi->GetStruct((*ubo)->type,struct_codes))
            return(false);

        final_shader+=struct_codes;

        final_shader+="\n}";
        final_shader+=(*ubo)->name;
        final_shader+=";\n";

        ++ubo;
    }

    return(true);
}

bool ShaderCreateInfo::ProcSSBO()
{
    return(false);
}

bool ShaderCreateInfo::ProcConstantID()
{
    auto const_list=sdm->GetConstList();

    const int count=const_list.GetCount();

    if(count<=0)return(true);

    final_shader+="\n";

    auto const_data=const_list.GetData();

    for(int i=0;i<count;i++)
    {
        final_shader+="layout(constant_id=";
        final_shader+=AnsiString::numberOf((*const_data)->constant_id);
        final_shader+=") const ";
        final_shader+=(*const_data)->type;
        final_shader+=" ";
        final_shader+=(*const_data)->name;
        final_shader+="=";
        final_shader+=(*const_data)->value;
        final_shader+=";\n";

        ++const_data;
    }

    return(true);
}

bool ShaderCreateInfo::ProcSampler()
{
    auto sampler_list=sdm->GetSamplerList();

    const int count=sampler_list.GetCount();

    if(count<=0)return(true);

    final_shader+="\n";

    auto sampler=sampler_list.GetData();

    for(int i=0;i<count;i++)
    {
        final_shader+="layout(set=";
        final_shader+=AnsiString::numberOf((*sampler)->set);
        final_shader+=",binding=";
        final_shader+=AnsiString::numberOf((*sampler)->binding);
        final_shader+=") uniform ";
        final_shader+=(*sampler)->type;
        final_shader+=" ";
        final_shader+=(*sampler)->name;
        final_shader+=";\n";

        ++sampler;
    }

    return(true);
}

bool ShaderCreateInfo::CreateShader(ShaderCreateInfo *last_sc)
{
    if(main_function.IsEmpty())
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

    {
        char ss_hex_str[9];

        final_shader+="\n#define ShaderStage         0x";
        final_shader+=utos(ss_hex_str,8,uint(shader_stage),16);
        final_shader+="\n";
    }

    ProcDefine();

    if(!ProcLayout())
        return(false);

    if(!ProcSubpassInput())
        return(false);
    if(!ProcInput(last_sc))
        return(false);
//    if(!ProcStruct())
//        return(false);

    ProcMI();

    if(!ProcUBO())
        return(false);
    //if(!ProcSSBO())
        //return(false);
    if(!ProcConstantID())
        return(false);
    if(!ProcSampler())
        return(false);

    ProcOutput();

    for(const char *str:function_list)
        final_shader+=str;

    final_shader+="\n";
    final_shader+=main_function;

#ifdef _DEBUG

    //想办法存成文件或是输出行号，以方便出错了调试
    LOG_INFO(AnsiString(GetShaderStageName(shader_stage))+" shader: \n"+final_shader);

#endif//_DEBUG

    if(!CompileToSPV())
        return(false);

    return(true);
}

bool ShaderCreateInfo::CompileToSPV()
{
    spv_data=CompileShader(shader_stage,final_shader.c_str());

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
