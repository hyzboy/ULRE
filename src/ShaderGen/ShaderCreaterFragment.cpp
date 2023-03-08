#include<hgl/shadergen/ShaderCreaterFragment.h>

SHADERGEN_NAMESPACE_BEGIN

void ShaderCreaterFragment::UseDefaultMain()
{
    shader_codes="void main()\n{\n";

    const auto &output_list=sdm.GetShaderStageIO().output;

    const uint count=output_list.GetCount();
    for(uint i=0;i<count;i++)
    {
        shader_codes+="\t";
        shader_codes+=output_list[i]->name;
        shader_codes+="=Get";
        shader_codes+=output_list[i]->name;
        shader_codes+="();\n";
    }

    shader_codes+="}";
}
SHADERGEN_NAMESPACE_END