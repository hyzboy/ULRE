#include<hgl/shadergen/ShaderCreaterFragment.h>

SHADERGEN_NAMESPACE_BEGIN

void ShaderCreaterFragment::UseDefaultMain()
{
    shader_codes="void main()\n{\n";

    const auto &output_list=sdm.GetShaderStageIO().output;

    const uint count=output_list.GetCount();
    ShaderStage **o=output_list.GetData();

    for(uint i=0;i<count;i++)
    {
        shader_codes+="\t";
        shader_codes+=(*o)->name;
        shader_codes+="=Get";
        shader_codes+=(*o)->name;
        shader_codes+="();\n";

        ++o;
    }

    shader_codes+="}";
}

bool ShaderCreaterFragment::ProcOutput()
{
    const auto &output_list=sdm.GetShaderStageIO().output;

    const uint count=output_list.GetCount();
    ShaderStage **o=output_list.GetData();

    for(uint i=0;i<count;i++)
    {
        final_shader+="layout(location=";
        final_shader+=AnsiString::numberOf(i);
        final_shader+=") out ";
        final_shader+=GetShaderStageTypeName(*o);
        final_shader+=" ";
        final_shader+=(*o)->name;
        final_shader+=";\n";

        ++o;
    }

    return(true);
}
SHADERGEN_NAMESPACE_END