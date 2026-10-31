#pragma once

#include <hgl/CoreType.h>
#include <hgl/mtl/PassProfile.h>
#include <hgl/type/StrChar.h>
#include <hgl/util/hash/FNV1a.h>

namespace hgl::graph::mtl
{
    // Reusable GLSL code is stage-agnostic. A module may be used by vertex,
    // fragment, or shared shader generation paths.
    //
    // Module identity is the unique `name` string (registered by name in the
    // GlslModuleRegistry; the contract layer derives stable IDs from it as
    // FNV1a(name)). There is deliberately no numeric ID track: file-backed
    // modules are discovered by directory scan and dependencies/conflicts
    // reference targets by name, resolved against the registry.

    enum class GlslModuleKind : uint8
    {
        Shared = 0,
        Surface,
        Position,
        Transform,
        Utility,
        FragmentShader
    };

    // ── Kind 名字注册表（单一真源；T1 口径：同上面的 Semantic 一处式）────
    inline constexpr uint32 GlslModuleKindCount =
        static_cast<uint32>(GlslModuleKind::FragmentShader) + 1;

    inline const char *GetGlslModuleKindName(
        const GlslModuleKind kind) noexcept
    {
        switch (kind)
        {
        case GlslModuleKind::Shared:         return "Shared";
        case GlslModuleKind::Surface:        return "Surface";
        case GlslModuleKind::Position:       return "Position";
        case GlslModuleKind::Transform:      return "Transform";
        case GlslModuleKind::Utility:        return "Utility";
        case GlslModuleKind::FragmentShader: return "FragmentShader";
        }
        return "Shared";
    }

    inline bool ParseGlslModuleKind(
        const char *name, GlslModuleKind &out_kind) noexcept
    {
        if (!name || !name[0])
            return false;

        for (uint32 i = 0; i < GlslModuleKindCount; ++i)
        {
            const GlslModuleKind kind = static_cast<GlslModuleKind>(i);
            if (hgl::strcmp(name, GetGlslModuleKindName(kind)) == 0)
            {
                out_kind = kind;
                return true;
            }
        }
        return false;
    }

    enum class GlslModuleSemantic : uint16
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
    // GlslModuleSemantic 的字符串名字只在本处定义一次：
    //   GetGlslModuleSemanticName：枚举 → 名字（switch）
    //   ParseGlslModuleSemantic：名字 → 枚举（线性查表，跳过 Unknown）
    // 新增语义只需改枚举 + 本 switch 一处。
    inline constexpr uint32 GlslModuleSemanticCount =
        static_cast<uint32>(GlslModuleSemantic::Size) + 1;

    inline const char *GetGlslModuleSemanticName(
        const GlslModuleSemantic semantic) noexcept
    {
        switch (semantic)
        {
        case GlslModuleSemantic::Unknown:       return "Unknown";
        case GlslModuleSemantic::Position:      return "Position";
        case GlslModuleSemantic::UV0:           return "UV0";
        case GlslModuleSemantic::Color:         return "Color";
        case GlslModuleSemantic::Normal:        return "Normal";
        case GlslModuleSemantic::Tangent:       return "Tangent";
        case GlslModuleSemantic::Binormal:      return "Binormal";
        case GlslModuleSemantic::WorldPosition: return "WorldPosition";
        case GlslModuleSemantic::WorldNormal:   return "WorldNormal";
        case GlslModuleSemantic::WorldTangent:  return "WorldTangent";
        case GlslModuleSemantic::WorldBinormal: return "WorldBinormal";
        case GlslModuleSemantic::Luminance:     return "Luminance";
        case GlslModuleSemantic::Camera:        return "Camera";
        case GlslModuleSemantic::Viewport:      return "Viewport";
        case GlslModuleSemantic::SkyLight:      return "SkyLight";
        case GlslModuleSemantic::MaterialData:  return "MaterialData";
        case GlslModuleSemantic::TransformID:   return "TransformID";
        case GlslModuleSemantic::Size:          return "Size";
        default:                                    return "Unknown";
        }
    }

    inline bool ParseGlslModuleSemantic(
        const char *name,
        GlslModuleSemantic &out_semantic) noexcept
    {
        if (!name || !name[0])
        {
            out_semantic = GlslModuleSemantic::Unknown;
            return false;
        }

        // 跳过 Unknown（旧解析表均不含它——"Unknown" 解析失败保持旧行为）
        for (uint32 i = 1; i < GlslModuleSemanticCount; ++i)
        {
            const GlslModuleSemantic semantic =
                static_cast<GlslModuleSemantic>(i);
            if (hgl::strcmp(name, GetGlslModuleSemanticName(semantic)) == 0)
            {
                out_semantic = semantic;
                return true;
            }
        }

        out_semantic = GlslModuleSemantic::Unknown;
        return false;
    }

    enum class GlslModuleCapabilitySource : uint8
    {
        GeometryAttribute = 0,
        Resource,
        ProducedSemantic
    };

    enum class GlslModuleNumericClass : uint32
    {
        None = 0,
        Float = 1u << 0,
        SignedInteger = 1u << 1,
        UnsignedInteger = 1u << 2,
        Normalized = 1u << 3,
        Packed = 1u << 4,
        Any = 0xffffffffu
    };

    enum class ShaderModuleCapability : uint32
    {
        None = 0,
        SurfaceBaseColor = 1u << 0,
        SurfaceNormal = 1u << 1,
        SurfaceMetallic = 1u << 2,
        SurfaceRoughness = 1u << 3,
        SurfaceOpacity = 1u << 4,
        /// 直接光“黑盒”能力。超薄架构把直接光拆成 diffuse/specular 两条独立轴后，
        /// 真实模板不再使用它；保留枚举值只为让历史 fixture 的解析不变成硬错误。
        DirectLight = 1u << 5,
        ShadowFactor = 1u << 6,
        AmbientDiffuse = 1u << 7,
        AmbientSpecular = 1u << 8,
        AmbientOcclusion = 1u << 9,
        ForwardOutput = 1u << 10,
        /// 直接光的漫反射项（由 diffuse_light_provider 槽提供）。
        DirectDiffuse = 1u << 11,
        /// 直接光的高光项（由 specular_light_provider 槽提供）。
        DirectSpecular = 1u << 12
    };


    struct GlslModuleDependency
    {
        const char *module_name = nullptr;      // 依赖目标模块名（注册表唯一键）
    };

    inline bool operator==(const GlslModuleDependency &lhs,
                           const GlslModuleDependency &rhs) noexcept
    {
        const bool same_name = lhs.module_name == rhs.module_name
            || (lhs.module_name && rhs.module_name
                && hgl::strcmp(lhs.module_name, rhs.module_name) == 0);
        return same_name;
    }

    struct GlslModuleSemanticRequirement
    {
        GlslModuleCapabilitySource source = GlslModuleCapabilitySource::GeometryAttribute;
        GlslModuleSemantic semantic = GlslModuleSemantic::Unknown;
        uint32 numeric_class_mask = static_cast<uint32>(GlslModuleNumericClass::Any);
        uint8 min_component_count = 0;
        uint8 max_component_count = 0;
    };

    // ValueArray<T> instantiates its virtual Find() for every concrete T, which
    // requires an equality operator.
    inline bool operator==(const GlslModuleSemanticRequirement &lhs,
                           const GlslModuleSemanticRequirement &rhs) noexcept
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
        const GlslModuleSemanticRequirement &v) noexcept
    {
        h << v.source
          << v.semantic
          << v.numeric_class_mask
          << v.min_component_count
          << v.max_component_count;
        return h;
    }

    // Name-keyed texture dependency consumed through MTL_TEX(). The name must
    // resolve to a MaterialDefinition texture declaration before compilation.
    struct GlslModuleTextureReferenceRequirement
    {
        const char *texture_name = nullptr;
        uint32 stage_flags = 0;
        bool required = true;
        bool allow_fallback = false;
    };

    inline bool operator==(
        const GlslModuleTextureReferenceRequirement &lhs,
        const GlslModuleTextureReferenceRequirement &rhs) noexcept
    {
        const bool same_name = lhs.texture_name == rhs.texture_name
            || (lhs.texture_name && rhs.texture_name
                && hgl::strcmp(lhs.texture_name, rhs.texture_name) == 0);
        return same_name
            && lhs.stage_flags == rhs.stage_flags
            && lhs.required == rhs.required
            && lhs.allow_fallback == rhs.allow_fallback;
    }

    inline hgl::hash::FNV1aHasher64 &operator<<(
        hgl::hash::FNV1aHasher64 &h,
        const GlslModuleTextureReferenceRequirement &v) noexcept
    {
        h << v.texture_name
          << v.stage_flags
          << v.required
          << v.allow_fallback;
        return h;
    }

    // Name-keyed sampler preset dependency. The name resolves against
    // sampler.toml (e.g. "Trilinear" => the module may use TrilinearSampler);
    // it is declared by the module itself so the module does not rely on the
    // material file happening to list the same preset. `required == false`
    // marks a preset used only on a conditional (material #define) code path.
    struct GlslModuleSamplerReferenceRequirement
    {
        const char *sampler_name = nullptr;
        bool required = true;
    };

    inline bool operator==(
        const GlslModuleSamplerReferenceRequirement &lhs,
        const GlslModuleSamplerReferenceRequirement &rhs) noexcept
    {
        const bool same_name = lhs.sampler_name == rhs.sampler_name
            || (lhs.sampler_name && rhs.sampler_name
                && hgl::strcmp(lhs.sampler_name, rhs.sampler_name) == 0);
        return same_name && lhs.required == rhs.required;
    }

    inline hgl::hash::FNV1aHasher64 &operator<<(
        hgl::hash::FNV1aHasher64 &h,
        const GlslModuleSamplerReferenceRequirement &v) noexcept
    {
        h << v.sampler_name
          << v.required;
        return h;
    }

    struct GlslModuleDefinition
    {
        const char *name = nullptr;
        const char *glsl_code = nullptr;

        // Capability metadata for file-backed/provider modules. Existing
        // all real modules carry kind/semantic fields.
        GlslModuleKind kind = GlslModuleKind::Shared;
        const GlslModuleSemanticRequirement *semantic_requirements = nullptr;
        uint32 semantic_requirement_count = 0;
        const GlslModuleSemantic *semantic_provides = nullptr;
        uint32 semantic_provide_count = 0;
        int32 priority = 0;
        uint32 flags = 0;

        const GlslModuleDependency *dependencies = nullptr;
        uint32 dependency_count = 0;

        // A root selected by PassProfileRequest occupies exactly one slot.
        // Dependency-only utility modules retain Unknown.
        ShaderModuleSlotRole slot_role = ShaderModuleSlotRole::Unknown;
        uint32 provided_capabilities = 0;
        uint32 required_capabilities = 0;

        const GlslModuleTextureReferenceRequirement
            *texture_reference_requirements = nullptr;
        uint32 texture_reference_requirement_count = 0;

        const GlslModuleSamplerReferenceRequirement
            *sampler_reference_requirements = nullptr;
        uint32 sampler_reference_requirement_count = 0;
    };

    uint64 GetGlslModuleDefinitionHash(
        const GlslModuleDefinition &definition) noexcept;

    inline bool IsValidGlslModuleDefinition(const GlslModuleDefinition &definition) noexcept
    {
        if (!definition.name || !*definition.name || !definition.glsl_code)
            return false;

        if ((definition.semantic_requirement_count > 0
          && !definition.semantic_requirements)
         || (definition.semantic_provide_count > 0
          && !definition.semantic_provides)
         || (definition.dependency_count > 0
          && !definition.dependencies)
         || (definition.texture_reference_requirement_count > 0
          && !definition.texture_reference_requirements)
         || (definition.sampler_reference_requirement_count > 0
          && !definition.sampler_reference_requirements))
            return false;

        for (uint32 i = 0; i < definition.semantic_requirement_count; ++i)
        {
            const auto &requirement = definition.semantic_requirements[i];
            if (requirement.semantic == GlslModuleSemantic::Unknown
             || requirement.numeric_class_mask == 0
             || (requirement.max_component_count != 0
              && requirement.min_component_count > requirement.max_component_count))
                return false;
        }

        for (uint32 i = 0;
             i < definition.texture_reference_requirement_count;
             ++i)
        {
            const auto &requirement =
                definition.texture_reference_requirements[i];
            if (!requirement.texture_name || !requirement.texture_name[0])
                return false;
        }

        for (uint32 i = 0;
             i < definition.sampler_reference_requirement_count;
             ++i)
        {
            const auto &requirement =
                definition.sampler_reference_requirements[i];
            if (!requirement.sampler_name || !requirement.sampler_name[0])
                return false;
        }

        return true;
    }
}
