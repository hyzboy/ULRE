#pragma once

#include <cstdint>
#include <string>
#include <hgl/mtl/ShaderDataSchema.h>

namespace hgl::graph::mtl
{
    /// Material instance data block (SSBO descriptor for per-material data).
    /// When stride > 0, MaterialInstance is active and valid.
    struct MaterialInstanceBlock
    {
        uint32_t          stride       = 0;      ///< Bytes per material instance (0 = inactive)
        uint32_t          max_count    = 0;      ///< Maximum instance count (ssbo_range / stride)
        uint32_t          stage_bits   = 0;      ///< Shader stages using this block
        ShaderDataSchema  schema       = ShaderDataSchema::None;
        std::string       schema_file;           ///< GLSL include path for schema

        bool IsActive() const { return stride > 0; }
    };

    /// Local-to-world transform block (SSBO descriptor for transform data).
    /// When enabled = true, LocalToWorld is active and valid.
    struct LocalToWorldBlock
    {
        uint32_t  max_count  = 0;       ///< Maximum transform count (ssbo_range / sizeof(Matrix4f))
        uint32_t  stage_bits = 0;       ///< Shader stages using this block
        bool      enabled    = false;   ///< Is this block active/enabled

        bool IsActive() const { return enabled; }
    };
}
