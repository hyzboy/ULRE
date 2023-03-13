#include<hgl/shadergen/ShaderCreaterFragment.h>

SHADERGEN_NAMESPACE_BEGIN

void ShaderCreaterFragment::UseDefaultMain()
{
    main_codes="void main()\n{\n";

    const auto &output_list=sdm.GetShaderStageIO().output;

    const uint count=output_list.GetCount();
    ShaderStage **o=output_list.GetData();

    for(uint i=0;i<count;i++)
    {
        main_codes+="\t";
        main_codes+=(*o)->name;
        main_codes+="=Get";
        main_codes+=(*o)->name;
        main_codes+="();\n";

        ++o;
    }

    main_codes+="}";
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