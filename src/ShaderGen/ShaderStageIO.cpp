#include<hgl/shadergen/ShaderStageIO.h>
#include<memory>

namespace hgl{namespace graph{
ShaderStageIO::ShaderStageIO(ShaderStage flag_bit)
{
    stage_flag=flag_bit;

    mem_zero(push_constant);
}

ShaderStageIO::~ShaderStageIO()
    =default;

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
    std::unique_ptr<ConstValueDescriptor> owned(sd);

    if(!owned)return(false);

    for(const auto &p:const_value_list)
        if(p&&p->name==owned->name)
            return(false);

    owned->constant_id=static_cast<int>(const_value_list.size());
    const_value_list.push_back(std::move(owned));
    return(true);
}

void ShaderStageIO::SetPushConstant(const std::string &name,uint8_t offset,uint8_t size)
{
    push_constant.name  =name;
    push_constant.offset=offset;
    push_constant.size  =size;
}
}}//namespace hgl::graph
