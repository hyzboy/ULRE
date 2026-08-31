#pragma once

namespace hgl::graph::mtl {}

#include <hgl/CoreType.h>
#include <hgl/common/VertexAttribDef.h>

namespace hgl::graph
{
    class GeometryVertexFormat;
}

namespace hgl::graph::mtl
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

    enum class InterStageSemantic : uint8
    {
        Unknown = 0,
        DataIndexID,
        WorldPosition,
        WorldNormal,
        UV0,
        Color,
        FragDirection,
        Luminance,
        WorldTangent,
        WorldBinormal,
        StyleID,

        ENUM_CLASS_RANGE(Unknown,StyleID)
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

    // shape → GLSL 类型名（单一真源：mesh 侧 VaryingGen 与 FS 侧声明生成共用）。
    // 原 MaterialStageInterface.cpp 匿名命名空间实现提升至此。
    inline const char *GetGLSLTypeName(
        const ShaderSemanticScalarType scalar_type,
        const uint8 component_count) noexcept
    {
        if (component_count == 0 || component_count > 4)
            return nullptr;

        switch (scalar_type)
        {
        case ShaderSemanticScalarType::Float:
        {
            static const char *const names[] =
                {nullptr, "float", "vec2", "vec3", "vec4"};
            return names[component_count];
        }
        case ShaderSemanticScalarType::SignedInteger:
        {
            static const char *const names[] =
                {nullptr, "int", "ivec2", "ivec3", "ivec4"};
            return names[component_count];
        }
        case ShaderSemanticScalarType::UnsignedInteger:
        {
            static const char *const names[] =
                {nullptr, "uint", "uvec2", "uvec3", "uvec4"};
            return names[component_count];
        }
        case ShaderSemanticScalarType::Boolean:
        {
            static const char *const names[] =
                {nullptr, "bool", "bvec2", "bvec3", "bvec4"};
            return names[component_count];
        }
        default:
            return nullptr;
        }
    }

    constexpr InterStageSemanticMask GetInterStageSemanticMask(
        const InterStageSemantic semantic) noexcept
    {
        const uint32 value = static_cast<uint32>(semantic);
        return value > 0 && value < 32 ? InterStageSemanticMask(1u << value) : 0;
    }

    enum class ShaderSemanticRegistryValidationError : uint8
    {
        None = 0,
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

    uint32 GetInterStageSemanticInfoCount() noexcept;
    const InterStageSemanticInfo *GetInterStageSemanticInfo(
        InterStageSemantic semantic) noexcept;

    bool ValidateInterStageSemanticRegistry(
        ShaderSemanticRegistryValidationResult &out_result) noexcept;
    bool ValidateShaderSemanticRegistries(
        ShaderSemanticRegistryValidationResult &out_result) noexcept;
}//namespace hgl::graph::mtl
