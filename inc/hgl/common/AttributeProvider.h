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
        SSBO_Vec3               = 2,    ///< storage buffer of vec3 (std430: vec4 stride)
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
        AttributeSemantic   semantic_hint;  ///< which semantic this provider most naturally serves (hint only)

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

}//namespace hgl::graph
