#pragma once

#include <hgl/CoreType.h>
#include <hgl/mtl/DescriptorSemantic.h>
#include <hgl/graph/ssbo/SSBOTypes.h>
#include <hgl/graph/ssbo/TextureSlot.h>
#include <hgl/type/StrChar.h>
#include <hgl/util/hash/FNV1a.h>

namespace hgl::graph::mtl
{
    constexpr uint16 GLSLCodeModuleUnversionedMetadataVersion = 0;
    constexpr uint16 GLSLCodeModuleCurrentMetadataVersion = 1;

    // Reusable GLSL code is stage-agnostic. A module may be used by vertex,
    // fragment, or shared shader generation paths.
    enum class GLSLCodeModuleID : uint16
    {
        SkyLightHeader = 0,
        SkyLightSimple,
        SkyLightCubeMap,
        PBRSurface,
        ENUM_CLASS_RANGE(SkyLightHeader, PBRSurface)
    };

    enum class GLSLCodeModuleKind : uint8
    {
        Shared = 0,
        Surface,
        VertexInput,
        Position,
        Basis,
        Decode,
        Transform,
        Utility,
        FragmentShader
    };

    enum class GLSLCodeModuleSemantic : uint16
    {
        Unknown = 0,
        Position,
        UV0,
        Color,
        ColorY,
        ColorUV,
        Normal,
        Tangent,
        Binormal,
        WorldPosition,
        WorldNormal,
        WorldTangent,
        WorldBinormal,
        Luminance,
        HeightMap,
        Camera,
        Viewport,
        SkyLight,
        MaterialData,
        TransformID
    };

    enum class GLSLCodeModuleCapabilitySource : uint8
    {
        GeometryAttribute = 0,
        Resource,
        Option,
        ProducedSemantic
    };

    enum class GLSLCodeModuleNumericClass : uint32
    {
        None = 0,
        Float = 1u << 0,
        SignedInteger = 1u << 1,
        UnsignedInteger = 1u << 2,
        Normalized = 1u << 3,
        Packed = 1u << 4,
        Any = 0xffffffffu
    };

    enum class GLSLCodeModuleProviderFlag : uint32
    {
        None = 0,
        Exclusive = 1u << 0
    };

    enum class GLSLCodeModuleConditionDomain : uint8
    {
        Option = 0,
        ShaderProgramPurpose,
        SurfaceProfile,
        DeviceFeature
    };

    enum class GLSLCodeModuleConditionOperator : uint8
    {
        Equals = 0,
        NotEquals
    };

    struct GLSLCodeModuleCondition
    {
        GLSLCodeModuleConditionDomain domain =
            GLSLCodeModuleConditionDomain::Option;
        GLSLCodeModuleConditionOperator operation =
            GLSLCodeModuleConditionOperator::Equals;
        const char *key = nullptr;
        const char *value = nullptr;
    };

    inline bool operator==(const GLSLCodeModuleCondition &lhs,
                           const GLSLCodeModuleCondition &rhs) noexcept
    {
        const bool same_key = lhs.key == rhs.key
            || (lhs.key && rhs.key && hgl::strcmp(lhs.key, rhs.key) == 0);
        const bool same_value = lhs.value == rhs.value
            || (lhs.value && rhs.value && hgl::strcmp(lhs.value, rhs.value) == 0);
        return lhs.domain == rhs.domain
            && lhs.operation == rhs.operation
            && same_key
            && same_value;
    }

    struct GLSLCodeModuleDependency
    {
        GLSLCodeModuleID module_id = GLSLCodeModuleID::SkyLightHeader;
        uint16 min_metadata_version =
            GLSLCodeModuleUnversionedMetadataVersion;
        uint16 max_metadata_version = GLSLCodeModuleCurrentMetadataVersion;
    };

    inline bool operator==(const GLSLCodeModuleDependency &lhs,
                           const GLSLCodeModuleDependency &rhs) noexcept
    {
        return lhs.module_id == rhs.module_id
            && lhs.min_metadata_version == rhs.min_metadata_version
            && lhs.max_metadata_version == rhs.max_metadata_version;
    }

    struct GLSLCodeModuleSemanticRequirement
    {
        GLSLCodeModuleCapabilitySource source = GLSLCodeModuleCapabilitySource::GeometryAttribute;
        GLSLCodeModuleSemantic semantic = GLSLCodeModuleSemantic::Unknown;
        uint32 numeric_class_mask = static_cast<uint32>(GLSLCodeModuleNumericClass::Any);
        uint8 min_component_count = 0;
        uint8 max_component_count = 0;
        uint16 reserved = 0;
    };

    // ValueArray<T> instantiates its virtual Find() for every concrete T, which
    // requires an equality operator.
    inline bool operator==(const GLSLCodeModuleSemanticRequirement &lhs,
                           const GLSLCodeModuleSemanticRequirement &rhs) noexcept
    {
        return lhs.source == rhs.source
            && lhs.semantic == rhs.semantic
            && lhs.numeric_class_mask == rhs.numeric_class_mask
            && lhs.min_component_count == rhs.min_component_count
            && lhs.max_component_count == rhs.max_component_count;
    }

    struct GLSLCodeModuleUBORequirement
    {
        UBODescriptorSemantic semantic = UBODescriptorSemantic::ViewportInfo;
        uint32 stage_flags = 0;
        bool required = true;
        bool allow_fallback = false;
    };

    inline bool operator==(const GLSLCodeModuleUBORequirement &lhs,
                           const GLSLCodeModuleUBORequirement &rhs) noexcept
    {
        return lhs.semantic == rhs.semantic
            && lhs.stage_flags == rhs.stage_flags
            && lhs.required == rhs.required
            && lhs.allow_fallback == rhs.allow_fallback;
    }

    struct GLSLCodeModuleSSBORequirement
    {
        const char *name = nullptr;
        SSBOType ssbo_type = SSBOType::UserDefined;
        uint32 data_slot = 0;
        uint32 stage_flags = 0;
        bool required = true;
        bool allow_fallback = false;
    };

    inline bool operator==(const GLSLCodeModuleSSBORequirement &lhs,
                           const GLSLCodeModuleSSBORequirement &rhs) noexcept
    {
        const bool same_name = lhs.name == rhs.name
            || (lhs.name && rhs.name && hgl::strcmp(lhs.name, rhs.name) == 0);
        return same_name
            && lhs.ssbo_type == rhs.ssbo_type
            && lhs.data_slot == rhs.data_slot
            && lhs.stage_flags == rhs.stage_flags
            && lhs.required == rhs.required
            && lhs.allow_fallback == rhs.allow_fallback;
    }

    struct GLSLCodeModuleTextureRequirement
    {
        const char *name = nullptr;
        const char *glsl_type = nullptr;
        DescriptorSemantic semantic = DescriptorSemantic::MaterialTexture;
        TextureSlot slot = TextureSlot::BaseColor;
        uint32 stage_flags = 0;
        bool required = true;
        bool allow_fallback = false;
    };

    inline bool operator==(const GLSLCodeModuleTextureRequirement &lhs,
                           const GLSLCodeModuleTextureRequirement &rhs) noexcept
    {
        const bool same_name = lhs.name == rhs.name
            || (lhs.name && rhs.name && hgl::strcmp(lhs.name, rhs.name) == 0);
        const bool same_glsl_type = lhs.glsl_type == rhs.glsl_type
            || (lhs.glsl_type && rhs.glsl_type && hgl::strcmp(lhs.glsl_type, rhs.glsl_type) == 0);
        return same_name
            && same_glsl_type
            && lhs.semantic == rhs.semantic
            && lhs.slot == rhs.slot
            && lhs.stage_flags == rhs.stage_flags
            && lhs.required == rhs.required
            && lhs.allow_fallback == rhs.allow_fallback;
    }

    struct GLSLCodeModuleTextureLayerRequirement
    {
        TextureSlot slot = TextureSlot::Custom0;
        uint32 stage_flags = 0;
        bool required = true;
        bool allow_fallback = false;
    };

    inline bool operator==(const GLSLCodeModuleTextureLayerRequirement &lhs,
                           const GLSLCodeModuleTextureLayerRequirement &rhs) noexcept
    {
        return lhs.slot == rhs.slot
            && lhs.stage_flags == rhs.stage_flags
            && lhs.required == rhs.required
            && lhs.allow_fallback == rhs.allow_fallback;
    }

    struct GLSLCodeModuleDefinition
    {
        GLSLCodeModuleID id = GLSLCodeModuleID::SkyLightHeader;
        const char *name = nullptr;
        const char *glsl_code = nullptr;

        const GLSLCodeModuleUBORequirement *ubo_requirements = nullptr;
        uint32 ubo_requirement_count = 0;

        const GLSLCodeModuleSSBORequirement *ssbo_requirements = nullptr;
        uint32 ssbo_requirement_count = 0;

        const GLSLCodeModuleTextureRequirement *texture_requirements = nullptr;
        uint32 texture_requirement_count = 0;

        const GLSLCodeModuleID *code_module_requirements = nullptr;
        uint32 code_module_requirement_count = 0;

        // Capability metadata for file-backed/provider modules. Existing
        // modules may omit these trailing fields until migrated.
        GLSLCodeModuleKind kind = GLSLCodeModuleKind::Shared;
        const GLSLCodeModuleSemanticRequirement *semantic_requirements = nullptr;
        uint32 semantic_requirement_count = 0;
        const GLSLCodeModuleSemantic *semantic_provides = nullptr;
        uint32 semantic_provide_count = 0;
        int32 priority = 0;
        uint32 flags = 0;

        const GLSLCodeModuleTextureLayerRequirement *texture_layer_requirements = nullptr;
        uint32 texture_layer_requirement_count = 0;

        uint16 metadata_version =
            GLSLCodeModuleUnversionedMetadataVersion;
        uint16 metadata_reserved = 0;

        const GLSLCodeModuleDependency *dependencies = nullptr;
        uint32 dependency_count = 0;

        const GLSLCodeModuleCondition *conditions = nullptr;
        uint32 condition_count = 0;

        const GLSLCodeModuleID *module_conflicts = nullptr;
        uint32 module_conflict_count = 0;
    };

    const GLSLCodeModuleDefinition *FindGLSLCodeModuleDefinition(GLSLCodeModuleID id) noexcept;
    bool TryGetGLSLCodeModuleIDByName(const char *name, GLSLCodeModuleID &out) noexcept;
    const char *GetGLSLCodeModuleName(GLSLCodeModuleID id) noexcept;
    uint64 GetGLSLCodeModuleDefinitionHash(GLSLCodeModuleID id) noexcept;
    uint64 GetGLSLCodeModuleDefinitionHash(
        const GLSLCodeModuleDefinition &definition) noexcept;

    inline bool IsValidGLSLCodeModuleDefinition(const GLSLCodeModuleDefinition &definition) noexcept
    {
        if (!definition.name || !*definition.name || !definition.glsl_code)
            return false;

        if ((definition.semantic_requirement_count > 0
          && !definition.semantic_requirements)
         || (definition.semantic_provide_count > 0
          && !definition.semantic_provides)
         || (definition.code_module_requirement_count > 0
          && !definition.code_module_requirements)
         || (definition.ubo_requirement_count > 0
          && !definition.ubo_requirements)
         || (definition.ssbo_requirement_count > 0
          && !definition.ssbo_requirements)
         || (definition.texture_requirement_count > 0
          && !definition.texture_requirements)
         || (definition.texture_layer_requirement_count > 0
          && !definition.texture_layer_requirements))
            return false;

        for (uint32 i = 0; i < definition.semantic_requirement_count; ++i)
        {
            const auto &requirement = definition.semantic_requirements[i];
            if (requirement.semantic == GLSLCodeModuleSemantic::Unknown
             || requirement.numeric_class_mask == 0
             || (requirement.max_component_count != 0
              && requirement.min_component_count > requirement.max_component_count))
                return false;
        }

        return true;
    }
}
