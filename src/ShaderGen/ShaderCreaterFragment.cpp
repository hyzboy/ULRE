#include<hgl/shadergen/ShaderCreaterFragment.h>

SHADERGEN_NAMESPACE_BEGIN

void ShaderCreaterFragment::UseDefaultMain()
{
    main_codes="void main()\n{\n";

    const auto &output_list=sdm.GetShaderStageIO().output;

    const uint count=output_list.GetCount();
    for(uint i=0;i<count;i++)
    {
        main_codes+="\t";
        main_codes+=output_list[i]->name;
        main_codes+="=Get";
        main_codes+=output_list[i]->name;
        main_codes+="();\n";
    }

    main_codes+="}";
}
SHADERGEN_NAMESPACE_END