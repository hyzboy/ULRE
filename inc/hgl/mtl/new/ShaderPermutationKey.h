#pragma once

#include "SurfaceType.h"
#include <hgl/mtl/SamplerName.h>
#include <string>

namespace hgl::graph
{
    // Permutation key = 16-bit packed + 8-bit per-slot texture array flags.
    //
    // packed [15:12] SurfaceType  (4 bit, max 16)
    // packed [8:7]   ShadowMode   (2 bit: None/PCF/PCSS)
    // packed [6:0]   Reserved
    //
    // texture_array_slot_flags: bit N = SamplerSlot(N) is sampler2DArray
    struct ShaderPermutationKey
    {
        uint16 packed;
        uint8  texture_array_slot_flags;

        ShaderPermutationKey() : packed(0), texture_array_slot_flags(0) {}

        void SetSurfaceType(SurfaceType st) { packed = (packed & 0x0FFF) | (static_cast<uint16>(st) << 12); }
        void SetShadowMode(uint8 sm)        { packed = (packed & 0xFE7F) | ((sm & 0x3) << 7); }

        void SetSlotArrayMode(mtl::SamplerSlot slot, bool enable)
        {
            const uint8 bit = uint8(1) << uint8(slot);
            if (enable) texture_array_slot_flags |= bit;
            else        texture_array_slot_flags &= uint8(~bit);
        }

        bool GetSlotArrayMode(mtl::SamplerSlot slot) const
        {
            return ((texture_array_slot_flags >> uint8(slot)) & uint8(1)) != 0;
        }

        bool GetTextureArrayMode() const { return texture_array_slot_flags != 0; }

        // Compatibility delegates — keep existing callers compiling.
        void SetTextureArrayMode(bool enable)
        {
            if (enable)
            {
                SetSlotArrayMode(mtl::SamplerSlot::BaseColor, true);
                SetSlotArrayMode(mtl::SamplerSlot::Normal,    true);
                SetSlotArrayMode(mtl::SamplerSlot::Roughness, true);
            }
            else
                texture_array_slot_flags = 0;
        }
        void SetBaseTextureArrayMode(bool enable)      { SetSlotArrayMode(mtl::SamplerSlot::BaseColor, enable); }
        void SetNormalTextureArrayMode(bool enable)    { SetSlotArrayMode(mtl::SamplerSlot::Normal,    enable); }
        void SetRoughnessTextureArrayMode(bool enable) { SetSlotArrayMode(mtl::SamplerSlot::Roughness, enable); }

        bool GetBaseTextureArrayMode() const      { return GetSlotArrayMode(mtl::SamplerSlot::BaseColor); }
        bool GetNormalTextureArrayMode() const    { return GetSlotArrayMode(mtl::SamplerSlot::Normal); }
        bool GetRoughnessTextureArrayMode() const { return GetSlotArrayMode(mtl::SamplerSlot::Roughness); }

        SurfaceType GetSurfaceType() const { return static_cast<SurfaceType>((packed >> 12) & 0xF); }
        uint8       GetShadowMode()  const { return (packed >> 7) & 0x3; }

        bool operator==(const ShaderPermutationKey& o) const
        {
            return packed == o.packed && texture_array_slot_flags == o.texture_array_slot_flags;
        }
        bool operator<(const ShaderPermutationKey& o) const
        {
            if (packed != o.packed) return packed < o.packed;
            return texture_array_slot_flags < o.texture_array_slot_flags;
        }

        void AppendGLSLDefines(std::string &out) const;
    };
}
