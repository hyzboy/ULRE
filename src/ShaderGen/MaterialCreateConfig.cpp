#include <string>
#include<hgl/graph/mtl/Material2DCreateConfig.h>
#include<hgl/graph/mtl/Material3DCreateConfig.h>
#include<hgl/vk/VertexAttrib.h>

namespace hgl::graph::mtl{
const std::string MaterialCreateConfig::ToHashString()
{
    std::string hash;

    char str[16];
    char *p=str;

    *p='M';++p;

    if(material_instance){*p='I';++p;}

    *p='_';++p;

    *p='0'+rt_output.color;++p;

    if(rt_output.depth){*p='D';++p;}
    if(rt_output.stencil){*p='S';++p;}

    *p='_';++p;

    if(shader_stage_flag_bit&(uint32)ShaderStage::Vertex){*p='V';++p;}
    if(shader_stage_flag_bit&(uint32)ShaderStage::TessControl){*p='T';++p;}     //tc/te有一个就行了
    if(shader_stage_flag_bit&(uint32)ShaderStage::Geometry){*p='G';++p;}
    if(shader_stage_flag_bit&(uint32)ShaderStage::Fragment){*p='F';++p;}
    if(shader_stage_flag_bit&(uint32)ShaderStage::Compute){*p='C';++p;}
    if(shader_stage_flag_bit&(uint32)ShaderStage::Mesh){*p='M';++p;}     //mesh/task有一个就行了
    *p='_';++p;

    *p=0;

    hash=p;
    hash+=GetPrimName(prim);

    return hash;
}

const std::string Material2DCreateConfig::ToHashString()
{
    std::string hash=MaterialCreateConfig::ToHashString();

    hash+=GetCoordinateSystem2DName(coordinate_system);

    if(local_to_world)
        hash+="_L2W";

    hash+="_";
    hash+=GetVertexAttribName(&position_format);

    return hash;
}

const std::string Material3DCreateConfig::ToHashString()
{
    std::string hash=MaterialCreateConfig::ToHashString();

    if(camera)
        hash+="_Camera";

    if(local_to_world)
        hash+="_L2W";

    hash+="_";
    hash+=GetVertexAttribName(&position_format);

    return hash;
}
}//namespace hgl::graph::mtl
