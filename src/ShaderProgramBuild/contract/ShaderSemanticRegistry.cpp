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
            {InterStageSemantic::Unknown,        "Unknown",            {},           InterStageInterpolation::Smooth, 0, InvalidShaderSemanticLocation, nullptr},
            {InterStageSemantic::DataIndexID,    "fragDataIndexID",    UIntShape(1),  InterStageInterpolation::Flat,   1, 0, "HGL_LOCATION_DATA_INDEX_ID"},
            {InterStageSemantic::WorldPosition,  "fragWorldPos",       FloatShape(3), InterStageInterpolation::Smooth, 1, 1, "HGL_LOCATION_WORLD_POS"},
            {InterStageSemantic::WorldNormal,    "fragWorldNormal",    FloatShape(3), InterStageInterpolation::Smooth, 1, 2, "HGL_LOCATION_WORLD_NORMAL"},
            {InterStageSemantic::UV0,            "fragUV0",            FloatShape(2), InterStageInterpolation::Smooth, 1, 3, "HGL_LOCATION_UV0"},
            {InterStageSemantic::Color,          "fragVertexColor",    FloatShape(4), InterStageInterpolation::Smooth, 1, 5, "HGL_LOCATION_VERTEX_COLOR"},
            {InterStageSemantic::FragDirection,  "fragDirection",      FloatShape(3), InterStageInterpolation::Smooth, 1, 6, "HGL_LOCATION_FRAG_DIRECTION"},
            {InterStageSemantic::Luminance,      "fragLuminance",      FloatShape(1), InterStageInterpolation::Smooth, 1, 7, "HGL_LOCATION_LUMINANCE"},
            {InterStageSemantic::WorldTangent,   "fragWorldTangent",   FloatShape(3), InterStageInterpolation::Smooth, 1, 8, "HGL_LOCATION_WORLD_TANGENT"},
            {InterStageSemantic::WorldBinormal,  "fragWorldBinormal",  FloatShape(3), InterStageInterpolation::Smooth, 1, 9, "HGL_LOCATION_WORLD_BINORMAL"},
            {InterStageSemantic::StyleID,        "fragStyleID",        UIntShape(1),  InterStageInterpolation::Flat,   1, 4, "HGL_LOCATION_STYLE_ID"}
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

        // 稳定 ABI 的 GLSL 宏名固定为 HGL_LOCATION_ 前缀 + 大写标识符，
        // 与 interstage_locations.glsl 的书写风格保持一致。
        constexpr const char kLocationMacroPrefix[] = "HGL_LOCATION_";
        constexpr uint32 kLocationMacroPrefixLength =
            static_cast<uint32>(sizeof(kLocationMacroPrefix) - 1);

        bool IsLocationMacroName(const char *macro_name) noexcept
        {
            if (!macro_name)
                return false;

            if (std::strncmp(macro_name, kLocationMacroPrefix, kLocationMacroPrefixLength) != 0)
                return false;

            const char *suffix = macro_name + kLocationMacroPrefixLength;
            if (!suffix[0])
                return false;

            for (const char *c = suffix; *c; ++c)
            {
                const bool valid = (*c >= 'A' && *c <= 'Z')
                    || (*c >= '0' && *c <= '9')
                    || *c == '_';
                if (!valid)
                    return false;
            }

            return true;
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

    const char *GetInterStageSemanticLocationMacro(
        const InterStageSemantic semantic) noexcept
    {
        const InterStageSemanticInfo *info = GetInterStageSemanticInfo(semantic);
        if (!info || !info->location_macro || !info->location_macro[0])
            return nullptr;

        return info->location_macro;
    }

    InterStageSemantic FindInterStageSemanticByLocationMacro(
        const char *location_macro) noexcept
    {
        if (!location_macro || !location_macro[0])
            return InterStageSemantic::Unknown;

        for (uint32 index = 1; index < InterStageSemanticRegistryCount; ++index)
        {
            const InterStageSemanticInfo &info = InterStageSemanticRegistry[index];
            if (info.location_macro
             && std::strcmp(info.location_macro, location_macro) == 0)
                return info.semantic;
        }

        return InterStageSemantic::Unknown;
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

            // GLSL 侧宏名也是稳定 ABI 的一部分：C++ 侧只在这里登记，
            // 门禁负责与 interstage_locations.glsl 的实际数值对拍。
            if (!info.location_macro || !info.location_macro[0])
                return SetValidationFailure(
                    out_result,
                    ShaderSemanticRegistryValidationError::
                        InterStageLocationMacroMissing,
                    index);

            if (!IsLocationMacroName(info.location_macro))
                return SetValidationFailure(
                    out_result,
                    ShaderSemanticRegistryValidationError::
                        InterStageLocationMacroInvalid,
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

                if (std::strcmp(info.location_macro, other.location_macro) == 0)
                    return SetValidationFailure(
                        out_result,
                        ShaderSemanticRegistryValidationError::
                            InterStageLocationMacroConflict,
                        other_index,
                        index);

            }
        }

        return true;
    }

}
