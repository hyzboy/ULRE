#include<hgl/shadergen/ShaderCreateInfoFragment.h>
#include<hgl/shadergen/ShaderDescriptorInfo.h>
#include<string>

namespace hgl{namespace graph{

int ShaderCreateInfoFragment::AddOutput(VIAList &via_list)
{
    int count=0;

    for(VIA &via:via_list)
    {
        //都输出了，没这些值
        //via.input_rate=VertexInputRate::Vertex;
        //via.group=VertexInputGroup::Basic;

        if(fsdi.AddOutput(via))
            ++count;
    }

    return count;
}

int ShaderCreateInfoFragment::AddOutput(const VAType &type,const std::string &name,Interpolation inter)
{
    VertexInputAttribute via;

    hgl::strcpy(via.name,sizeof(via.name),name.c_str());

    via.basetype        =(uint8)type.basetype;
    via.vec_size        =       type.vec_size;
    via.interpolation   =       inter;

    return fsdi.AddOutput(via);
}

int ShaderCreateInfoFragment::AddOutput(const char *type,const std::string &name,Interpolation inter)
{
    VAType vat;

    if(name.empty())
        return -1;

    if(!ParseVertexAttribType(&vat,type))
        return -2;

    return AddOutput(vat,name,inter);
}

bool ShaderCreateInfoFragment::ProcOutput()
{
    const auto &output_list=fsdi.GetOutput();

    const VertexInputAttribute *o=output_list.items;

    std::string block;
    block+="\n";

    for(uint i=0;i<output_list.count;i++)
    {
        block+="layout(location=";
        const std::string location_str=std::to_string(i);
        block+=location_str;
        block+=") out ";
        block+=GetShaderAttributeTypename(o);
        block+=" ";
        block+=o->name;
        block+=";\n";

        ++o;
    }

    final_shader+=block.c_str();

    return(true);
}
}}//namespace hgl::graph


