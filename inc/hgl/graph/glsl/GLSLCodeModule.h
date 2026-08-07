#pragma once

#include <hgl/CoreType.h>
#include <hgl/mtl/DescriptorSemantic.h>
#include <hgl/graph/ssbo/SSBOTypes.h>
#include <hgl/graph/ssbo/TextureSlot.h>
#include <hgl/util/hash/FNV1a.h>

namespace hgl::graph::mtl
{
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
    };

    struct GLSLCodeModuleSSBORequirement
    {
        const char *name = nullptr;
        SSBOType ssbo_type = SSBOType::UserDefined;
        uint32 data_slot = 0;
        uint32 stage_flags = 0;
    };

    struct GLSLCodeModuleTextureRequirement
    {
        const char *name = nullptr;
        const char *glsl_type = nullptr;
        DescriptorSemantic semantic = DescriptorSemantic::MaterialTexture;
        TextureSlot slot = TextureSlot::BaseColor;
        uint32 stage_flags = 0;
        bool required = true;
    };

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
    };

    const GLSLCodeModuleDefinition *FindGLSLCodeModuleDefinition(GLSLCodeModuleID id) noexcept;
    bool TryGetGLSLCodeModuleIDByName(const char *name, GLSLCodeModuleID &out) noexcept;
    const char *GetGLSLCodeModuleName(GLSLCodeModuleID id) noexcept;
    uint64 GetGLSLCodeModuleDefinitionHash(GLSLCodeModuleID id) noexcept;

    inline bool IsValidGLSLCodeModuleDefinition(const GLSLCodeModuleDefinition &definition) noexcept
    {
        if (!definition.name || !*definition.name || !definition.glsl_code)
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
