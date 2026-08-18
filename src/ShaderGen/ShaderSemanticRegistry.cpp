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

        constexpr GeometrySemanticInfo GeometrySemanticRegistry[] =
        {
            {VertexSemantic::Unknown,        "Unknown",        {},            0},
            {VertexSemantic::Position,       "Position",       FloatShape(3),  1},
            {VertexSemantic::Normal,         "Normal",         FloatShape(3),  1},
            {VertexSemantic::Tangent,        "Tangent",        FloatShape(3),  1},
            {VertexSemantic::Bitangent,      "Binormal",       FloatShape(3),  1},
            {VertexSemantic::Color,          "Color",          FloatShape(4),  1},
            {VertexSemantic::Luminance,      "Luminance",      FloatShape(1),  1},
            {VertexSemantic::TexCoord,       "TexCoord",       FloatShape(2),  1},
            {VertexSemantic::AO,             "AO",             FloatShape(1),  1},
            {VertexSemantic::Size,           "Size",           FloatShape(2),  1},
            {VertexSemantic::Rotation,       "Rotation",       FloatShape(1),  1},
            {VertexSemantic::Assign,         "Assign",         UIntShape(1),   1},
            {VertexSemantic::JointID,        "JointID",        UIntShape(4),   1},
            {VertexSemantic::JointWeight,    "JointWeight",    FloatShape(4),  1},
            {VertexSemantic::TransformID,    "TransformID",    UIntShape(1),   1},
            {VertexSemantic::DataIndexID,    "DataIndexID",    UIntShape(1),   1}
        };

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
            {InterStageSemantic::WorldBinormal,  "fragWorldBinormal",  FloatShape(3), InterStageInterpolation::Smooth, 1, 9}
        };

        constexpr uint32 GeometrySemanticRegistryCount =
            static_cast<uint32>(sizeof(GeometrySemanticRegistry)
                / sizeof(GeometrySemanticRegistry[0]));
        constexpr uint32 InterStageSemanticRegistryCount =
            static_cast<uint32>(sizeof(InterStageSemanticRegistry)
                / sizeof(InterStageSemanticRegistry[0]));

        static_assert(
            GeometrySemanticRegistryCount
                == static_cast<uint32>(VertexSemantic::RANGE_SIZE),
            "Geometry semantic registry must cover VertexSemantic");
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

    uint32 GetGeometrySemanticInfoCount() noexcept
    {
        return GeometrySemanticRegistryCount;
    }

    const GeometrySemanticInfo *GetGeometrySemanticInfo(
        const VertexSemantic semantic) noexcept
    {
        const uint32 index = static_cast<uint32>(semantic);
        if (index >= GeometrySemanticRegistryCount)
            return nullptr;

        return GeometrySemanticRegistry + index;
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

    bool ResolveCurrentGeometrySemanticLocation(
        const GeometryVertexFormat &geometry,
        const VertexSemantic semantic,
        uint32 &out_location) noexcept
    {
        const GeometrySemanticInfo *info = GetGeometrySemanticInfo(semantic);
        if (!info
         || semantic == VertexSemantic::Unknown
         || info->location_policy != GeometrySemanticLocationPolicy::GeometryAttributeOrder)
            return false;

        for (uint32 index = 0; index < geometry.GetCount(); ++index)
        {
            const GeometryVertexAttributeFormat *attribute = geometry.Get(index);
            if (attribute && attribute->semantic == semantic)
            {
                out_location = index;
                return true;
            }
        }

        return false;
    }

    bool ValidateGeometrySemanticRegistry(
        ShaderSemanticRegistryValidationResult &out_result) noexcept
    {
        out_result = {};

        for (uint32 index = 0; index < GeometrySemanticRegistryCount; ++index)
        {
            const GeometrySemanticInfo &info = GeometrySemanticRegistry[index];
            if (static_cast<uint32>(info.semantic) != index)
                return SetValidationFailure(
                    out_result,
                    ShaderSemanticRegistryValidationError::GeometryEntryOrder,
                    index);

            if (index == 0)
                continue;

            if (!info.shader_symbol || !info.shader_symbol[0])
                return SetValidationFailure(
                    out_result,
                    ShaderSemanticRegistryValidationError::GeometryNameMissing,
                    index);

            if (!HasValidShape(info.default_shape) || info.location_width == 0)
                return SetValidationFailure(
                    out_result,
                    ShaderSemanticRegistryValidationError::GeometryTypeInvalid,
                    index);

            if (info.location_policy
                != GeometrySemanticLocationPolicy::GeometryAttributeOrder)
            {
                return SetValidationFailure(
                    out_result,
                    ShaderSemanticRegistryValidationError::
                        GeometryLocationPolicyInvalid,
                    index);
            }
        }

        return true;
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
        return ValidateGeometrySemanticRegistry(out_result)
            && ValidateInterStageSemanticRegistry(out_result);
    }
}
