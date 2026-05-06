#pragma once

#include <hgl/type/EnumUtil.h>
#include <string_view>

namespace hgl::graph
{
    /// Identifies which vertex attribute semantic slot a provider services.
    ///
    /// Numeric values are stable and are embedded in the shader cache key.
    /// Built-in IDs 0–7 must never be reordered or repurposed.
    /// UserCustom_Begin (64) and above are reserved for application extension.
    ///
    /// Transition note:
    /// This enum is retained for compatibility while call sites are cleaned up
    /// toward VertexAttrib-centric access paths.
    // REMOVE: AttributeSemantic enum and all references.
    // REMOVE: semantic_hint field in AttributeProvider struct.
    // REMOVE: All compatibility notes referencing AttributeSemantic.
    enum class AttributeSemantic : uint8
    {
        Normal              = 0,
        Tangent             = 1,
        Color               = 2,
        TexCoord0           = 3,
        TexCoord1           = 4,
        Joints              = 5,
        Weights             = 6,
        InstanceTransform   = 7,   ///< per-instance LocalToWorld (replaces instance VBO)
        BuiltinCount        = 8,
        UserCustom_Begin    = 64,
    };

    /// Identifies which attribute provider implementation (GLSL source + upload
    /// format) is used for a given AttributeSemantic.
    ///
    /// Built-in IDs 0–1023 are stable and embedded in the shader cache key.
    /// User-defined IDs start at 0x8000 (lower 15 bits = hash of glsl_path).
    enum class AttributeProviderId : uint16
    {
        None                    = 0,    ///< semantic not active; emitter emits nothing
        SSBO_Vec2               = 1,    ///< storage buffer of tightly-packed vec2
        SSBO_Vec3               = 2,    ///< storage buffer of vec3 (scalar layout: 12 B stride, no padding)
        SSBO_Vec4               = 3,    ///< storage buffer of tightly-packed vec4
        SSBO_PackedRGBA8        = 4,    ///< R8G8B8A8_UNORM packed uint → vec4
        SSBO_PackedNormal_Oct   = 5,    ///< octahedral R16G16_SNORM → unit vec3
        SSBO_PackedUV_2x16      = 6,    ///< R16G16_UNORM packed uint → vec2
        Constant                = 7,    ///< compile-time constant; no buffer binding
        UserCustom_Begin        = 0x8000,
    };

    /// Describes one attribute provider: the ID it implements, the GLSL snippet
    /// that must be included in the vertex shader, and what GPU resources it
    /// requires.  The emitter consults this struct when building the shader
    /// source and the material resource manifest.
    struct AttributeProvider
    {
        AttributeProviderId id;
        // REMOVE: semantic_hint field in AttributeProvider struct.

        /// Path to the GLSL implementation file, relative to the shader library root.
        /// Empty for None and Constant (emitter handles those inline).
        std::string_view    glsl_path;

        bool                needs_ssbo    = true;   ///< requires a storage buffer binding
        bool                needs_uniform = false;  ///< requires a UBO or push-constant field
        bool                needs_sampler = false;  ///< requires a texture sampler binding

        /// Size in bytes of one tightly-packed record on the CPU upload side.
        /// 0 = runtime-determined (e.g. Constant has no buffer).
        uint8               byte_stride   = 0;
    };

    // -----------------------------------------------------------------------
    // Compile-time stride table (scalar / GL_EXT_scalar_block_layout).
    // Returns the per-vertex byte stride for a built-in provider ID.
    // Returns 0 for None, Constant, and UserCustom IDs.
    // -----------------------------------------------------------------------
    constexpr uint32 GetAttribProviderStride(AttributeProviderId id) noexcept
    {
        switch (id)
        {
        case AttributeProviderId::SSBO_Vec2:             return  8;  ///< vec2: 2 × float
        case AttributeProviderId::SSBO_Vec3:             return 12;  ///< vec3: 3 × float, scalar layout – no padding
        case AttributeProviderId::SSBO_Vec4:             return 16;  ///< vec4: 4 × float
        case AttributeProviderId::SSBO_PackedRGBA8:      return  4;  ///< u8vec4
        case AttributeProviderId::SSBO_PackedNormal_Oct: return  4;  ///< u16vec2 octahedral
        case AttributeProviderId::SSBO_PackedUV_2x16:    return  4;  ///< f16vec2
        default:                                         return  0;
        }
    }

    static_assert(GetAttribProviderStride(AttributeProviderId::SSBO_Vec3) == 12);
    static_assert(GetAttribProviderStride(AttributeProviderId::SSBO_Vec2) ==  8);
    static_assert(GetAttribProviderStride(AttributeProviderId::SSBO_Vec4) == 16);
    static_assert(GetAttribProviderStride(AttributeProviderId::SSBO_PackedRGBA8) == 4);
    static_assert(GetAttribProviderStride(AttributeProviderId::None)     ==  0);

}//namespace hgl::graph
