#pragma once

#include <hgl/CoreType.h>
#include <hgl/mtl/DescriptorSemantic.h>
#include <hgl/graph/ssbo/SSBOTypes.h>
#include <hgl/graph/ssbo/TextureSlot.h>
#include <hgl/type/StrChar.h>
#include <hgl/util/hash/FNV1a.h>

namespace hgl::graph::mtl
{
    // Reusable GLSL code is stage-agnostic. A module may be used by vertex,
    // fragment, or shared shader generation paths.
    //
    // Module identity is the unique `name` string (registered by name in the
    // GLSLCodeModuleRegistry; the contract layer derives stable IDs from it as
    // FNV1a(name)). There is deliberately no numeric ID track: file-backed
    // modules are discovered by directory scan and dependencies/conflicts
    // reference targets by name, resolved against the registry.

    enum class GLSLCodeModuleKind : uint8
    {
        Shared = 0,
        Surface,
        Position,
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
        TransformID,
        Size
    };

    // ── 语义名字注册表（单一真源，T1）───────────────────────────────
    // GLSLCodeModuleSemantic 的字符串名字只在本处定义一次：
    //   GetGLSLCodeModuleSemanticName：枚举 → 名字（switch）
    //   ParseGLSLCodeModuleSemantic：名字 → 枚举（线性查表，跳过 Unknown）
    // 旧 MaterialDefinitionFile.cpp / GLSLCodeModuleFile.cpp 各自维护的
    // 平行名字表已删除——新增语义只需改枚举 + 本 switch 一处。
    inline constexpr uint32 GLSLCodeModuleSemanticCount =
        static_cast<uint32>(GLSLCodeModuleSemantic::Size) + 1;

    inline const char *GetGLSLCodeModuleSemanticName(
        const GLSLCodeModuleSemantic semantic) noexcept
    {
        switch (semantic)
        {
        case GLSLCodeModuleSemantic::Unknown:       return "Unknown";
        case GLSLCodeModuleSemantic::Position:      return "Position";
        case GLSLCodeModuleSemantic::UV0:           return "UV0";
        case GLSLCodeModuleSemantic::Color:         return "Color";
        case GLSLCodeModuleSemantic::ColorY:        return "ColorY";
        case GLSLCodeModuleSemantic::ColorUV:       return "ColorUV";
        case GLSLCodeModuleSemantic::Normal:        return "Normal";
        case GLSLCodeModuleSemantic::Tangent:       return "Tangent";
        case GLSLCodeModuleSemantic::Binormal:      return "Binormal";
        case GLSLCodeModuleSemantic::WorldPosition: return "WorldPosition";
        case GLSLCodeModuleSemantic::WorldNormal:   return "WorldNormal";
        case GLSLCodeModuleSemantic::WorldTangent:  return "WorldTangent";
        case GLSLCodeModuleSemantic::WorldBinormal: return "WorldBinormal";
        case GLSLCodeModuleSemantic::Luminance:     return "Luminance";
        case GLSLCodeModuleSemantic::HeightMap:     return "HeightMap";
        case GLSLCodeModuleSemantic::Camera:        return "Camera";
        case GLSLCodeModuleSemantic::Viewport:      return "Viewport";
        case GLSLCodeModuleSemantic::SkyLight:      return "SkyLight";
        case GLSLCodeModuleSemantic::MaterialData:  return "MaterialData";
        case GLSLCodeModuleSemantic::TransformID:   return "TransformID";
        case GLSLCodeModuleSemantic::Size:          return "Size";
        default:                                    return "Unknown";
        }
    }

    inline bool ParseGLSLCodeModuleSemantic(
        const char *name,
        GLSLCodeModuleSemantic &out_semantic) noexcept
    {
        if (!name || !name[0])
        {
            out_semantic = GLSLCodeModuleSemantic::Unknown;
            return false;
        }

        // 跳过 Unknown（旧解析表均不含它——"Unknown" 解析失败保持旧行为）
        for (uint32 i = 1; i < GLSLCodeModuleSemanticCount; ++i)
        {
            const GLSLCodeModuleSemantic semantic =
                static_cast<GLSLCodeModuleSemantic>(i);
            if (hgl::strcmp(name, GetGLSLCodeModuleSemanticName(semantic)) == 0)
            {
                out_semantic = semantic;
                return true;
            }
        }

        out_semantic = GLSLCodeModuleSemantic::Unknown;
        return false;
    }

    enum class GLSLCodeModuleCapabilitySource : uint8
    {
        GeometryAttribute = 0,
        Resource,
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


    struct GLSLCodeModuleDependency
    {
        const char *module_name = nullptr;      // 依赖目标模块名（注册表唯一键）
    };

    inline bool operator==(const GLSLCodeModuleDependency &lhs,
                           const GLSLCodeModuleDependency &rhs) noexcept
    {
        const bool same_name = lhs.module_name == rhs.module_name
            || (lhs.module_name && rhs.module_name
                && hgl::strcmp(lhs.module_name, rhs.module_name) == 0);
        return same_name;
    }

    struct GLSLCodeModuleSemanticRequirement
    {
        GLSLCodeModuleCapabilitySource source = GLSLCodeModuleCapabilitySource::GeometryAttribute;
        GLSLCodeModuleSemantic semantic = GLSLCodeModuleSemantic::Unknown;
        uint32 numeric_class_mask = static_cast<uint32>(GLSLCodeModuleNumericClass::Any);
        uint8 min_component_count = 0;
        uint8 max_component_count = 0;
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

    // Explicit per-field hashing: the generic FNV1aHasher::operator<<(const T&)
    // hashes raw struct bytes including padding, which is layout-coupled and
    // indeterminate for partially-initialized objects. Cache keys must depend
    // only on the semantic fields.
    inline hgl::hash::FNV1aHasher64 &operator<<(
        hgl::hash::FNV1aHasher64 &h,
        const GLSLCodeModuleSemanticRequirement &v) noexcept
    {
        h << v.source
          << v.semantic
          << v.numeric_class_mask
          << v.min_component_count
          << v.max_component_count;
        return h;
    }

    struct GLSLCodeModuleSSBORequirement
    {
        const char *name = nullptr;
        SSBOType ssbo_type = SSBOType::UserDefined;
        uint32 material_private_data_slot = 0;
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
            && lhs.material_private_data_slot == rhs.material_private_data_slot
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

    inline hgl::hash::FNV1aHasher64 &operator<<(
        hgl::hash::FNV1aHasher64 &h,
        const GLSLCodeModuleTextureLayerRequirement &v) noexcept
    {
        h << v.slot
          << v.stage_flags
          << v.required
          << v.allow_fallback;
        return h;
    }

    struct GLSLCodeModuleDefinition
    {
        const char *name = nullptr;
        const char *glsl_code = nullptr;

        const GLSLCodeModuleSSBORequirement *ssbo_requirements = nullptr;
        uint32 ssbo_requirement_count = 0;

        // Capability metadata for file-backed/provider modules. Existing
        // all real modules carry kind/semantic fields.
        GLSLCodeModuleKind kind = GLSLCodeModuleKind::Shared;
        const GLSLCodeModuleSemanticRequirement *semantic_requirements = nullptr;
        uint32 semantic_requirement_count = 0;
        const GLSLCodeModuleSemantic *semantic_provides = nullptr;
        uint32 semantic_provide_count = 0;
        int32 priority = 0;
        uint32 flags = 0;

        const GLSLCodeModuleTextureLayerRequirement *texture_layer_requirements = nullptr;
        uint32 texture_layer_requirement_count = 0;

        const GLSLCodeModuleDependency *dependencies = nullptr;
        uint32 dependency_count = 0;

        // Names of mutually exclusive modules. The registry resolves them at
        // load time; strings are owned by the registry's file-data storage.
        const char **module_conflict_names = nullptr;
        uint32 module_conflict_count = 0;
    };

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
         || (definition.dependency_count > 0
          && !definition.dependencies)
         || (definition.ssbo_requirement_count > 0
          && !definition.ssbo_requirements)
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
