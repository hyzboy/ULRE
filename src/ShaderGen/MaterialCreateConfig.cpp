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

    hash=str;

    if(const char *prim_name=GetPrimName(prim))
        hash+=prim_name;
    else
        hash+="UnknownPrim";

    if(override_geometry_mode)
    {
        hash += "_GM";
        hash += std::to_string(static_cast<uint32_t>(geometry_mode_override));
    }

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

    if(camera)
        hash+="_Camera";

    if(sky)
        hash+="_Sky";

    hash+="_Amb";
    char amb_model_str[2]={(char)('0'+(uint8)sky_ambient_model),0};
    hash+=amb_model_str;

    if(local_to_world)
        hash+="_L2W";

    hash+="_";
    if(const char *attrib_name=GetVertexAttribName(&position_format))
        hash+=attrib_name;
    else
        hash+="UnknownVA";

    return hash;
}

std::string BillboardMaterialCreateConfig::ToHashStdString()
{
    std::string hash = Material3DCreateConfig::ToHashStdString();

    hash += fixed_size ? "_Fixed" : "_Dynamic";

    if (fixed_size && (pixel_size.x > 0 || pixel_size.y > 0))
    {
        hash += "_";
        hash += std::to_string(pixel_size.x);
        hash += "x";
        hash += std::to_string(pixel_size.y);
    }

    if (front_face == VK_FRONT_FACE_COUNTER_CLOCKWISE)
        hash += "_CCW";

    if (!texture_id.empty())
    {
        hash += "_";
        hash += texture_id;
    }

    switch (blend_mode)
    {
    case BlendMode::Masked:         hash += "_Masked";  break;
    case BlendMode::Dither:         hash += "_Dither";  break;
    case BlendMode::AlphaToCoverage: hash += "_A2C";    break;
    case BlendMode::Opaque:         hash += "_Opaque";  break;
    default: break; // Transparent is the default, no suffix needed
    }

    if (base_color_channel == TextureChannelHint::Grayscale)
        hash += "_Gray";

    return hash;
}

}//namespace hgl::graph::mtl
