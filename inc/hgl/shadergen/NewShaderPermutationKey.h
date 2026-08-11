#pragma once

#include <hgl/mtl/SurfaceType.h>
#include <string>

namespace hgl::graph::shadergen
{
    // 16-bit packed key:
    //   [15:12] SurfaceType  (4 bit, max 16)

    //   [8:7]   ShadowMode   (2 bit: None/PCF/PCSS)
    //   [6:4]   Flags        (3 bit: fog/skinning/wind etc.)

    //   [1:0]   Reserved     (2 bit)
    struct NewShaderPermutationKey
    {
        uint16 packed;

        NewShaderPermutationKey() : packed(0) {}

        void SetSurfaceType(SurfaceType st)     { packed = (packed & 0x0FFF) | (static_cast<uint16>(st) << 12); }
        void SetShadowMode(uint8 sm)             { packed = (packed & 0xFE7F) | ((sm & 0x3) << 7); }
        void SetFlags(uint8 flags)               { packed = (packed & 0xFF8F) | ((flags & 0x7) << 4); }

        SurfaceType     GetSurfaceType() const   { return static_cast<SurfaceType>((packed >> 12) & 0xF); }
        uint8           GetShadowMode()  const   { return (packed >> 7) & 0x3; }
        uint8           GetFlags()       const   { return (packed >> 4) & 0x7; }

        bool operator==(const NewShaderPermutationKey& o) const { return packed == o.packed; }
        bool operator<(const NewShaderPermutationKey& o) const { return packed < o.packed; }

        void AppendGLSLDefines(std::string &out) const;
    };
}
