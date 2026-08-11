#include <hgl/graph/glsl/GLSLCodeModuleMetadata.h>

#include <hgl/type/ValueArray.h>
#include <hgl/type/StrChar.h>

namespace hgl::graph::mtl
{
    using namespace hgl::graph::mtl;
    namespace
    {
        bool SetMetadataFailure(
            GLSLCodeModuleMetadataValidationDiagnostic &out_diagnostic,
            const GLSLCodeModuleMetadataValidationError error,
            const GLSLCodeModuleID module_id,
            const uint32 item_index = 0,
            const GLSLCodeModuleID related_module_id =
                GLSLCodeModuleID::SkyLightHeader,
            const GLSLCodeModuleSemantic semantic =
                GLSLCodeModuleSemantic::Unknown) noexcept
        {
            out_diagnostic.error = error;
            out_diagnostic.module_id = module_id;
            out_diagnostic.related_module_id = related_module_id;
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

        bool HasConditionOverlap(
            const GLSLCodeModuleDefinition &lhs,
            const GLSLCodeModuleDefinition &rhs) noexcept
        {
            for (uint32 i = 0; i < lhs.condition_count; ++i)
            {
                const GLSLCodeModuleCondition &left = lhs.conditions[i];
                for (uint32 j = 0; j < rhs.condition_count; ++j)
                {
                    const GLSLCodeModuleCondition &right = rhs.conditions[j];
                    if (left.domain != right.domain
                     || hgl::strcmp(left.key, right.key) != 0)
                        continue;

                    const bool same_value =
                        hgl::strcmp(left.value, right.value) == 0;
                    if (left.operation == GLSLCodeModuleConditionOperator::Equals
                     && right.operation == GLSLCodeModuleConditionOperator::Equals
                     && !same_value)
                        return false;

                    if (same_value
                     && left.operation != right.operation)
                        return false;
                }
            }

            return true;
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
            const GLSLCodeModuleID id,
            int &out_index) noexcept
        {
            for (int index = 0; index < registry.GetCount(); ++index)
            {
                const GLSLCodeModuleDefinition *definition =
                    registry.GetModuleByIndex(index);
                if (definition && definition->id == id)
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
                GetNormalizedGLSLCodeModuleDependencyCount(*definition);
            for (uint32 dependency_index = 0;
                 dependency_index < dependency_count;
                 ++dependency_index)
            {
                GLSLCodeModuleDependency dependency{};
                if (!GetNormalizedGLSLCodeModuleDependency(
                        *definition, dependency_index, dependency))
                    return false;

                int target_index = -1;
                if (!FindModuleIndex(registry, dependency.module_id, target_index))
                    continue;

                if (visit_state[target_index] == 1)
                {
                    return SetMetadataFailure(
                        out_diagnostic,
                        GLSLCodeModuleMetadataValidationError::DependencyCycle,
                        definition->id,
                        dependency_index,
                        dependency.module_id);
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
        case GLSLCodeModuleMetadataValidationError::UnsupportedVersion: return "UnsupportedVersion";
        case GLSLCodeModuleMetadataValidationError::InvalidArray: return "InvalidArray";
        case GLSLCodeModuleMetadataValidationError::InvalidRequirement: return "InvalidRequirement";
        case GLSLCodeModuleMetadataValidationError::DuplicateRequirement: return "DuplicateRequirement";
        case GLSLCodeModuleMetadataValidationError::DuplicateProvide: return "DuplicateProvide";
        case GLSLCodeModuleMetadataValidationError::InvalidDependencyVersion: return "InvalidDependencyVersion";
        case GLSLCodeModuleMetadataValidationError::DuplicateDependency: return "DuplicateDependency";
        case GLSLCodeModuleMetadataValidationError::SelfDependency: return "SelfDependency";
        case GLSLCodeModuleMetadataValidationError::MissingDependency: return "MissingDependency";
        case GLSLCodeModuleMetadataValidationError::DependencyVersionMismatch: return "DependencyVersionMismatch";
        case GLSLCodeModuleMetadataValidationError::InvalidCondition: return "InvalidCondition";
        case GLSLCodeModuleMetadataValidationError::DuplicateCondition: return "DuplicateCondition";
        case GLSLCodeModuleMetadataValidationError::DuplicateConflict: return "DuplicateConflict";
        case GLSLCodeModuleMetadataValidationError::SelfConflict: return "SelfConflict";
        case GLSLCodeModuleMetadataValidationError::MissingConflictTarget: return "MissingConflictTarget";
        case GLSLCodeModuleMetadataValidationError::DependencyCycle: return "DependencyCycle";
        case GLSLCodeModuleMetadataValidationError::AmbiguousProviderPriority: return "AmbiguousProviderPriority";
        }

        return "Unknown";
    }

    uint32 GetNormalizedGLSLCodeModuleDependencyCount(
        const GLSLCodeModuleDefinition &definition) noexcept
    {
        return definition.dependency_count > 0
            ? definition.dependency_count
            : definition.code_module_requirement_count;
    }

    bool GetNormalizedGLSLCodeModuleDependency(
        const GLSLCodeModuleDefinition &definition,
        const uint32 index,
        GLSLCodeModuleDependency &out_dependency) noexcept
    {
        if (definition.dependency_count > 0)
        {
            if (!definition.dependencies || index >= definition.dependency_count)
                return false;

            out_dependency = definition.dependencies[index];
            return true;
        }

        if (!definition.code_module_requirements
         || index >= definition.code_module_requirement_count)
            return false;

        out_dependency = {};
        out_dependency.module_id = definition.code_module_requirements[index];
        return true;
    }

    bool AreGLSLCodeModulesConflicting(
        const GLSLCodeModuleDefinition &lhs,
        const GLSLCodeModuleDefinition &rhs) noexcept
    {
        if ((lhs.module_conflict_count > 0 && !lhs.module_conflicts)
         || (rhs.module_conflict_count > 0 && !rhs.module_conflicts))
            return false;

        for (uint32 i = 0; i < lhs.module_conflict_count; ++i)
        {
            if (lhs.module_conflicts[i] == rhs.id)
                return true;
        }

        for (uint32 i = 0; i < rhs.module_conflict_count; ++i)
        {
            if (rhs.module_conflicts[i] == lhs.id)
                return true;
        }

        return false;
    }

    bool ValidateGLSLCodeModuleMetadata(
        const GLSLCodeModuleDefinition &definition,
        GLSLCodeModuleMetadataValidationDiagnostic &out_diagnostic) noexcept
    {
        out_diagnostic = {};
        out_diagnostic.module_id = definition.id;

        if (!IsValidGLSLCodeModuleDefinition(definition))
            return SetMetadataFailure(
                out_diagnostic,
                GLSLCodeModuleMetadataValidationError::InvalidDefinition,
                definition.id);

        if (definition.metadata_version > GLSLCodeModuleCurrentMetadataVersion)
            return SetMetadataFailure(
                out_diagnostic,
                GLSLCodeModuleMetadataValidationError::UnsupportedVersion,
                definition.id);

        if (!HasValidArray(
                definition.semantic_requirements,
                definition.semantic_requirement_count)
         || !HasValidArray(
                definition.semantic_provides,
                definition.semantic_provide_count)
         || !HasValidArray(
                definition.code_module_requirements,
                definition.code_module_requirement_count)
         || !HasValidArray(definition.dependencies, definition.dependency_count)
         || !HasValidArray(definition.conditions, definition.condition_count)
         || !HasValidArray(
                definition.module_conflicts,
                definition.module_conflict_count)
         || !HasValidArray(
                definition.ubo_requirements,
                definition.ubo_requirement_count)
         || !HasValidArray(
                definition.ssbo_requirements,
                definition.ssbo_requirement_count)
         || !HasValidArray(
                definition.texture_requirements,
                definition.texture_requirement_count)
         || !HasValidArray(
                definition.texture_layer_requirements,
                definition.texture_layer_requirement_count))
        {
            return SetMetadataFailure(
                out_diagnostic,
                GLSLCodeModuleMetadataValidationError::InvalidArray,
                definition.id);
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
                    definition.id,
                    i,
                    definition.id,
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
                        definition.id,
                        i,
                        definition.id,
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
                    definition.id,
                    i);

            for (uint32 j = 0; j < i; ++j)
            {
                if (semantic == definition.semantic_provides[j])
                    return SetMetadataFailure(
                        out_diagnostic,
                        GLSLCodeModuleMetadataValidationError::DuplicateProvide,
                        definition.id,
                        i,
                        definition.id,
                        semantic);
            }
        }

        const uint32 dependency_count =
            GetNormalizedGLSLCodeModuleDependencyCount(definition);
        for (uint32 i = 0; i < dependency_count; ++i)
        {
            GLSLCodeModuleDependency dependency{};
            if (!GetNormalizedGLSLCodeModuleDependency(
                    definition, i, dependency)
             || dependency.min_metadata_version
                    > dependency.max_metadata_version
             || dependency.max_metadata_version
                    > GLSLCodeModuleCurrentMetadataVersion)
            {
                return SetMetadataFailure(
                    out_diagnostic,
                    GLSLCodeModuleMetadataValidationError::
                        InvalidDependencyVersion,
                    definition.id,
                    i);
            }

            if (dependency.module_id == definition.id)
                return SetMetadataFailure(
                    out_diagnostic,
                    GLSLCodeModuleMetadataValidationError::SelfDependency,
                    definition.id,
                    i,
                    dependency.module_id);

            for (uint32 j = 0; j < i; ++j)
            {
                GLSLCodeModuleDependency other{};
                if (GetNormalizedGLSLCodeModuleDependency(
                        definition, j, other)
                 && dependency.module_id == other.module_id)
                {
                    return SetMetadataFailure(
                        out_diagnostic,
                        GLSLCodeModuleMetadataValidationError::
                            DuplicateDependency,
                        definition.id,
                        i,
                        dependency.module_id);
                }
            }
        }

        for (uint32 i = 0; i < definition.condition_count; ++i)
        {
            const GLSLCodeModuleCondition &condition = definition.conditions[i];
            if (condition.domain < GLSLCodeModuleConditionDomain::Option
             || condition.domain
                    > GLSLCodeModuleConditionDomain::DeviceFeature
             || condition.operation
                    < GLSLCodeModuleConditionOperator::Equals
             || condition.operation
                    > GLSLCodeModuleConditionOperator::NotEquals
             || !condition.key
             || !condition.key[0]
             || !condition.value
             || !condition.value[0])
            {
                return SetMetadataFailure(
                    out_diagnostic,
                    GLSLCodeModuleMetadataValidationError::InvalidCondition,
                    definition.id,
                    i);
            }

            for (uint32 j = 0; j < i; ++j)
            {
                if (condition == definition.conditions[j])
                    return SetMetadataFailure(
                        out_diagnostic,
                        GLSLCodeModuleMetadataValidationError::
                            DuplicateCondition,
                        definition.id,
                        i);
            }
        }

        for (uint32 i = 0; i < definition.module_conflict_count; ++i)
        {
            const GLSLCodeModuleID conflict = definition.module_conflicts[i];
            if (conflict == definition.id)
                return SetMetadataFailure(
                    out_diagnostic,
                    GLSLCodeModuleMetadataValidationError::SelfConflict,
                    definition.id,
                    i,
                    conflict);

            for (uint32 j = 0; j < i; ++j)
            {
                if (conflict == definition.module_conflicts[j])
                    return SetMetadataFailure(
                        out_diagnostic,
                        GLSLCodeModuleMetadataValidationError::
                            DuplicateConflict,
                        definition.id,
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
                GetNormalizedGLSLCodeModuleDependencyCount(*definition);
            for (uint32 i = 0; i < dependency_count; ++i)
            {
                GLSLCodeModuleDependency dependency{};
                if (!GetNormalizedGLSLCodeModuleDependency(
                        *definition, i, dependency))
                    return false;

                const GLSLCodeModuleDefinition *target =
                    registry.Find(dependency.module_id);
                if (!target)
                    return SetMetadataFailure(
                        out_diagnostic,
                        GLSLCodeModuleMetadataValidationError::MissingDependency,
                        definition->id,
                        i,
                        dependency.module_id);

                if (target->metadata_version < dependency.min_metadata_version
                 || target->metadata_version > dependency.max_metadata_version)
                {
                    return SetMetadataFailure(
                        out_diagnostic,
                        GLSLCodeModuleMetadataValidationError::
                            DependencyVersionMismatch,
                        definition->id,
                        i,
                        dependency.module_id);
                }
            }

            for (uint32 i = 0; i < definition->module_conflict_count; ++i)
            {
                if (!registry.Find(definition->module_conflicts[i]))
                    return SetMetadataFailure(
                        out_diagnostic,
                        GLSLCodeModuleMetadataValidationError::
                            MissingConflictTarget,
                        definition->id,
                        i,
                        definition->module_conflicts[i]);
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
                 || left->priority != right->priority
                 || !HasConditionOverlap(*left, *right))
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
                                left->id,
                                left_provide,
                                right->id,
                                left->semantic_provides[left_provide]);
                        }
                    }
                }
            }
        }

        return true;
    }
}
