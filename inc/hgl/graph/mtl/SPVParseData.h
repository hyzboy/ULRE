#pragma once
/**
 * SPVParseData.h — SPIR-V Reflection Data Structures
 *
 * This header is the shared contract between the GLSLCompiler.dll (which fills
 * these structs via SPIRV-Cross) and the ULRE engine (which consumes them to
 * build VkDescriptorSetLayout, VkVertexInputState, etc.).
 *
 * Rules:
 *  - Pure C-compatible types only (no STL, no Vulkan types).
 *  - All heap memory is owned by the DLL; caller must free via FreeParseSPVData().
 *  - This header is shared between GLSLCompiler and ULRE — keep it in sync.
 *
 * Design document: doc/refactor/ShaderGen_Compiler_Loader_Separation.md
 */

#include <stdint.h>
#include <string.h>  // memset

namespace hgl
{

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

/// Maximum length of a shader resource name (including null terminator).
static constexpr uint32_t SPV_NAME_MAX = 64;

// ---------------------------------------------------------------------------
// Enumerations
// ---------------------------------------------------------------------------

/// GLSL/SPIR-V base scalar type.
enum class SPVBaseType : uint8_t
{
    Bool    = 0,
    Int,        ///< signed integer (any width)
    UInt,       ///< unsigned integer (any width)
    Float,      ///< float or half (treated as float for VkFormat)
    Double,
    Struct,
    Image,
    Sampler,
    MAX
};

/// Descriptor resource category (replaces the implicit VkDescriptorType array index).
enum class SPVDescriptorKind : uint8_t
{
    UniformBuffer        = 0,   ///< layout(std140) uniform  → UBO
    StorageBuffer,              ///< layout(std430) buffer   → SSBO
    CombinedImageSampler,       ///< sampler2D / sampler2DArray …
    SampledImage,               ///< texture2D (separable)
    StorageSampler,             ///< sampler  (separable)
    StorageImage,               ///< image2D (read/write)
    InputAttachment,            ///< subpassInput (Mobile Vulkan)
    PushConstant,
    MAX
};

// ---------------------------------------------------------------------------
// UBO / SSBO member reflection
// ---------------------------------------------------------------------------

/// Reflects a single member of a UBO or SSBO struct block.
/// Used to validate that the CPU-side MaterialInstance layout matches the
/// shader-side struct (offset, size, type).
struct SPVMember
{
    char        name[SPV_NAME_MAX];
    uint32_t    offset;       ///< byte offset within the parent struct
    uint32_t    size;         ///< total byte size of this member
    SPVBaseType basetype;
    uint8_t     vec_size;     ///< vector components (1–4)
    uint8_t     col_count;    ///< matrix columns; 0 for non-matrix types
    uint8_t     array_size;   ///< array elements; 0 for non-array members
    uint8_t     _pad;
};

// ---------------------------------------------------------------------------
// Descriptor binding (unified — all types in one flat list)
// ---------------------------------------------------------------------------

/// Reflects one descriptor binding declared in the shader.
/// All descriptor kinds (UBO, SSBO, sampler, etc.) are represented here;
/// the kind is carried explicitly rather than being implied by array index.
struct SPVDescriptorBinding
{
    char                name[SPV_NAME_MAX];
    uint32_t            set;
    uint32_t            binding;
    SPVDescriptorKind   kind;
    uint8_t             _pad[3];
    uint32_t            array_count;    ///< 1 = non-array; >1 for texture arrays
    uint32_t            buffer_size;    ///< declared struct size (bytes); 0 for non-buffer
    uint32_t            member_count;
    SPVMember          *members;        ///< heap-allocated; nullptr when member_count==0
};

// ---------------------------------------------------------------------------
// Stage input / output attributes (vertex inputs, interpolants, FS outputs)
// ---------------------------------------------------------------------------

/// Reflects one stage attribute (vertex input, VS→FS interpolant, or FS output).
struct SPVStageAttribute
{
    char        name[SPV_NAME_MAX];
    uint32_t    location;
    uint32_t    component;  ///< component within the location slot (packed attributes)
    SPVBaseType basetype;
    uint8_t     vec_size;   ///< number of components (1–4)
    uint8_t     _pad[2];
};

// ---------------------------------------------------------------------------
// Push constant range
// ---------------------------------------------------------------------------

struct SPVPushConstantRange
{
    char     name[SPV_NAME_MAX];
    uint32_t offset;
    uint32_t size;
};

// ---------------------------------------------------------------------------
// Subpass input (Mobile Vulkan — subpassLoad)
// ---------------------------------------------------------------------------

struct SPVSubpassInput
{
    char     name[SPV_NAME_MAX];
    uint32_t attachment_index;
    uint32_t binding;
};

// ---------------------------------------------------------------------------
// Generic flat array (C ABI safe)
// ---------------------------------------------------------------------------

template<typename T>
struct SPVArray
{
    uint32_t count;
    T       *items;

    SPVArray() { count = 0; items = nullptr; }

    const T *begin() const { return items; }
    const T *end()   const { return items + count; }
    bool     empty() const { return count == 0; }
};

// ---------------------------------------------------------------------------
// Top-level parse result for one shader stage
// ---------------------------------------------------------------------------

struct SPVParseData
{
    /// Vertex inputs (VS) or mesh-shader outputs fed to FS.
    SPVArray<SPVStageAttribute>    stage_inputs;

    /// Stage outputs (VS → FS interpolants, or FS color outputs).
    SPVArray<SPVStageAttribute>    stage_outputs;

    /// All descriptor bindings declared in this stage (flat, any kind).
    SPVArray<SPVDescriptorBinding> descriptors;

    SPVArray<SPVPushConstantRange> push_constants;
    SPVArray<SPVSubpassInput>      subpass_inputs;

    SPVParseData()
    {
        memset(this, 0, sizeof(*this));
    }

    // Destructor is intentionally not defined here — the DLL owns the heap
    // allocations.  Always free via GLSLCompilerInterface::FreeParseSPVData().

    // ---------------------------------------------------------------------------
    // Convenience accessors
    // ---------------------------------------------------------------------------

    /// Find a descriptor binding by name (linear search — build-time only).
    const SPVDescriptorBinding *FindDescriptor(const char *n) const
    {
        for (uint32_t i = 0; i < descriptors.count; ++i)
            if (strncmp(descriptors.items[i].name, n, SPV_NAME_MAX) == 0)
                return &descriptors.items[i];
        return nullptr;
    }

    /// Find a vertex input attribute by name.
    const SPVStageAttribute *FindInput(const char *n) const
    {
        for (uint32_t i = 0; i < stage_inputs.count; ++i)
            if (strncmp(stage_inputs.items[i].name, n, SPV_NAME_MAX) == 0)
                return &stage_inputs.items[i];
        return nullptr;
    }
};

} // namespace hgl
