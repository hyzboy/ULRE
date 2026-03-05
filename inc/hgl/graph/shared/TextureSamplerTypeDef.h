#pragma once

#include <hgl/vk/VKNamespace.h>
#include <hgl/type/EnumUtil.h>
#include <hgl/type/StrChar.h>

namespace hgl::graph
{
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
        int result=::hgl::find_str_in_array<char>(int(TextureType::RANGE_SIZE),(const char **)TextureTypeName,name,name_len);

        if(result<(int)TextureType::BEGIN_RANGE||result>(int)TextureType::END_RANGE)
            return TextureType::Error;

        return (TextureType)result;
    }

    enum class SamplerType:uint8
    {
        Error,

        Sampler1D,
        Sampler2D,
        Sampler3D,

        SamplerCube,
        Sampler2DRect,

        Sampler1DArray,
        Sampler2DArray,

        SamplerCubeArray,

        SamplerBuffer,

        Sampler2DMS,
        Sampler2DMSArray,

        Sampler1DShadow,
        Sampler2DShadow,

        SamplerCubeShadow,
        Sampler2DRectShadow,

        Sampler1DArrayShadow,
        Sampler2DArrayShadow,
        SamplerCubeArrayShadow,

        ENUM_CLASS_RANGE(Sampler1D,SamplerCubeArrayShadow)
    };

    constexpr const char *SamplerTypeName[]=
    {
        "samplerError",

        "sampler1D",
        "sampler2D",
        "sampler3D",

        "samplerCube",
        "sampler2DRect",

        "sampler1DArray",
        "sampler2DArray",

        "samplerCubeArray",

        "samplerBuffer",

        "sampler2DMS",
        "sampler2DMSArray",

        "sampler1DShadow",
        "sampler2DShadow",

        "samplerCubeShadow",
        "sampler2DRectShadow",

        "sampler1DArrayShadow",
        "sampler2DArrayShadow",
        "samplerCubeArrayShadow",
    };

    inline const char *GetSamplerTypeName(const SamplerType st)
    {
        RANGE_CHECK_RETURN_NULLPTR(st);

        return SamplerTypeName[static_cast<int>(st)];
    }

    inline const SamplerType ParseSamplerType(const char *name,int name_len=0)
    {
        int result=::hgl::find_str_in_array<char>(int(SamplerType::RANGE_SIZE),(const char **)SamplerTypeName,name,name_len);

        if(result<(int)SamplerType::BEGIN_RANGE||result>(int)SamplerType::END_RANGE)
            return SamplerType::Error;

        return (SamplerType)result;
    }
}
