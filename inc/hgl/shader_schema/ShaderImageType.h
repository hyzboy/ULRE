#pragma once

#include<hgl/type/EnumUtil.h>
#include<hgl/type/StrChar.h>

namespace hgl::shader_schema
{
enum class ShaderImageType:uint8
{
    Error,

    Image1D,
    Image2D,
    Image3D,

    ImageCube,
    Image2DRect,

    Image1DArray,
    Image2DArray,

    ImageCubeArray,

    ImageBuffer,

    Image2DMS,
    Image2DMSArray,

    ENUM_CLASS_RANGE(Image1D,Image2DMSArray)
};

constexpr const char *ShaderImageTypeName[]=
{
    "imageError",

    "image1D",
    "image2D",
    "image3D",

    "imageCube",
    "image2DRect",

    "image1DArray",
    "image2DArray",

    "imageCubeArray",

    "imageBuffer",

    "image2DMS",
    "image2DMSArray"
};

inline const char *GetShaderImageTypeName(const ShaderImageType it)
{
    RANGE_CHECK_RETURN_NULLPTR(it);

    return ShaderImageTypeName[(int)it];
}

inline const ShaderImageType ParseShaderImageType(const char *name,int name_len=0)
{
    int result=::hgl::find_str_in_array<char>(int(ShaderImageType::RANGE_SIZE),(const char **)ShaderImageTypeName,name,name_len);

    if(result<(int)ShaderImageType::BEGIN_RANGE||result>(int)ShaderImageType::END_RANGE)
        return ShaderImageType::Error;

    return (ShaderImageType)result;
}
}

// Backward compatibility aliases for hgl::graph
namespace hgl::graph
{
    using hgl::shader_schema::ShaderImageType;
    using hgl::shader_schema::GetShaderImageTypeName;
    using hgl::shader_schema::GetShaderImageTypeByString;
}//namespace hgl::graph
