#include <hgl/mtl/GLSLCodeModuleMetadata.h>

#include <hgl/type/ValueArray.h>
#include <hgl/type/StrChar.h>

#include <cstring>

namespace hgl::graph::mtl
{
    namespace
    {
        bool SetMetadataFailure(
            GLSLCodeModuleMetadataValidationDiagnostic &out_diagnostic,
            const GLSLCodeModuleMetadataValidationError error,
            const char *module_name,
            const uint32 item_index = 0,
            const char *related_module_name = nullptr,
            const GLSLCodeModuleSemantic semantic =
                GLSLCodeModuleSemantic::Unknown) noexcept
        {
            out_diagnostic.error = error;
            out_diagnostic.module_name = module_name ? module_name : "";
            out_diagnostic.related_module_name =
                related_module_name ? related_module_name : "";
            out_diagnostic.semantic = semantic;
            out_diagnostic.item_index = item_index;
            return false;
        }

        bool HasValidArray(const void *data, const uint32 count) noexcept
        {
            return count == 0 || data != nullptr;
        }

        bool IsValidRequirement(
            const GLSLCodeModuleSemanticRequirement &requirement) noexcept
        {
            return requirement.source >=
                    GLSLCodeModuleCapabilitySource::GeometryAttribute
                && requirement.source <=
                    GLSLCodeModuleCapabilitySource::ProducedSemantic
                && requirement.semantic != GLSLCodeModuleSemantic::Unknown
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
            const GLSLCodeModuleDefinition &definition) noexcept
        {
            return definition.semantic_provide_count > 0
                && definition.kind != GLSLCodeModuleKind::Surface
                && definition.kind != GLSLCodeModuleKind::FragmentShader;
        }

        bool FindModuleIndex(
            const GLSLCodeModuleRegistry &registry,
            const char *name,
            int &out_index) noexcept
        {
            for (int index = 0; index < registry.GetCount(); ++index)
            {
                const GLSLCodeModuleDefinition *definition =
                    registry.GetModuleByIndex(index);
                if (definition && SameName(definition->name, name))
                {
                    out_index = index;
                    return true;
                }
            }

            return false;
        }

        bool ValidateDependencyVisit(
            const GLSLCodeModuleRegistry &registry,
            const int module_index,
            ValueArray<uint8> &visit_state,
            GLSLCodeModuleMetadataValidationDiagnostic &out_diagnostic) noexcept
        {
            if (visit_state[module_index] == 2)
                return true;
            if (visit_state[module_index] == 1)
                return false;

            visit_state[module_index] = 1;
            const GLSLCodeModuleDefinition *definition =
                registry.GetModuleByIndex(module_index);
            if (!definition)
                return false;

            const uint32 dependency_count =
                definition->dependency_count;
            for (uint32 dependency_index = 0;
                 dependency_index < dependency_count;
                 ++dependency_index)
            {
                GLSLCodeModuleDependency dependency =
                    definition->dependencies[dependency_index];

                int target_index = -1;
                if (!FindModuleIndex(registry, dependency.module_name, target_index))
                    continue;

                if (visit_state[target_index] == 1)
                {
                    return SetMetadataFailure(
                        out_diagnostic,
                        GLSLCodeModuleMetadataValidationError::DependencyCycle,
                        definition->name,
                        dependency_index,
                        dependency.module_name);
                }

                if (!ValidateDependencyVisit(
                        registry, target_index, visit_state, out_diagnostic))
                    return false;
            }

            visit_state[module_index] = 2;
            return true;
        }
    }

    const char *GetGLSLCodeModuleMetadataValidationErrorName(
        const GLSLCodeModuleMetadataValidationError error) noexcept
    {
        switch (error)
        {
        case GLSLCodeModuleMetadataValidationError::None: return "None";
        case GLSLCodeModuleMetadataValidationError::InvalidDefinition: return "InvalidDefinition";
        case GLSLCodeModuleMetadataValidationError::InvalidArray: return "InvalidArray";
        case GLSLCodeModuleMetadataValidationError::InvalidRequirement: return "InvalidRequirement";
        case GLSLCodeModuleMetadataValidationError::DuplicateRequirement: return "DuplicateRequirement";
        case GLSLCodeModuleMetadataValidationError::DuplicateProvide: return "DuplicateProvide";
        case GLSLCodeModuleMetadataValidationError::DuplicateDependency: return "DuplicateDependency";
        case GLSLCodeModuleMetadataValidationError::SelfDependency: return "SelfDependency";
        case GLSLCodeModuleMetadataValidationError::MissingDependency: return "MissingDependency";
        case GLSLCodeModuleMetadataValidationError::DuplicateConflict: return "DuplicateConflict";
        case GLSLCodeModuleMetadataValidationError::SelfConflict: return "SelfConflict";
        case GLSLCodeModuleMetadataValidationError::MissingConflictTarget: return "MissingConflictTarget";
        case GLSLCodeModuleMetadataValidationError::DependencyCycle: return "DependencyCycle";
        case GLSLCodeModuleMetadataValidationError::AmbiguousProviderPriority: return "AmbiguousProviderPriority";
        }

        return "Unknown";
    }

    // 直通访问器——ResolvedModuleGraphBuilder 等公共调用方经此访问依赖表
    // （Metadata.cpp 内部已直接访问字段）
    uint32 GetNormalizedGLSLCodeModuleDependencyCount(
        const GLSLCodeModuleDefinition &definition) noexcept
    {
        return definition.dependency_count;
    }

    bool GetNormalizedGLSLCodeModuleDependency(
        const GLSLCodeModuleDefinition &definition,
        const uint32 index,
        GLSLCodeModuleDependency &out_dependency) noexcept
    {
        if (!definition.dependencies || index >= definition.dependency_count)
            return false;

        out_dependency = definition.dependencies[index];
        return true;
    }

    bool AreGLSLCodeModulesConflicting(
        const GLSLCodeModuleDefinition &lhs,
        const GLSLCodeModuleDefinition &rhs) noexcept
    {
        if ((lhs.module_conflict_count > 0 && !lhs.module_conflict_names)
         || (rhs.module_conflict_count > 0 && !rhs.module_conflict_names))
            return false;

        for (uint32 i = 0; i < lhs.module_conflict_count; ++i)
        {
            if (SameName(lhs.module_conflict_names[i], rhs.name))
                return true;
        }

        for (uint32 i = 0; i < rhs.module_conflict_count; ++i)
        {
            if (SameName(rhs.module_conflict_names[i], lhs.name))
                return true;
        }

        return false;
    }

    bool ValidateGLSLCodeModuleMetadata(
        const GLSLCodeModuleDefinition &definition,
        GLSLCodeModuleMetadataValidationDiagnostic &out_diagnostic) noexcept
    {
        out_diagnostic = {};
        out_diagnostic.module_name = definition.name ? definition.name : "";

        if (!IsValidGLSLCodeModuleDefinition(definition))
            return SetMetadataFailure(
                out_diagnostic,
                GLSLCodeModuleMetadataValidationError::InvalidDefinition,
                definition.name);

        if (!HasValidArray(
                definition.semantic_requirements,
                definition.semantic_requirement_count)
         || !HasValidArray(
                definition.semantic_provides,
                definition.semantic_provide_count)
         || !HasValidArray(definition.dependencies, definition.dependency_count)
         || !HasValidArray(
                definition.module_conflict_names,
                definition.module_conflict_count)
         || !HasValidArray(
                definition.ssbo_requirements,
                definition.ssbo_requirement_count)
         || !HasValidArray(
                definition.texture_layer_requirements,
                definition.texture_layer_requirement_count))
        {
            return SetMetadataFailure(
                out_diagnostic,
                GLSLCodeModuleMetadataValidationError::InvalidArray,
                definition.name);
        }

        for (uint32 i = 0;
             i < definition.semantic_requirement_count;
             ++i)
        {
            const GLSLCodeModuleSemanticRequirement &requirement =
                definition.semantic_requirements[i];
            if (!IsValidRequirement(requirement))
                return SetMetadataFailure(
                    out_diagnostic,
                    GLSLCodeModuleMetadataValidationError::InvalidRequirement,
                    definition.name,
                    i,
                    nullptr,
                    requirement.semantic);

            for (uint32 j = 0; j < i; ++j)
            {
                const GLSLCodeModuleSemanticRequirement &other =
                    definition.semantic_requirements[j];
                if (requirement.source == other.source
                 && requirement.semantic == other.semantic)
                {
                    return SetMetadataFailure(
                        out_diagnostic,
                        GLSLCodeModuleMetadataValidationError::
                            DuplicateRequirement,
                        definition.name,
                        i,
                        nullptr,
                        requirement.semantic);
                }
            }
        }

        for (uint32 i = 0; i < definition.semantic_provide_count; ++i)
        {
            const GLSLCodeModuleSemantic semantic =
                definition.semantic_provides[i];
            if (semantic == GLSLCodeModuleSemantic::Unknown)
                return SetMetadataFailure(
                    out_diagnostic,
                    GLSLCodeModuleMetadataValidationError::InvalidRequirement,
                    definition.name,
                    i);

            for (uint32 j = 0; j < i; ++j)
            {
                if (semantic == definition.semantic_provides[j])
                    return SetMetadataFailure(
                        out_diagnostic,
                        GLSLCodeModuleMetadataValidationError::DuplicateProvide,
                        definition.name,
                        i,
                        nullptr,
                        semantic);
            }
        }

        const uint32 dependency_count =
            definition.dependency_count;
        for (uint32 i = 0; i < dependency_count; ++i)
        {
            // 内联访问（原 GetNormalizedGLSLCodeModuleDependency 的边界防御
            // 由循环条件保证——i 恒 < dependency_count）
            const GLSLCodeModuleDependency &dependency =
                definition.dependencies[i];

            if (SameName(dependency.module_name, definition.name))
                return SetMetadataFailure(
                    out_diagnostic,
                    GLSLCodeModuleMetadataValidationError::SelfDependency,
                    definition.name,
                    i,
                    dependency.module_name);

            for (uint32 j = 0; j < i; ++j)
            {
                GLSLCodeModuleDependency other{};
                if (j < definition.dependency_count
                 && SameName(dependency.module_name,
                             definition.dependencies[j].module_name))
                {
                    return SetMetadataFailure(
                        out_diagnostic,
                        GLSLCodeModuleMetadataValidationError::
                            DuplicateDependency,
                        definition.name,
                        i,
                        dependency.module_name);
                }
            }
        }

        for (uint32 i = 0; i < definition.module_conflict_count; ++i)
        {
            const char *const conflict = definition.module_conflict_names[i];
            if (SameName(conflict, definition.name))
                return SetMetadataFailure(
                    out_diagnostic,
                    GLSLCodeModuleMetadataValidationError::SelfConflict,
                    definition.name,
                    i,
                    conflict);

            for (uint32 j = 0; j < i; ++j)
            {
                if (SameName(conflict, definition.module_conflict_names[j]))
                    return SetMetadataFailure(
                        out_diagnostic,
                        GLSLCodeModuleMetadataValidationError::
                            DuplicateConflict,
                        definition.name,
                        i,
                        conflict);
            }
        }

        return true;
    }

    bool ValidateGLSLCodeModuleRegistryMetadata(
        const GLSLCodeModuleRegistry &registry,
        GLSLCodeModuleMetadataValidationDiagnostic &out_diagnostic) noexcept
    {
        out_diagnostic = {};

        for (int index = 0; index < registry.GetCount(); ++index)
        {
            const GLSLCodeModuleDefinition *definition =
                registry.GetModuleByIndex(index);
            if (!definition
             || !ValidateGLSLCodeModuleMetadata(
                    *definition, out_diagnostic))
                return false;

            const uint32 dependency_count =
                definition->dependency_count;
            for (uint32 i = 0; i < dependency_count; ++i)
            {
                GLSLCodeModuleDependency dependency =
                    definition->dependencies[i];

                const GLSLCodeModuleDefinition *target =
                    registry.FindByName(dependency.module_name);
                if (!target)
                    return SetMetadataFailure(
                        out_diagnostic,
                        GLSLCodeModuleMetadataValidationError::MissingDependency,
                        definition->name,
                        i,
                        dependency.module_name);

            }

            for (uint32 i = 0; i < definition->module_conflict_count; ++i)
            {
                if (!registry.FindByName(definition->module_conflict_names[i]))
                    return SetMetadataFailure(
                        out_diagnostic,
                        GLSLCodeModuleMetadataValidationError::
                            MissingConflictTarget,
                        definition->name,
                        i,
                        definition->module_conflict_names[i]);
            }
        }

        ValueArray<uint8> visit_state;
        visit_state.Resize(registry.GetCount());
        for (int index = 0; index < visit_state.GetCount(); ++index)
            visit_state[index] = 0;

        for (int index = 0; index < registry.GetCount(); ++index)
        {
            if (visit_state[index] == 0
             && !ValidateDependencyVisit(
                    registry, index, visit_state, out_diagnostic))
                return false;
        }

        for (int left_index = 0;
             left_index < registry.GetCount();
             ++left_index)
        {
            const GLSLCodeModuleDefinition *left =
                registry.GetModuleByIndex(left_index);
            if (!left || !IsProviderCandidate(*left))
                continue;

            for (int right_index = left_index + 1;
                 right_index < registry.GetCount();
                 ++right_index)
            {
                const GLSLCodeModuleDefinition *right =
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
                                GLSLCodeModuleMetadataValidationError::
                                    AmbiguousProviderPriority,
                                left->name,
                                left_provide,
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
