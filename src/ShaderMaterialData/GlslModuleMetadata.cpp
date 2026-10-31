#include <hgl/mtl/GlslModuleMetadata.h>

#include <hgl/type/StrChar.h>

#include <cstring>

namespace hgl::graph::mtl
{
    namespace
    {
        bool SetMetadataFailure(
            GlslModuleMetadataValidationDiagnostic &out_diagnostic,
            const GlslModuleMetadataValidationError error,
            const char *module_name,
            const char *related_module_name = nullptr,
            const GlslModuleSemantic semantic =
                GlslModuleSemantic::Unknown) noexcept
        {
            out_diagnostic.error = error;
            out_diagnostic.module_name = module_name ? module_name : "";
            out_diagnostic.related_module_name =
                related_module_name ? related_module_name : "";
            out_diagnostic.semantic = semantic;
            return false;
        }

        bool HasValidArray(const void *data, const uint32 count) noexcept
        {
            return count == 0 || data != nullptr;
        }

        bool IsValidRequirement(
            const GlslModuleSemanticRequirement &requirement) noexcept
        {
            return requirement.source >=
                    GlslModuleCapabilitySource::GeometryAttribute
                && requirement.source <=
                    GlslModuleCapabilitySource::ProducedSemantic
                && requirement.semantic != GlslModuleSemantic::Unknown
                && requirement.numeric_class_mask != 0
                && requirement.min_component_count <= 4
                && requirement.max_component_count <= 4
                && (requirement.max_component_count == 0
                 || requirement.min_component_count
                    <= requirement.max_component_count);
        }

        bool SameName(const char *lhs, const char *rhs) noexcept
        {
            return lhs == rhs
                || (lhs && rhs && std::strcmp(lhs, rhs) == 0);
        }

        bool IsProviderCandidate(
            const GlslModuleDefinition &definition) noexcept
        {
            return definition.semantic_provide_count > 0
                && definition.kind != GlslModuleKind::Surface
                && definition.kind != GlslModuleKind::FragmentShader;
        }
    }

    const char *GetGlslModuleMetadataValidationErrorName(
        const GlslModuleMetadataValidationError error) noexcept
    {
#define HGL_ERROR(name) case GlslModuleMetadataValidationError::name: return #name;
        switch (error)
        {
            HGL_GLSL_CODE_MODULE_METADATA_VALIDATION_ERROR_LIST
        }
#undef HGL_ERROR
        return "Unknown";
    }

    bool ValidateGlslModuleMetadata(
        const GlslModuleDefinition &definition,
        GlslModuleMetadataValidationDiagnostic &out_diagnostic) noexcept
    {
        out_diagnostic = {};
        out_diagnostic.module_name = definition.name ? definition.name : "";

        if (!IsValidGlslModuleDefinition(definition))
            return SetMetadataFailure(
                out_diagnostic,
                GlslModuleMetadataValidationError::InvalidDefinition,
                definition.name);

        if (!HasValidArray(
                definition.semantic_requirements,
                definition.semantic_requirement_count)
         || !HasValidArray(
                definition.semantic_provides,
                definition.semantic_provide_count)
         || !HasValidArray(definition.dependencies, definition.dependency_count)
         || !HasValidArray(
                definition.texture_reference_requirements,
                definition.texture_reference_requirement_count)
         || !HasValidArray(
                definition.sampler_reference_requirements,
                definition.sampler_reference_requirement_count))
        {
            return SetMetadataFailure(
                out_diagnostic,
                GlslModuleMetadataValidationError::InvalidArray,
                definition.name);
        }

        for (uint32 i = 0;
             i < definition.semantic_requirement_count;
             ++i)
        {
            const GlslModuleSemanticRequirement &requirement =
                definition.semantic_requirements[i];
            if (!IsValidRequirement(requirement))
                return SetMetadataFailure(
                    out_diagnostic,
                    GlslModuleMetadataValidationError::InvalidRequirement,
                    definition.name,
                    nullptr,
                    requirement.semantic);

            for (uint32 j = 0; j < i; ++j)
            {
                const GlslModuleSemanticRequirement &other =
                    definition.semantic_requirements[j];
                if (requirement.source == other.source
                 && requirement.semantic == other.semantic)
                {
                    return SetMetadataFailure(
                        out_diagnostic,
                        GlslModuleMetadataValidationError::
                            DuplicateRequirement,
                        definition.name,
                        nullptr,
                        requirement.semantic);
                }
            }
        }

        for (uint32 i = 0; i < definition.semantic_provide_count; ++i)
        {
            const GlslModuleSemantic semantic =
                definition.semantic_provides[i];
            if (semantic == GlslModuleSemantic::Unknown)
                return SetMetadataFailure(
                    out_diagnostic,
                    GlslModuleMetadataValidationError::InvalidRequirement,
                    definition.name);

            for (uint32 j = 0; j < i; ++j)
            {
                if (semantic == definition.semantic_provides[j])
                    return SetMetadataFailure(
                        out_diagnostic,
                        GlslModuleMetadataValidationError::DuplicateProvide,
                        definition.name,
                        nullptr,
                        semantic);
            }
        }

        const uint32 dependency_count =
            definition.dependency_count;
        for (uint32 i = 0; i < dependency_count; ++i)
        {
            // 内联访问：边界由循环条件保证（i 恒 < dependency_count）
            const GlslModuleDependency &dependency =
                definition.dependencies[i];

            if (SameName(dependency.module_name, definition.name))
                return SetMetadataFailure(
                    out_diagnostic,
                    GlslModuleMetadataValidationError::SelfDependency,
                    definition.name,
                    dependency.module_name);

            for (uint32 j = 0; j < i; ++j)
            {
                if (SameName(dependency.module_name,
                             definition.dependencies[j].module_name))
                {
                    return SetMetadataFailure(
                        out_diagnostic,
                        GlslModuleMetadataValidationError::
                            DuplicateDependency,
                        definition.name,
                        dependency.module_name);
                }
            }
        }

        return true;
    }

    bool ValidateGlslModuleRegistryMetadata(
        const GlslModuleRegistry &registry,
        GlslModuleMetadataValidationDiagnostic &out_diagnostic) noexcept
    {
        out_diagnostic = {};

        // 只做单条定义的自述校验 + 同优先级 provider 歧义。
        // 缺失依赖 / 环 / 缺失冲突目标由装配期负责（见头文件注释）。
        for (int index = 0; index < registry.GetCount(); ++index)
        {
            const GlslModuleDefinition *definition =
                registry.GetModuleByIndex(index);
            if (!definition
             || !ValidateGlslModuleMetadata(
                    *definition, out_diagnostic))
                return false;
        }

        for (int left_index = 0;
             left_index < registry.GetCount();
             ++left_index)
        {
            const GlslModuleDefinition *left =
                registry.GetModuleByIndex(left_index);
            if (!left || !IsProviderCandidate(*left))
                continue;

            for (int right_index = left_index + 1;
                 right_index < registry.GetCount();
                 ++right_index)
            {
                const GlslModuleDefinition *right =
                    registry.GetModuleByIndex(right_index);
                if (!right
                 || !IsProviderCandidate(*right)
                 || left->priority != right->priority)
                    continue;

                for (uint32 left_provide = 0;
                     left_provide < left->semantic_provide_count;
                     ++left_provide)
                {
                    for (uint32 right_provide = 0;
                         right_provide < right->semantic_provide_count;
                         ++right_provide)
                    {
                        if (left->semantic_provides[left_provide]
                            == right->semantic_provides[right_provide])
                        {
                            return SetMetadataFailure(
                                out_diagnostic,
                                GlslModuleMetadataValidationError::
                                    AmbiguousProviderPriority,
                                left->name,
                                right->name,
                                left->semantic_provides[left_provide]);
                        }
                    }
                }
            }
        }

        return true;
    }
}
