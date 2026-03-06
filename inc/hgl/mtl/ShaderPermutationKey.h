#pragma once

#include<hgl/mtl/SkyLight.h>
#include <string>

namespace hgl::graph::mtl{

enum class LightModel : uint8
{
    Unlit       = 0,
    Lambert,
    BlinnPhong,
    PBR_Lite,
    PBR_Full,
    CelShading,

    ENUM_CLASS_RANGE(Unlit, CelShading)
};

enum class SpecularChannel : uint8
{
    Combined    = 0,
    Separated,

    ENUM_CLASS_RANGE(Combined, Separated)
};

enum class ShadowReceive : uint8
{
    None        = 0,
    PCF,
    PCSS,

    ENUM_CLASS_RANGE(None, PCSS)
};

struct ShaderPermutationKey
{
    SkyLightAmbientModel ambient = SkyLightAmbientModel::Simple;
    LightModel      light       = LightModel::BlinnPhong;
    SpecularChannel specular    = SpecularChannel::Combined;
    ShadowReceive   shadow      = ShadowReceive::None;

    uint32_t ToU32() const
    {
        return  (uint32_t(ambient)  <<  0) |
                (uint32_t(light)    <<  8) |
                (uint32_t(specular) << 16) |
                (uint32_t(shadow)   << 24);
    }

    bool operator==(const ShaderPermutationKey &o) const { return ToU32() == o.ToU32(); }
    bool operator!=(const ShaderPermutationKey &o) const { return !(*this == o); }
    bool operator< (const ShaderPermutationKey &o) const { return ToU32() <  o.ToU32(); }

    void AppendGLSLDefines(std::string &defines_out) const;
};

}//namespace hgl::graph::mtl
