#include<hgl/shadergen/ShaderCreater.h>
#include"GLSLCompiler.h"

SHADERGEN_NAMESPACE_BEGIN
int ShaderCreater::AddOutput(const VAT &type,const AnsiString &name)
{
    ShaderStage *ss=new ShaderStage;

    hgl::strcpy(ss->name,sizeof(ss->name),name.c_str());

    ss->basetype=(uint8) type.basetype;
    ss->vec_size=        type.vec_size;

    return sdm.AddOutput(ss);
}

int ShaderCreater::AddOutput(const AnsiString &type,const AnsiString &name)
{
    VAT vat;

    if(name.IsEmpty())
        return -1;

    if(!ParseVertexAttribType(&vat,type))
        return -2;

    return AddOutput(vat,name);
}

bool ShaderCreater::ProcSubpassInput()
{
    auto sil=sdm.GetSubpassInputList();

    if(sil.IsEmpty())
        return(true);

    const auto si=sil.GetData();
    const int si_count=sil.GetCount();

    for(int i=0;i<si_count;i++)
    {
        final_shader+="layout(input_attachment_index=";
        final_shader+=AnsiString::numberOf((*si)->input_attachment_index);
        final_shader+=", binding=";
        final_shader+=AnsiString::numberOf((*si)->binding);
        final_shader+=") uniform subpassInput ";
        final_shader+=(*si)->name;
        final_shader+=";\n";
    }

    final_shader+="\n";

    return(true);

}

bool ShaderCreater::ProcInput(ShaderCreater *last_sc)
{
    if(!last_sc)
        return(false);

    AnsiString last_output=last_sc->GetOutputStruct();

    if(last_output.IsEmpty())
        return(true);

    final_shader+="layout(location=0) in ";
    final_shader+=last_output;
    final_shader+="Input;\n\n";

    return(true);
}

bool ShaderCreater::ProcOutput()
{
    final_shader+="layout(location=0) out ";

    output_struct.Clear();

    output_struct=GetShaderStageName(shader_stage);
    output_struct+="_Output\n{\n";

    for(auto *ss:sdm.GetShaderStageIO().output)
    {
        output_struct+="\t";
        output_struct+=GetShaderStageTypeName(ss);
        output_struct+=" ";
        output_struct+=ss->name;
        output_struct+=";\n";
    }

    output_struct+="}";

    final_shader+=output_struct;
    final_shader+="Output;\n\n";

    return(true);
}

bool ShaderCreater::ProcStruct()
{
    const AnsiStringList struct_list=sdm.GetStructList();

    AnsiString codes;

    for(auto &str:struct_list)
    {
        if(!mdm->GetStruct(*str,codes))
            return(false);

        final_shader+="struct ";
        final_shader+=*str;
        final_shader+="\n{\n";
        final_shader+=codes;
        final_shader+="};\n\n";
    }

    return(true);
}

bool ShaderCreater::ProcUBO()
{
    auto ubo_list=sdm.GetUBOList();

    const int count=ubo_list.GetCount();

    if(count<=0)return(true);

    auto ubo=ubo_list.GetData();

    for(int i=0;i<count;i++)
    {
        final_shader+="layout(set=";
        final_shader+=AnsiString::numberOf((*ubo)->set);
        final_shader+=",binding=";
        final_shader+=AnsiString::numberOf((*ubo)->binding);
        final_shader+=") uniform ";
        final_shader+=(*ubo)->type;
        final_shader+=" ";
        final_shader+=(*ubo)->name;
        final_shader+="\n";

        ++ubo;
    }

    final_shader+="\n";
    return(true);
}

bool ShaderCreater::ProcSSBO()
{
    return(false);
}

bool ShaderCreater::ProcConst()
{auto const_list=sdm.GetConstList();

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

bool ShaderCreater::ProcSampler()
{
    auto sampler_list=sdm.GetSamplerList();

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

bool ShaderCreater::CreateShader(ShaderCreater *last_sc)
{
    final_shader="#version 460 core\n";

    if(!ProcSubpassInput())
        return(false);
    if(!ProcInput(last_sc))
        return(false);
    if(!ProcStruct())
        return(false);

    if(!ProcUBO())
        return(false);
    //if(!ProcSSBO())
        //return(false);
    if(!ProcConst())
        return(false);
    if(!ProcSampler())
        return(false);

    ProcOutput();

    final_shader+="\n";

    final_shader+=main_codes;

    return(true);
}

bool ShaderCreater::CompileToSPV()
{
    if(main_codes.IsEmpty())
        return(false);

    glsl_compiler::SPVData *spv_data=glsl_compiler::Compile(shader_stage,final_shader.c_str());

    if(!spv_data)
        return(false);


}
SHADERGEN_NAMESPACE_END
