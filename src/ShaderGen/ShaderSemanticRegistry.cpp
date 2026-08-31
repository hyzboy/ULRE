#include <hgl/mtl/ShaderSemanticRegistry.h>

#include <hgl/graph/geo/GeometryVertexFormat.h>
#include <cstring>

namespace hgl::graph::mtl
{
    using namespace hgl::graph::mtl;
    namespace
    {
        constexpr ShaderSemanticValueShape FloatShape(const uint8 component_count)
        {
            return {ShaderSemanticScalarType::Float, component_count};
        }

        constexpr ShaderSemanticValueShape UIntShape(const uint8 component_count)
        {
            return {ShaderSemanticScalarType::UnsignedInteger, component_count};
        }

        constexpr InterStageSemanticInfo InterStageSemanticRegistry[] =
        {
            {InterStageSemantic::Unknown,        "Unknown",            {},           InterStageInterpolation::Smooth, 0, InvalidShaderSemanticLocation},
            {InterStageSemantic::DataIndexID,    "fragDataIndexID",    UIntShape(1),  InterStageInterpolation::Flat,   1, 0},
            {InterStageSemantic::WorldPosition,  "fragWorldPos",       FloatShape(3), InterStageInterpolation::Smooth, 1, 1},
            {InterStageSemantic::WorldNormal,    "fragWorldNormal",    FloatShape(3), InterStageInterpolation::Smooth, 1, 2},
            {InterStageSemantic::UV0,            "fragUV0",            FloatShape(2), InterStageInterpolation::Smooth, 1, 3},
            {InterStageSemantic::Color,          "fragVertexColor",    FloatShape(4), InterStageInterpolation::Smooth, 1, 5},
            {InterStageSemantic::FragDirection,  "fragDirection",      FloatShape(3), InterStageInterpolation::Smooth, 1, 6},
            {InterStageSemantic::Luminance,      "fragLuminance",      FloatShape(1), InterStageInterpolation::Smooth, 1, 7},
            {InterStageSemantic::WorldTangent,   "fragWorldTangent",   FloatShape(3), InterStageInterpolation::Smooth, 1, 8},
            {InterStageSemantic::WorldBinormal,  "fragWorldBinormal",  FloatShape(3), InterStageInterpolation::Smooth, 1, 9},
            {InterStageSemantic::StyleID,        "fragStyleID",        UIntShape(1),  InterStageInterpolation::Flat,   1, 4}
        };

        constexpr uint32 InterStageSemanticRegistryCount =
            static_cast<uint32>(sizeof(InterStageSemanticRegistry)
                / sizeof(InterStageSemanticRegistry[0]));

        static_assert(
            InterStageSemanticRegistryCount
                == static_cast<uint32>(InterStageSemantic::RANGE_SIZE),
            "Inter-stage semantic registry must cover InterStageSemantic");

        bool HasValidShape(const ShaderSemanticValueShape &shape) noexcept
        {
            return shape.scalar_type != ShaderSemanticScalarType::Unknown
                && shape.component_count > 0
                && shape.component_count <= 4;
        }

        bool SetValidationFailure(
            ShaderSemanticRegistryValidationResult &out_result,
            const ShaderSemanticRegistryValidationError error,
            const uint32 first_index,
            const uint32 second_index = 0) noexcept
        {
            out_result.error = error;
            out_result.first_index = first_index;
            out_result.second_index = second_index;
            return false;
        }
    }

    uint32 GetInterStageSemanticInfoCount() noexcept
    {
        return InterStageSemanticRegistryCount;
    }

    const InterStageSemanticInfo *GetInterStageSemanticInfo(
        const InterStageSemantic semantic) noexcept
    {
        const uint32 index = static_cast<uint32>(semantic);
        if (index >= InterStageSemanticRegistryCount)
            return nullptr;

        return InterStageSemanticRegistry + index;
    }

    bool ValidateInterStageSemanticRegistry(
        ShaderSemanticRegistryValidationResult &out_result) noexcept
    {
        out_result = {};
        for (uint32 index = 0; index < InterStageSemanticRegistryCount; ++index)
        {
            const InterStageSemanticInfo &info = InterStageSemanticRegistry[index];
            if (static_cast<uint32>(info.semantic) != index)
                return SetValidationFailure(
                    out_result,
                    ShaderSemanticRegistryValidationError::InterStageEntryOrder,
                    index);

            if (index == 0)
                continue;

            if (!info.shader_symbol || !info.shader_symbol[0])
                return SetValidationFailure(
                    out_result,
                    ShaderSemanticRegistryValidationError::InterStageNameMissing,
                    index);

            if (!HasValidShape(info.value_shape) || info.location_width == 0)
                return SetValidationFailure(
                    out_result,
                    ShaderSemanticRegistryValidationError::InterStageTypeInvalid,
                    index);

            if (info.interpolation < InterStageInterpolation::Smooth
             || info.interpolation > InterStageInterpolation::NoPerspective)
            {
                return SetValidationFailure(
                    out_result,
                    ShaderSemanticRegistryValidationError::
                        InterStageInterpolationInvalid,
                    index);
            }

            if (info.stable_location == InvalidShaderSemanticLocation)
                return SetValidationFailure(
                    out_result,
                    ShaderSemanticRegistryValidationError::
                        InterStageLocationInvalid,
                    index);

            for (uint32 other_index = 1;
                 other_index < index;
                 ++other_index)
            {
                const InterStageSemanticInfo &other =
                    InterStageSemanticRegistry[other_index];
                const uint32 info_end =
                    info.stable_location + info.location_width;
                const uint32 other_end =
                    other.stable_location + other.location_width;
                if (info.stable_location < other_end
                 && other.stable_location < info_end)
                {
                    return SetValidationFailure(
                        out_result,
                        ShaderSemanticRegistryValidationError::
                            InterStageLocationConflict,
                        other_index,
                        index);
                }

                if (std::strcmp(info.shader_symbol, other.shader_symbol) == 0)
                    return SetValidationFailure(
                        out_result,
                        ShaderSemanticRegistryValidationError::
                            InterStageNameConflict,
                        other_index,
                        index);

            }
        }

        return true;
    }

    bool ValidateShaderSemanticRegistries(
        ShaderSemanticRegistryValidationResult &out_result) noexcept
    {
        // GeometrySemantic 半边（VBO 顶点属性语义）已删除——只剩 InterStage 校验
        return ValidateInterStageSemanticRegistry(out_result);
    }
}
