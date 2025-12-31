#pragma once

#include<hgl/graph/VKNamespace.h>
#include<hgl/type/EnumUtil.h>
#include<hgl/type/StrChar.h>

VK_NAMESPACE_BEGIN
enum class TextureType:uint8
{
    Error,

    Texture1D,
    Texture2D,
    Texture3D,

    TextureCube,
    Texture2DRect,

    Texture1DArray,
    Texture2DArray,

    TextureCubeArray,

    TextureBuffer,

    Texture2DMS,
    Texture2DMSArray,

    ENUM_CLASS_RANGE(Texture1D,Texture2DMSArray)
};

constexpr const char *TextureTypeName[]=
{
    "textureError",

    "texture1D",
    "texture2D",
    "texture3D",

    "textureCube",
    "texture2DRect"

    "texture1DArray",
    "texture2DArray",

    "textureCubeArray",

    "textureBuffer",

    "texture2DMS",
    "texture2DMSArray"
};

inline const char *GetTextureTypeName(const TextureType st)
{
    RANGE_CHECK_RETURN_NULLPTR(st);

    return TextureTypeName[static_cast<int>(st)];
}

inline const TextureType ParseTextureType(const char *name,int name_len=0)
{
    int result=hgl::find_str_in_array<char>(int(TextureType::RANGE_SIZE),(const char **)TextureTypeName,name,name_len);

    if(result<(int)TextureType::BEGIN_RANGE||result>(int)TextureType::END_RANGE)
        return TextureType::Error;

    return (TextureType)result;
}

constexpr const VkImageViewType TextureImageViewType[]=
{
    VK_IMAGE_VIEW_TYPE_1D,
    VK_IMAGE_VIEW_TYPE_2D,
    VK_IMAGE_VIEW_TYPE_3D,
    VK_IMAGE_VIEW_TYPE_CUBE,

    VK_IMAGE_VIEW_TYPE_1D_ARRAY,
    VK_IMAGE_VIEW_TYPE_2D_ARRAY,
    VK_IMAGE_VIEW_TYPE_CUBE_ARRAY,

    VK_IMAGE_VIEW_TYPE_2D,
    VK_IMAGE_VIEW_TYPE_2D_ARRAY,

    VK_IMAGE_VIEW_TYPE_1D,
    VK_IMAGE_VIEW_TYPE_2D,
    VK_IMAGE_VIEW_TYPE_CUBE,

    VK_IMAGE_VIEW_TYPE_1D_ARRAY,
    VK_IMAGE_VIEW_TYPE_2D_ARRAY,
    VK_IMAGE_VIEW_TYPE_CUBE_ARRAY,
};

inline const VkImageViewType GetTextureImageViewType(const TextureType st)
{
    if(!RangeCheck(st))
        return(VK_IMAGE_VIEW_TYPE_MAX_ENUM);

    return TextureImageViewType[static_cast<int>(st)];
}

VK_NAMESPACE_END
