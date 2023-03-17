#include<hgl/shadergen/ShaderCreateInfoFragment.h>
#include<hgl/shadergen/ShaderDescriptorInfo.h>

SHADERGEN_NAMESPACE_BEGIN

using namespace hgl::graph;

void ShaderCreateInfoFragment::UseDefaultMain()
{
    main_codes="void main()\n{\n";

    const auto &output_list=sdm->GetShaderStageIO().output;

    const ShaderAttribute *o=output_list.items;

    for(uint i=0;i<output_list.count;i++)
    {
        main_codes+="\t";
        main_codes+=o->name;
        main_codes+="=Get";
        main_codes+=o->name;
        main_codes+="();\n";

        ++o;
    }

    main_codes+="}";
}

bool ShaderCreateInfoFragment::ProcOutput()
{
    const auto &output_list=sdm->GetShaderStageIO().output;

    const ShaderAttribute *o=output_list.items;

    for(uint i=0;i<output_list.count;i++)
    {
        final_shader+="layout(location=";
        final_shader+=AnsiString::numberOf(i);
        final_shader+=") out ";
        final_shader+=GetShaderAttributeTypename(o);
        final_shader+=" ";
        final_shader+=o->name;
        final_shader+=";\n";

        ++o;
    }

    return(true);
}
SHADERGEN_NAMESPACE_END