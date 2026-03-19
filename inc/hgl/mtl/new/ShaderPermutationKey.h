#pragma once

#include "SurfaceType.h"
#include <string>

namespace hgl::graph
{
    // 16-bit packed key:
    //   [15:12] SurfaceType  (4 bit, max 16)
    //   [8:7]   ShadowMode   (2 bit: None/PCF/PCSS)
    //   [6:4]   Flags        (3 bit: texture array mode per slot for Standard)
    //             bit0: BaseColor uses sampler2DArray
    //             bit1: Normal uses sampler2DArray
    //             bit2: Roughness/MR uses sampler2DArray
    //   [1:0]   Reserved     (2 bit)
    struct ShaderPermutationKey
    {
        uint16 packed;

        ShaderPermutationKey() : packed(0) {}

        void SetSurfaceType(SurfaceType st)     { packed = (packed & 0x0FFF) | (static_cast<uint16>(st) << 12); }
        void SetShadowMode(uint8 sm)             { packed = (packed & 0xFE7F) | ((sm & 0x3) << 7); }
        void SetFlags(uint8 flags)               { packed = (packed & 0xFF8F) | ((flags & 0x7) << 4); }

        // Compatibility: set all Standard texture slots to same array mode.
        void SetTextureArrayMode(bool enable)
        {
            uint8 f = GetFlags();
            if (enable) f |= 0x7; else f &= ~0x7;
            SetFlags(f);
        }
        bool GetTextureArrayMode() const { return (GetFlags() & 0x7) != 0; }

        void SetBaseTextureArrayMode(bool enable)
        {
            uint8 f = GetFlags();
            if (enable) f |= 0x1; else f &= ~0x1;
            SetFlags(f);
        }
        void SetNormalTextureArrayMode(bool enable)
        {
            uint8 f = GetFlags();
            if (enable) f |= 0x2; else f &= ~0x2;
            SetFlags(f);
        }
        void SetRoughnessTextureArrayMode(bool enable)
        {
            uint8 f = GetFlags();
            if (enable) f |= 0x4; else f &= ~0x4;
            SetFlags(f);
        }

        bool GetBaseTextureArrayMode() const { return (GetFlags() & 0x1) != 0; }
        bool GetNormalTextureArrayMode() const { return (GetFlags() & 0x2) != 0; }
        bool GetRoughnessTextureArrayMode() const { return (GetFlags() & 0x4) != 0; }

        SurfaceType     GetSurfaceType() const   { return static_cast<SurfaceType>((packed >> 12) & 0xF); }
        
        uint8           GetShadowMode()  const   { return (packed >> 7) & 0x3; }
        uint8           GetFlags()       const   { return (packed >> 4) & 0x7; }

        bool operator==(const ShaderPermutationKey& o) const { return packed == o.packed; }
        bool operator<(const ShaderPermutationKey& o) const { return packed < o.packed; }

        void AppendGLSLDefines(std::string &out) const;
    };
}
