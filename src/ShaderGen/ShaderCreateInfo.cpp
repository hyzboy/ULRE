#include<hgl/shadergen/ShaderCreateInfo.h>
#include<hgl/shadergen/ShaderDescriptorInfo.h>
#include"GLSLCompiler.h"
#include<hgl/graph/mtl/UBOCommon.h>
#include"common/MFCommon.h"

namespace hgl{namespace graph{
ShaderCreateInfo::ShaderCreateInfo(VkShaderStageFlagBits ss,MaterialDescriptorInfo *m)
{
    shader_stage=ss;
    mdi=m;
    sdm=new ShaderDescriptorInfo(ss);

    spv_data=nullptr;
}

ShaderCreateInfo::~ShaderCreateInfo()
{
    if(spv_data)
        FreeSPVData(spv_data);

    delete sdm;
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

    final_shader+="\n";

    return(true);
}

void ShaderCreateInfo::SetMaterialInstance(UBODescriptor *ubo)
{
    sdm->AddUBO(DescriptorSetType::PerMaterial,ubo);
    sdm->AddStruct(mtl::MaterialInstanceStruct);
    AddFunction(mtl::func::GetMI);
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

    final_shader+="layout(location=0) in ";
    final_shader+=last_output;
    final_shader+="Input;\n\n";

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
        output_struct+="\t";

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

    final_shader+="layout(location=0) out ";
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

        final_shader+="struct ";
        final_shader+=*str;
        final_shader+="\n{";
        final_shader+=codes;
        final_shader+="};\n\n";
    }

    return(true);
}

bool ShaderCreateInfo::ProcUBO()
{
    auto ubo_list=sdm->GetUBOList();

    const int count=ubo_list.GetCount();

    if(count<=0)return(true);

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
        final_shader+="{\n";

        if(!mdi->GetStruct((*ubo)->type,struct_codes))
            return(false);

        final_shader+=struct_codes;

        final_shader+="\n}";
        final_shader+=(*ubo)->name;
        final_shader+=";\n";

        ++ubo;
    }

    final_shader+="\n";
    return(true);
}

bool ShaderCreateInfo::ProcSSBO()
{
    return(false);
}

bool ShaderCreateInfo::ProcConst()
{
    auto const_list=sdm->GetConstList();

    const int count=const_list.GetCount();

    if(count<=0)return(true);

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

    final_shader+="\n";
    return(true);
}

bool ShaderCreateInfo::ProcSampler()
{
    auto sampler_list=sdm->GetSamplerList();

    const int count=sampler_list.GetCount();

    if(count<=0)return(true);

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
        final_shader+="\n";

        ++sampler;
    }

    final_shader+="\n";
    return(true);
}

bool ShaderCreateInfo::CreateShader(ShaderCreateInfo *last_sc)
{
    final_shader="#version 460 core\n";

    if(!ProcSubpassInput())
        return(false);
    if(!ProcInput(last_sc))
        return(false);
//    if(!ProcStruct())
//        return(false);

    if(!ProcUBO())
        return(false);
    //if(!ProcSSBO())
        //return(false);
    if(!ProcConst())
        return(false);
    if(!ProcSampler())
        return(false);

    ProcOutput();

    for(int i=0;i<function_list.GetCount();i++)
    {
        final_shader+="\n";
        final_shader+=function_list[i];
    }

#ifdef _DEBUG

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
