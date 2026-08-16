#pragma once

namespace hgl::graph::mtl {}

#include <hgl/CoreType.h>
#include <hgl/common/VertexAttribDef.h>

namespace hgl::graph
{
    class GeometryVertexFormat;
}

namespace hgl::graph::shadergen
{
    using namespace hgl::graph::mtl;
    constexpr uint32 InvalidShaderSemanticLocation = uint32(-1);

    enum class ShaderSemanticScalarType : uint8
    {
        Unknown = 0,
        Float,
        SignedInteger,
        UnsignedInteger,
        Boolean
    };

    struct ShaderSemanticValueShape
    {
        ShaderSemanticScalarType scalar_type = ShaderSemanticScalarType::Unknown;
        uint8 component_count = 0;
    };

    enum class GeometrySemanticLocationPolicy : uint8
    {
        GeometryAttributeOrder = 0
    };

    struct GeometrySemanticInfo
    {
        VertexSemantic semantic = VertexSemantic::Unknown;
        const char *shader_symbol = nullptr;
        ShaderSemanticValueShape default_shape{};
        uint8 location_width = 0;
        GeometrySemanticLocationPolicy location_policy =
            GeometrySemanticLocationPolicy::GeometryAttributeOrder;
    };

    enum class InterStageSemantic : uint8
    {
        Unknown = 0,
        DataIndexID,
        WorldPosition,
        WorldNormal,
        UV0,
        UV1,
        Color,
        FragDirection,
        Luminance,
        WorldTangent,
        WorldBinormal,

        ENUM_CLASS_RANGE(Unknown,WorldBinormal)
    };

    enum class InterStageInterpolation : uint8
    {
        Smooth = 0,
        Flat,
        NoPerspective
    };

    struct InterStageSemanticInfo
    {
        InterStageSemantic semantic = InterStageSemantic::Unknown;
        const char *shader_symbol = nullptr;
        ShaderSemanticValueShape value_shape{};
        InterStageInterpolation interpolation = InterStageInterpolation::Smooth;
        uint8 location_width = 0;
        uint32 stable_location = InvalidShaderSemanticLocation;
    };

    using InterStageSemanticMask = uint32;

    constexpr InterStageSemanticMask GetInterStageSemanticMask(
        const InterStageSemantic semantic) noexcept
    {
        const uint32 value = static_cast<uint32>(semantic);
        return value > 0 && value < 32 ? InterStageSemanticMask(1u << value) : 0;
    }

    enum class ShaderSemanticRegistryValidationError : uint8
    {
        None = 0,
        GeometryEntryOrder,
        GeometryNameMissing,
        GeometryTypeInvalid,
        GeometryLocationPolicyInvalid,
        InterStageEntryOrder,
        InterStageNameMissing,
        InterStageNameConflict,
        InterStageTypeInvalid,
        InterStageInterpolationInvalid,
        InterStageLocationInvalid,
        InterStageLocationConflict
    };

    struct ShaderSemanticRegistryValidationResult
    {
        ShaderSemanticRegistryValidationError error =
            ShaderSemanticRegistryValidationError::None;
        uint32 first_index = 0;
        uint32 second_index = 0;
    };

    uint32 GetGeometrySemanticInfoCount() noexcept;
    const GeometrySemanticInfo *GetGeometrySemanticInfo(
        VertexSemantic semantic) noexcept;

    uint32 GetInterStageSemanticInfoCount() noexcept;
    const InterStageSemanticInfo *GetInterStageSemanticInfo(
        InterStageSemantic semantic) noexcept;

    bool ResolveCurrentGeometrySemanticLocation(
        const GeometryVertexFormat &geometry,
        VertexSemantic semantic,
        uint32 &out_location) noexcept;

    bool ValidateGeometrySemanticRegistry(
        ShaderSemanticRegistryValidationResult &out_result) noexcept;
    bool ValidateInterStageSemanticRegistry(
        ShaderSemanticRegistryValidationResult &out_result) noexcept;
    bool ValidateShaderSemanticRegistries(
        ShaderSemanticRegistryValidationResult &out_result) noexcept;
}//namespace hgl::graph::shadergen
