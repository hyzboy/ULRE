#include<hgl/shadergen/ShaderStageIO.h>

namespace hgl{namespace graph{
ShaderStageIO::ShaderStageIO(ShaderStage flag_bit)
{
    stage_flag=flag_bit;

    mem_zero(push_constant);
}

ShaderStageIO::~ShaderStageIO()
{
    for(auto *p:const_value_list)
        delete p;
}

std::string ShaderStageIO::GetStageName()const
{
    switch(stage_flag)
    {
        case ShaderStage::Vertex: return "Vertex";
        case ShaderStage::TessControl: return "TessControl";
        case ShaderStage::TessEval: return "TessEval";
        case ShaderStage::Geometry: return "Geometry";
        case ShaderStage::Fragment: return "Fragment";
        case ShaderStage::Compute: return "Compute";
        case ShaderStage::ClusterCulling: return "ClusterCulling";
        default: return "Unknown";
    }
}

bool ShaderStageIO::AddConstValue(ConstValueDescriptor *sd)
{
    if(!sd)return(false);

    for(auto *p:const_value_list)
        if(p->name==sd->name)
            return(false);

    sd->constant_id=static_cast<int>(const_value_list.size());
    const_value_list.push_back(sd);
    return(true);
}

void ShaderStageIO::SetPushConstant(const std::string &name,uint8_t offset,uint8_t size)
{
    push_constant.name  =name;
    push_constant.offset=offset;
    push_constant.size  =size;
}
}}//namespace hgl::graph
