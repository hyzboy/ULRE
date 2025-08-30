#include<hgl/graph/mtl/Material2DCreateConfig.h>
#include<hgl/graph/mtl/Material3DCreateConfig.h>
#include<hgl/graph/VertexAttrib.h>

STD_MTL_NAMESPACE_BEGIN
const AnsiString MaterialCreateConfig::ToHashString()
{
    AnsiString hash;

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

const AnsiString Material2DCreateConfig::ToHashString()
{
    AnsiString hash=MaterialCreateConfig::ToHashString();

    hash+=GetCoordinateSystem2DName(coordinate_system);

    if(local_to_world)
        hash+="_L2W";

    hash+="_";
    hash+=GetVertexAttribName(&position_format);

    return hash;
}

const AnsiString Material3DCreateConfig::ToHashString()
{
    AnsiString hash=MaterialCreateConfig::ToHashString();

    if(camera)
        hash+="_Camera";

    if(local_to_world)
        hash+="_L2W";

    hash+="_";
    hash+=GetVertexAttribName(&position_format);

    return hash;
}
STD_MTL_NAMESPACE_END
