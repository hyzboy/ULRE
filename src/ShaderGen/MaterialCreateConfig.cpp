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

    if(effective_feature_mask != 0)
    {
        // Phase 2: Effective feature mask takes precedence over individual flags
        hash += "_Feat";
        hash += std::to_string(effective_feature_mask);
    }
    else
    {
        // Fallback: old field-based hash path (backward compat when intent_features=0)
        if(camera)
            hash+="_Camera";

        if(sky)
            hash+="_Sky";
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
    case RenderAlphaMode::Masked:         hash += "_Masked";  break;
    case RenderAlphaMode::Dither:         hash += "_Dither";  break;
    case RenderAlphaMode::AlphaToCoverage:hash += "_A2C";     break;
    case RenderAlphaMode::Opaque:         hash += "_Opaque";  break;
    default: break; // Transparent is the default, no suffix needed
    }

    if (base_color_channel == TextureChannelHint::Grayscale)
        hash += "_Gray";

    if (use_texture_array)
        hash += "_TexArr";

    return hash;
}

}//namespace hgl::graph::mtl
