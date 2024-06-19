#include<hgl/shadergen/ShaderCreateInfoFragment.h>
#include<hgl/shadergen/ShaderDescriptorInfo.h>

namespace hgl{namespace graph{
    
int ShaderCreateInfoFragment::AddOutput(const VAType &type,const AnsiString &name,Interpolation inter)
{
    VertexInputAttribute via;

    hgl::strcpy(via.name,sizeof(via.name),name.c_str());

    via.basetype        =(uint8)type.basetype;
    via.vec_size        =       type.vec_size;
    via.interpolation   =       inter;

    return fsdi.AddOutput(via);
}

int ShaderCreateInfoFragment::AddOutput(const AnsiString &type,const AnsiString &name,Interpolation inter)
{
    VAType vat;

    if(name.IsEmpty())
        return -1;

    if(!ParseVertexAttribType(&vat,type))
        return -2;

    return AddOutput(vat,name,inter);
}

bool ShaderCreateInfoFragment::ProcOutput()
{
    const auto &output_list=fsdi.GetOutput();

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