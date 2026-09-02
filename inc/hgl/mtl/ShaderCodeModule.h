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
    // ShaderCodeModuleRegistry; the contract layer derives stable IDs from it as
    // FNV1a(name)). There is deliberately no numeric ID track: file-backed
    // modules are discovered by directory scan and dependencies/conflicts
    // reference targets by name, resolved against the registry.

    enum class ShaderCodeModuleKind : uint8
    {
        Shared = 0,
        Surface,
        Position,
        Transform,
        Utility,
        FragmentShader
    };

    enum class ShaderCodeModuleSemantic : uint16
    {
        Unknown = 0,
        Position,
        UV0,
        Color,
        Normal,
        Tangent,
        Binormal,
        WorldPosition,
        WorldNormal,
        WorldTangent,
        WorldBinormal,
        Luminance,
        Camera,
        Viewport,
        SkyLight,
        MaterialData,
        TransformID,
        Size
    };
    // 2026-09 清扫：ColorY/ColorUV/HeightMap 死枚举值删除（ShaderLibrary
    // 全部模块 0 引用，名表 switch 同步收敛）。

    // ── 语义名字注册表（单一真源，T1）───────────────────────────────
    // ShaderCodeModuleSemantic 的字符串名字只在本处定义一次：
    //   GetShaderCodeModuleSemanticName：枚举 → 名字（switch）
    //   ParseShaderCodeModuleSemantic：名字 → 枚举（线性查表，跳过 Unknown）
    // 新增语义只需改枚举 + 本 switch 一处。
    inline constexpr uint32 ShaderCodeModuleSemanticCount =
        static_cast<uint32>(ShaderCodeModuleSemantic::Size) + 1;

    inline const char *GetShaderCodeModuleSemanticName(
        const ShaderCodeModuleSemantic semantic) noexcept
    {
        switch (semantic)
        {
        case ShaderCodeModuleSemantic::Unknown:       return "Unknown";
        case ShaderCodeModuleSemantic::Position:      return "Position";
        case ShaderCodeModuleSemantic::UV0:           return "UV0";
        case ShaderCodeModuleSemantic::Color:         return "Color";
        case ShaderCodeModuleSemantic::Normal:        return "Normal";
        case ShaderCodeModuleSemantic::Tangent:       return "Tangent";
        case ShaderCodeModuleSemantic::Binormal:      return "Binormal";
        case ShaderCodeModuleSemantic::WorldPosition: return "WorldPosition";
        case ShaderCodeModuleSemantic::WorldNormal:   return "WorldNormal";
        case ShaderCodeModuleSemantic::WorldTangent:  return "WorldTangent";
        case ShaderCodeModuleSemantic::WorldBinormal: return "WorldBinormal";
        case ShaderCodeModuleSemantic::Luminance:     return "Luminance";
        case ShaderCodeModuleSemantic::Camera:        return "Camera";
        case ShaderCodeModuleSemantic::Viewport:      return "Viewport";
        case ShaderCodeModuleSemantic::SkyLight:      return "SkyLight";
        case ShaderCodeModuleSemantic::MaterialData:  return "MaterialData";
        case ShaderCodeModuleSemantic::TransformID:   return "TransformID";
        case ShaderCodeModuleSemantic::Size:          return "Size";
        default:                                    return "Unknown";
        }
    }

    inline bool ParseShaderCodeModuleSemantic(
        const char *name,
        ShaderCodeModuleSemantic &out_semantic) noexcept
    {
        if (!name || !name[0])
        {
            out_semantic = ShaderCodeModuleSemantic::Unknown;
            return false;
        }

        // 跳过 Unknown（旧解析表均不含它——"Unknown" 解析失败保持旧行为）
        for (uint32 i = 1; i < ShaderCodeModuleSemanticCount; ++i)
        {
            const ShaderCodeModuleSemantic semantic =
                static_cast<ShaderCodeModuleSemantic>(i);
            if (hgl::strcmp(name, GetShaderCodeModuleSemanticName(semantic)) == 0)
            {
                out_semantic = semantic;
                return true;
            }
        }

        out_semantic = ShaderCodeModuleSemantic::Unknown;
        return false;
    }

    enum class ShaderCodeModuleCapabilitySource : uint8
    {
        GeometryAttribute = 0,
        Resource,
        ProducedSemantic
    };

    enum class ShaderCodeModuleNumericClass : uint32
    {
        None = 0,
        Float = 1u << 0,
        SignedInteger = 1u << 1,
        UnsignedInteger = 1u << 2,
        Normalized = 1u << 3,
        Packed = 1u << 4,
        Any = 0xffffffffu
    };


    struct ShaderCodeModuleDependency
    {
        const char *module_name = nullptr;      // 依赖目标模块名（注册表唯一键）
    };

    inline bool operator==(const ShaderCodeModuleDependency &lhs,
                           const ShaderCodeModuleDependency &rhs) noexcept
    {
        const bool same_name = lhs.module_name == rhs.module_name
            || (lhs.module_name && rhs.module_name
                && hgl::strcmp(lhs.module_name, rhs.module_name) == 0);
        return same_name;
    }

    struct ShaderCodeModuleSemanticRequirement
    {
        ShaderCodeModuleCapabilitySource source = ShaderCodeModuleCapabilitySource::GeometryAttribute;
        ShaderCodeModuleSemantic semantic = ShaderCodeModuleSemantic::Unknown;
        uint32 numeric_class_mask = static_cast<uint32>(ShaderCodeModuleNumericClass::Any);
        uint8 min_component_count = 0;
        uint8 max_component_count = 0;
    };

    // ValueArray<T> instantiates its virtual Find() for every concrete T, which
    // requires an equality operator.
    inline bool operator==(const ShaderCodeModuleSemanticRequirement &lhs,
                           const ShaderCodeModuleSemanticRequirement &rhs) noexcept
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
        const ShaderCodeModuleSemanticRequirement &v) noexcept
    {
        h << v.source
          << v.semantic
          << v.numeric_class_mask
          << v.min_component_count
          << v.max_component_count;
        return h;
    }

    struct ShaderCodeModuleSSBORequirement
    {
        const char *name = nullptr;
        SSBOType ssbo_type = SSBOType::UserDefined;
        uint32 material_private_data_slot = 0;
        uint32 stage_flags = 0;
        bool required = true;
        bool allow_fallback = false;
    };

    inline bool operator==(const ShaderCodeModuleSSBORequirement &lhs,
                           const ShaderCodeModuleSSBORequirement &rhs) noexcept
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

    struct ShaderCodeModuleTextureLayerRequirement
    {
        TextureSlot slot = TextureSlot::Custom0;
        uint32 stage_flags = 0;
        bool required = true;
        bool allow_fallback = false;
    };

    inline bool operator==(const ShaderCodeModuleTextureLayerRequirement &lhs,
                           const ShaderCodeModuleTextureLayerRequirement &rhs) noexcept
    {
        return lhs.slot == rhs.slot
            && lhs.stage_flags == rhs.stage_flags
            && lhs.required == rhs.required
            && lhs.allow_fallback == rhs.allow_fallback;
    }

    inline hgl::hash::FNV1aHasher64 &operator<<(
        hgl::hash::FNV1aHasher64 &h,
        const ShaderCodeModuleTextureLayerRequirement &v) noexcept
    {
        h << v.slot
          << v.stage_flags
          << v.required
          << v.allow_fallback;
        return h;
    }

    struct ShaderCodeModuleDefinition
    {
        const char *name = nullptr;
        const char *glsl_code = nullptr;

        const ShaderCodeModuleSSBORequirement *ssbo_requirements = nullptr;
        uint32 ssbo_requirement_count = 0;

        // Capability metadata for file-backed/provider modules. Existing
        // all real modules carry kind/semantic fields.
        ShaderCodeModuleKind kind = ShaderCodeModuleKind::Shared;
        const ShaderCodeModuleSemanticRequirement *semantic_requirements = nullptr;
        uint32 semantic_requirement_count = 0;
        const ShaderCodeModuleSemantic *semantic_provides = nullptr;
        uint32 semantic_provide_count = 0;
        int32 priority = 0;
        uint32 flags = 0;

        const ShaderCodeModuleTextureLayerRequirement *texture_layer_requirements = nullptr;
        uint32 texture_layer_requirement_count = 0;

        const ShaderCodeModuleDependency *dependencies = nullptr;
        uint32 dependency_count = 0;

        // Names of mutually exclusive modules. The registry resolves them at
        // load time; strings are owned by the registry's file-data storage.
        const char **module_conflict_names = nullptr;
        uint32 module_conflict_count = 0;
    };

    uint64 GetShaderCodeModuleDefinitionHash(
        const ShaderCodeModuleDefinition &definition) noexcept;

    inline bool IsValidShaderCodeModuleDefinition(const ShaderCodeModuleDefinition &definition) noexcept
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
            if (requirement.semantic == ShaderCodeModuleSemantic::Unknown
             || requirement.numeric_class_mask == 0
             || (requirement.max_component_count != 0
              && requirement.min_component_count > requirement.max_component_count))
                return false;
        }

        return true;
    }
}
