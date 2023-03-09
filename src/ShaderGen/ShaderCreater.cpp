#include<hgl/shadergen/ShaderCreater.h>

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

}

bool ShaderCreater::ProcInput(ShaderCreater *last_sc)
{
    if(!last_sc)
        return;

    AnsiString last_output=last_sc->GetOutputStruct();

    if(last_output.IsEmpty())
        return;

    final_shader+="layout(location=0) in ";
    final_shader+=last_output;
    final_shader+="Input;\n\n";
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
}

bool ShaderCreater::ProcStruct()
{
    const AnsiStringList struct_list=sdm.GetStructList();

    AnsiString codes;

    for(const AnsiString &str:struct_list)
    {
        if(!mdm->GetStruct(str,codes))
            return(false);
        
    }

    return(true);
}

bool ShaderCreater::ProcUBO()
{
}

bool ShaderCreater::ProcSSBO()
{
}

bool ShaderCreater::ProcConst()
{
}

bool ShaderCreater::CreateShader(ShaderCreater *last_sc)
{
    final_shader="#version 460 core\n";

    ProcInput(last_sc);

    ProcOutput();
}

bool ShaderCreater::CompileToSPV()
{
    if(shader_codes.IsEmpty())
        return(false);

    
}
SHADERGEN_NAMESPACE_END
