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

void ShaderCreater::ProcInput()
{
    
}

void ShaderCreater::ProcOutput()
{
    final_shader+="layout(location=0) out ";
    final_shader+=GetShaderStageName(shader_stage);
    final_shader+="_Output\n{";

    output_struct.Clear();

    for(auto *ss:sdm.GetShaderStageIO().output)
    {
        output_struct+="\t";
        output_struct+=GetShaderStageTypeName(ss);
        output_struct+=" ";
        output_struct+=ss->name;
        output_struct+=";\n";
    }

    final_shader+=output_struct;
    final_shader+="}Output;\n\n";
}

bool ShaderCreater::CreateShader(ShaderCreater *last_sc)
{
    final_shader="#version 460 core\n";

    
}

bool ShaderCreater::CompileToSPV()
{
    if(shader_codes.IsEmpty())
        return(false);

    
}
SHADERGEN_NAMESPACE_END
