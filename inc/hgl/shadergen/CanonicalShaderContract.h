#pragma once

namespace hgl::graph::mtl {}

#include <hgl/CoreType.h>
#include <hgl/common/DescriptorSetTypeDef.h>
#include <hgl/common/ShaderStageDef.h>
#include <hgl/common/VertexAttribDef.h>
#include <hgl/graph/glsl/GLSLCodeModule.h>
#include <hgl/graph/ssbo/SSBOTypes.h>
#include <hgl/graph/ssbo/TextureSlot.h>
#include <hgl/mtl/DescriptorKind.h>
#include <hgl/mtl/DescriptorSemantic.h>
#include <hgl/shadergen/ShaderSemanticRegistry.h>
#include <hgl/shadergen/ShaderStageBuildContext.h>
#include <hgl/type/ValueArray.h>

namespace hgl::graph::shadergen
{
    using namespace hgl::graph::mtl;
    using ShaderContractStableID = uint64;

    enum class ShaderProgramPurpose : uint8
    {
        ForwardColor = 0,
        DepthOnly,
        ShadowDepth
    };

    struct ResolvedModuleContractEntry
    {
        ShaderContractStableID module_id = 0;
        uint64 module_content_hash = 0;
        uint32 topological_order = 0;
        uint32 flags = 0;
    };

    inline bool operator==(
        const ResolvedModuleContractEntry &lhs,
        const ResolvedModuleContractEntry &rhs) noexcept
    {
        return lhs.module_id == rhs.module_id
            && lhs.module_content_hash == rhs.module_content_hash
            && lhs.topological_order == rhs.topological_order
            && lhs.flags == rhs.flags;
    }

    struct ResolvedModuleDependencyContract
    {
        ShaderContractStableID source_module_id = 0;
        ShaderContractStableID target_module_id = 0;
    };

    inline bool operator==(
        const ResolvedModuleDependencyContract &lhs,
        const ResolvedModuleDependencyContract &rhs) noexcept
    {
        return lhs.source_module_id == rhs.source_module_id
            && lhs.target_module_id == rhs.target_module_id;
    }

    struct ResolvedProviderSelectionContract
    {
        GLSLCodeModuleSemantic semantic = GLSLCodeModuleSemantic::Unknown;
        ShaderContractStableID provider_module_id = 0;
        int32 priority = 0;
        uint32 flags = 0;
    };

    inline bool operator==(
        const ResolvedProviderSelectionContract &lhs,
        const ResolvedProviderSelectionContract &rhs) noexcept
    {
        return lhs.semantic == rhs.semantic
            && lhs.provider_module_id == rhs.provider_module_id
            && lhs.priority == rhs.priority
            && lhs.flags == rhs.flags;
    }

    struct ResolvedModuleGraph
    {
        ValueArray<ResolvedModuleContractEntry> modules;
        ValueArray<ResolvedModuleDependencyContract> dependencies;
        ValueArray<ResolvedProviderSelectionContract> provider_selections;
        ValueArray<GLSLCodeModuleSemanticRequirement>
            aggregated_semantic_requirements;
    };

    struct GeometrySemanticContractEntry
    {
        VertexSemantic semantic = VertexSemantic::Unknown;
        ShaderSemanticScalarType scalar_type =
            ShaderSemanticScalarType::Unknown;
        uint8 component_count = 0;
        uint8 location_width = 0;
        uint32 physical_location = InvalidShaderSemanticLocation;
        uint32 physical_format = 0;
    };

    inline bool operator==(
        const GeometrySemanticContractEntry &lhs,
        const GeometrySemanticContractEntry &rhs) noexcept
    {
        return lhs.semantic == rhs.semantic
            && lhs.scalar_type == rhs.scalar_type
            && lhs.component_count == rhs.component_count
            && lhs.location_width == rhs.location_width
            && lhs.physical_location == rhs.physical_location
            && lhs.physical_format == rhs.physical_format;
    }

    struct InterStageSemanticContractEntry
    {
        InterStageSemantic semantic = InterStageSemantic::Unknown;
        ShaderSemanticScalarType scalar_type =
            ShaderSemanticScalarType::Unknown;
        InterStageInterpolation interpolation =
            InterStageInterpolation::Smooth;
        uint8 component_count = 0;
        uint8 location_width = 0;
        uint32 location = InvalidShaderSemanticLocation;
    };

    inline bool operator==(
        const InterStageSemanticContractEntry &lhs,
        const InterStageSemanticContractEntry &rhs) noexcept
    {
        return lhs.semantic == rhs.semantic
            && lhs.scalar_type == rhs.scalar_type
            && lhs.interpolation == rhs.interpolation
            && lhs.component_count == rhs.component_count
            && lhs.location_width == rhs.location_width
            && lhs.location == rhs.location;
    }

    struct ShaderDescriptorContractEntry
    {
        ShaderContractStableID logical_resource_id = 0;
        uint64 resource_schema_id = 0;
        DescriptorSemantic semantic = DescriptorSemantic::Unknown;
        DescriptorSemanticLayer semantic_layer =
            DescriptorSemanticLayer::Unknown;
        DescriptorSetType set_type = DescriptorSetType::Unknow;
        DescriptorKind kind = DescriptorKind::UBO;
        TextureSlot texture_slot = TextureSlot::BaseColor;
        SSBOType ssbo_type = SSBOType::UserDefined;
        uint32 data_slot = 0;
        uint32 stage_flags = 0;
        uint32 array_count = 1;
        bool required = true;
        bool allow_fallback = false;
    };

    inline bool operator==(
        const ShaderDescriptorContractEntry &lhs,
        const ShaderDescriptorContractEntry &rhs) noexcept
    {
        return lhs.logical_resource_id == rhs.logical_resource_id
            && lhs.resource_schema_id == rhs.resource_schema_id
            && lhs.semantic == rhs.semantic
            && lhs.semantic_layer == rhs.semantic_layer
            && lhs.set_type == rhs.set_type
            && lhs.kind == rhs.kind
            && lhs.texture_slot == rhs.texture_slot
            && lhs.ssbo_type == rhs.ssbo_type
            && lhs.data_slot == rhs.data_slot
            && lhs.stage_flags == rhs.stage_flags
            && lhs.array_count == rhs.array_count
            && lhs.required == rhs.required
            && lhs.allow_fallback == rhs.allow_fallback;
    }

    struct ShaderEntryPointContract
    {
        ShaderStage stage = ShaderStage::Vertex;
        ShaderContractStableID entry_point_id = 0;
    };

    inline bool operator==(
        const ShaderEntryPointContract &lhs,
        const ShaderEntryPointContract &rhs) noexcept
    {
        return lhs.stage == rhs.stage
            && lhs.entry_point_id == rhs.entry_point_id;
    }

    struct ShaderInterfaceContract
    {
        ValueArray<GeometrySemanticContractEntry> geometry_semantics;
        ValueArray<InterStageSemanticContractEntry> inter_stage_semantics;
        ValueArray<ShaderDescriptorContractEntry> descriptor_requirements;
        ValueArray<ShaderEntryPointContract> entry_points;
    };

    struct ShaderOutputAttachmentContract
    {
        ShaderContractStableID write_semantic_id = 0;
        ShaderStageValueType value_type = ShaderStageValueType::Unknown;
        uint32 location = 0;
        uint32 location_width = 1;
        uint32 flags = 0;
    };

    inline bool operator==(
        const ShaderOutputAttachmentContract &lhs,
        const ShaderOutputAttachmentContract &rhs) noexcept
    {
        return lhs.write_semantic_id == rhs.write_semantic_id
            && lhs.value_type == rhs.value_type
            && lhs.location == rhs.location
            && lhs.location_width == rhs.location_width
            && lhs.flags == rhs.flags;
    }

    struct OutputContract
    {
        ShaderProgramPurpose purpose = ShaderProgramPurpose::ForwardColor;
        bool depth_only = false;
        ValueArray<ShaderOutputAttachmentContract> attachments;
    };

    bool ValidateResolvedModuleGraph(
        const ResolvedModuleGraph &graph) noexcept;
    bool ValidateShaderInterfaceContract(
        const ShaderInterfaceContract &contract) noexcept;
    bool ValidateOutputContract(const OutputContract &contract) noexcept;

    bool SerializeResolvedModuleGraph(
        const ResolvedModuleGraph &graph,
        ValueArray<uint8> &out_bytes);
    bool SerializeShaderInterfaceContract(
        const ShaderInterfaceContract &contract,
        ValueArray<uint8> &out_bytes);
    bool SerializeOutputContract(
        const OutputContract &contract,
        ValueArray<uint8> &out_bytes);
    uint64 GetResolvedModuleGraphHash(
        const ResolvedModuleGraph &graph) noexcept;
    uint64 GetShaderInterfaceContractHash(
        const ShaderInterfaceContract &contract) noexcept;
    uint64 GetOutputContractHash(
        const OutputContract &contract) noexcept;
}
