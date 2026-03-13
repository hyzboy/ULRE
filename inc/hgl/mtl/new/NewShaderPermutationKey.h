#pragma once

#include "SurfaceType.h"
#include "QualityTier.h"
#include "PlatformBackend.h"

namespace hgl::graph
{
    // 16-bit packed key:
    //   [15:12] SurfaceType  (4 bit, max 16)
    //   [11:9]  QualityTier  (3 bit, max 8)
    //   [8:7]   ShadowMode   (2 bit: None/PCF/PCSS)
    //   [6:4]   Flags        (3 bit: fog/skinning/wind etc.)
    //   [3:2]   Platform     (2 bit: PC/Apple/Android)
    //   [1:0]   Reserved     (2 bit)
    struct NewShaderPermutationKey
    {
        uint16 packed;

        NewShaderPermutationKey() : packed(0) {}

        void SetSurfaceType(SurfaceType st)     { packed = (packed & 0x0FFF) | (static_cast<uint16>(st) << 12); }
        void SetQualityTier(QualityTier qt)      { packed = (packed & 0xF1FF) | (static_cast<uint16>(qt) << 9); }
        void SetShadowMode(uint8 sm)             { packed = (packed & 0xFE7F) | ((sm & 0x3) << 7); }
        void SetFlags(uint8 flags)               { packed = (packed & 0xFF8F) | ((flags & 0x7) << 4); }
        void SetPlatform(PlatformBackend pb)     { packed = (packed & 0xFFF3) | (static_cast<uint16>(pb) << 2); }

        SurfaceType     GetSurfaceType() const   { return static_cast<SurfaceType>((packed >> 12) & 0xF); }
        QualityTier     GetQualityTier() const   { return static_cast<QualityTier>((packed >> 9) & 0x7); }
        uint8           GetShadowMode()  const   { return (packed >> 7) & 0x3; }
        uint8           GetFlags()       const   { return (packed >> 4) & 0x7; }
        PlatformBackend GetPlatform()    const   { return static_cast<PlatformBackend>((packed >> 2) & 0x3); }

        bool operator==(const NewShaderPermutationKey& o) const { return packed == o.packed; }
        bool operator<(const NewShaderPermutationKey& o) const { return packed < o.packed; }
    };
}
