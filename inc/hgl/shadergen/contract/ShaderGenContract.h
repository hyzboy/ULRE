#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace hgl::graph::mtl::contract
{
    constexpr uint32_t kShaderGenContractVersion = 1;

    enum class ResourceClass : uint8_t
    {
        Unknown = 0,
        UniformBuffer,
        StorageBuffer,
        SampledImage,
        Sampler,
        CombinedImageSampler,
        InputAttachment,
    };

    enum class ShaderStageMask : uint32_t
    {
        None     = 0,
        Vertex   = 1u << 0,
        Geometry = 1u << 1,
        Fragment = 1u << 2,
        Compute  = 1u << 3,
    };

    enum class PlatformTier : uint8_t
    {
        Unknown = 0,
        Desktop,
        Mobile,
    };

    enum class QualityLevel : uint8_t
    {
        Low = 0,
        Medium,
        High,
        Ultra,
    };

    struct ResourceRequirement
    {
        std::string name;
        ResourceClass resource_class = ResourceClass::Unknown;
        bool required = true;
    };

    struct VertexInputRequirement
    {
        std::string semantic;
        uint32_t location = 0;
        std::string type_name;
        uint32_t input_rate = 0;
    };

    struct MaterialCreateConfigLite
    {
        uint32_t primitive_type = 0;
        uint32_t shader_stage_flags = 0;
        bool enable_lighting = false;
    };

    struct ShaderPermutationKeyLite
    {
        uint32_t key0 = 0;
        uint32_t key1 = 0;
        uint32_t key2 = 0;
        uint32_t key3 = 0;
    };

    struct PipelineModeLite
    {
        uint32_t render_path = 0;
        uint32_t output_mode = 0;
    };

    struct StageSpvBlob
    {
        uint32_t stage_mask = uint32_t(ShaderStageMask::None);
        std::vector<uint32_t> words;
    };

    struct DescriptorBindingDesc
    {
        uint32_t set = 0;
        uint32_t binding = 0;
        ResourceClass resource_class = ResourceClass::Unknown;
        uint32_t stage_mask = uint32_t(ShaderStageMask::None);
        std::string name;
        std::string type_name;
    };

    struct ShaderResourceLayout
    {
        std::vector<DescriptorBindingDesc> bindings;
    };

    struct VertexAttributeDesc
    {
        uint32_t location = 0;
        std::string semantic;
        std::string type_name;
        uint32_t input_rate = 0;
    };

    struct VertexInputLayout
    {
        std::vector<VertexAttributeDesc> attributes;
    };

    struct BufferFieldDesc
    {
        std::string name;
        std::string type_name;
        uint32_t offset = 0;
        uint32_t size = 0;
    };

    struct BufferStructDesc
    {
        std::string struct_name;
        ResourceClass resource_class = ResourceClass::Unknown;
        uint32_t binding = 0;
        uint32_t set = 0;
        uint32_t byte_size = 0;
        std::vector<BufferFieldDesc> fields;
    };

    struct ShaderDiagnostics
    {
        bool has_error = false;
        std::vector<std::string> errors;
        std::vector<std::string> warnings;
    };

    struct ShaderCacheKey
    {
        uint64_t hi = 0;
        uint64_t lo = 0;
    };

    struct ShaderGenRequest
    {
        uint32_t contract_version = kShaderGenContractVersion;

        uint32_t material_id = 0;
        MaterialCreateConfigLite material_cfg;
        ShaderPermutationKeyLite permutation;
        PipelineModeLite pipeline_mode;

        std::vector<ResourceRequirement> required_resources;
        std::vector<VertexInputRequirement> vertex_requirements;

        PlatformTier platform_tier = PlatformTier::Unknown;
        QualityLevel quality_level = QualityLevel::High;

        bool enable_debug_info = false;
        bool enable_fallback = true;
    };

    struct ShaderGenResult
    {
        uint32_t contract_version = kShaderGenContractVersion;

        std::vector<StageSpvBlob> spv_per_stage;
        ShaderResourceLayout layout;
        VertexInputLayout vertex_layout;
        std::vector<BufferStructDesc> buffer_structs;
        ShaderDiagnostics diagnostics;
        ShaderCacheKey cache_key;
    };
}
