#pragma once

#include <cstdint>

namespace hgl::graph::mtl {

/// Per-instance shader data schema selector.
///
/// Each enumerator identifies a fixed GLSL struct layout that the shader reads
/// as the "MaterialInstance" block via the MaterialInstanceData SSBO.
///
/// Lifecycle: Replace the old (mi_glsl_codes, mi_struct_bytes) pair in
/// StaticMaterialDef with a single ShaderDataSchema value.  The registry
/// GetShaderDataSchemaInfo() provides the corresponding byte size needed
/// by the runtime allocator, and the GLSL file path for future injection.
enum class ShaderDataSchema : uint32_t
{
    None = 0,           ///< No per-instance data.

    Color4f,            ///< struct MaterialInstance { vec4 Color; }          — 16 bytes
    TextColor,          ///< struct MaterialInstance { uint TextColor; }       —  4 bytes
    BillboardSizeUVec2, ///< struct MaterialInstance { uvec2 BillboardSize; }  —  8 bytes
    PBRColorParams,     ///< struct MaterialInstance { uint base_color; float metallic; float roughness; }   — 12 bytes
    StandardParams,     ///< struct MaterialInstance { uint base_color; float metallic; float roughness; float normal_scale; } — 16 bytes
    TextureArrayID,     ///< struct MaterialInstance { uvec4 id; }             — 16 bytes

    COUNT
};

/// Metadata associated with a ShaderDataSchema entry.
struct ShaderDataSchemaInfo
{
    /// Path of the GLSL file that defines the MaterialInstance struct,
    /// relative to ShaderLibrary/common/schema/.
    /// Null for ShaderDataSchema::None.
    const char *  glsl_schema_file;

    /// GLSL struct name — always "MaterialInstance" for all current schemas.
    const char *  struct_name;

    /// Size of the struct in bytes as seen by the GPU (std430 layout).
    uint32_t      byte_size;
};

/// Returns the info record for the given schema.
/// For ShaderDataSchema::None, returns a zero-initialised record
/// (nullptr fields, 0 bytes).
const ShaderDataSchemaInfo & GetShaderDataSchemaInfo(ShaderDataSchema schema);

} // namespace hgl::graph::mtl
