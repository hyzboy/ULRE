#include<hgl/mtl/Material2DCreateConfig.h>
#include<hgl/mtl/Material3DCreateConfig.h>
#include <hgl/common/PrimitiveTypeDef.h>
#include <hgl/common/VertexAttribDef.h>
#include<string>

namespace hgl::graph::mtl{
std::string MaterialCreateConfig::ToHashStdString()
{
    std::string hash;
    hash.reserve(128);

    hash.push_back('M');

    if(material_instance)
        hash.push_back('I');

    hash.push_back('_');
    hash.push_back(static_cast<char>('0' + rt_output.color));

    if(rt_output.depth)
        hash.push_back('D');
    if(rt_output.stencil)
        hash.push_back('S');

    hash.push_back('_');

    if(shader_stage_flag_bit&(uint32)ShaderStage::Vertex)
        hash.push_back('V');
    //不再支持Tessellation和GeometryShader
    if(shader_stage_flag_bit&(uint32)ShaderStage::Fragment)
        hash.push_back('F');
    if(shader_stage_flag_bit&(uint32)ShaderStage::Compute)
        hash.push_back('C');
    if(shader_stage_flag_bit&(uint32)ShaderStage::Mesh)
        hash.push_back('M');
    if(shader_stage_flag_bit&(uint32)ShaderStage::Task)
        hash.push_back('T');

    hash.push_back('_');

    if(const char *prim_name=GetPrimName(prim))
        hash+=prim_name;
    else
        hash+="UnknownPrim";

    if(texture_source_bits_override != 0)
    {
        hash += "_TSB";
        hash += std::to_string(texture_source_bits_override);
    }

    if(sampler_feature_bits_override != 0)
    {
        hash += "_SFB";
        hash += std::to_string(sampler_feature_bits_override);
    }

    return hash;
}

std::string Material2DCreateConfig::ToHashStdString()
{
    std::string hash=MaterialCreateConfig::ToHashStdString();

    if(const char *cs_name=GetCoordinateSystem2DName(coordinate_system))
        hash+=cs_name;
    else
        hash+="UnknownCS2D";

    if(local_to_world)
        hash+="_L2W";

    hash+="_";
    if(const char *attrib_name=GetVertexAttribName(&position_format))
        hash+=attrib_name;
    else
        hash+="UnknownVA";

    return hash;
}

std::string Material3DCreateConfig::ToHashStdString()
{
    std::string hash=MaterialCreateConfig::ToHashStdString();

    if(effective_feature_mask != 0)
    {
        // Phase 2: Effective feature mask takes precedence over individual flags
        hash += "_Feat";
        hash += std::to_string(effective_feature_mask);
    }

    hash+="_Amb";
    hash.push_back(static_cast<char>('0'+(uint8)sky_ambient_model));

    if(local_to_world)
        hash+="_L2W";

    hash+="_";
    if(const char *attrib_name=GetVertexAttribName(&position_format))
        hash+=attrib_name;
    else
        hash+="UnknownVA";

    if(coord_2d != graph::CoordinateSystem2D::NDC)
    {
        hash+="_2D";
        hash+=graph::GetCoordinateSystem2DName(coord_2d);
    }

    return hash;
}


}//namespace hgl::graph::mtl
