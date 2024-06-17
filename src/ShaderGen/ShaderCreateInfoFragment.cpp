#include<hgl/shadergen/ShaderCreateInfoFragment.h>
#include<hgl/shadergen/ShaderDescriptorInfo.h>

namespace hgl{namespace graph{

using namespace hgl::graph;

bool ShaderCreateInfoFragment::ProcOutput()
{
    const auto &output_list=sdi->GetShaderStageIO().output;

    const VertexInputAttribute *o=output_list.items;

        final_shader+="\n";

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
}}//namespace hgl::graph