#include <hgl/graph/glsl/GLSLCodeModuleCapabilityResolver.h>

#include <hgl/graph/geo/GeometryVertexFormat.h>
#include <hgl/vk/VKFormat.h>

#include <cstring>

namespace hgl::graph::mtl
{
    namespace
    {
        constexpr int kSemanticTableSize = 64;

        inline int GetSemanticIndex(const GLSLCodeModuleSemantic semantic) noexcept
        {
            const int index = static_cast<int>(semantic);
            return (index >= 0 && index < kSemanticTableSize) ? index : -1;
        }

        inline bool ProvidesSemantic(const GLSLCodeModuleDefinition *definition,
                                     const GLSLCodeModuleSemantic semantic) noexcept
        {
            if (!definition || definition->semantic_provide_count == 0)
                return false;

            for (uint32 i = 0; i < definition->semantic_provide_count; ++i)
            {
                if (definition->semantic_provides[i] == semantic)
                    return true;
            }

            return false;
        }

        inline bool IsProviderCandidate(const GLSLCodeModuleDefinition *definition) noexcept
        {
            if (!definition || definition->semantic_provide_count == 0)
                return false;

            // Terminal stages are never used as providers.
            return definition->kind != GLSLCodeModuleKind::Surface
                && definition->kind != GLSLCodeModuleKind::FragmentShader;
        }

        bool ContainsSemantic(const GLSLCodeModuleSemantic *list, const uint32 count,
                              const GLSLCodeModuleSemantic semantic) noexcept
        {
            for (uint32 i = 0; i < count; ++i)
            {
                if (list[i] == semantic)
                    return true;
            }

            return false;
        }

        struct ResolverState
        {
            const GLSLCodeModuleRegistry &registry;
            const GLSLCodeModuleResolutionRequest &request;
            GLSLCodeModuleResolutionResult &result;

            bool provided[kSemanticTableSize] = {};
            ValueArray<GLSLCodeModuleID> selected_ids;

            bool IsSelected(const GLSLCodeModuleID id) const
            {
                for (int i = 0; i < selected_ids.GetCount(); ++i)
                {
                    if (selected_ids[i] == id)
                        return true;
                }

                return false;
            }
        };

        bool CandidateFeasible(const ResolverState &state,
                               const GLSLCodeModuleDefinition *candidate,
                               const bool *in_progress,
                               const char **out_reason);

        // Returns true when `semantic` is already provided or at least one
        // unselected provider can still produce it without a dependency cycle.
        bool CheckCanProvide(const ResolverState &state,
                             const GLSLCodeModuleSemantic semantic,
                             const bool *in_progress,
                             const char **out_reason)
        {
            if (semantic == GLSLCodeModuleSemantic::Unknown)
            {
                if (out_reason) *out_reason = "unknown semantic";
                return false;
            }

            const int index = GetSemanticIndex(semantic);
            if (index < 0)
            {
                if (out_reason) *out_reason = "semantic out of range";
                return false;
            }

            if (state.provided[index])
                return true;

            if (in_progress[index])
            {
                if (out_reason) *out_reason = "circular dependency";
                return false;
            }

            bool next_in_progress[kSemanticTableSize];
            std::memcpy(next_in_progress, in_progress, sizeof(next_in_progress));
            next_in_progress[index] = true;

            for (int i = 0; i < state.registry.GetCount(); ++i)
            {
                const auto *candidate = state.registry.GetModuleByIndex(i);
                if (!IsProviderCandidate(candidate) || !ProvidesSemantic(candidate, semantic))
                    continue;
                if (state.IsSelected(candidate->id))
                    continue;

                if (CandidateFeasible(state, candidate, next_in_progress, nullptr))
                    return true;
            }

            if (out_reason) *out_reason = "no feasible provider";
            return false;
        }

        bool CandidateFeasible(const ResolverState &state,
                               const GLSLCodeModuleDefinition *candidate,
                               const bool *in_progress,
                               const char **out_reason)
        {
            for (uint32 i = 0; i < candidate->semantic_requirement_count; ++i)
            {
                const auto &requirement = candidate->semantic_requirements[i];
                const char *detail = nullptr;
                bool satisfied = false;

                switch (requirement.source)
                {
                case GLSLCodeModuleCapabilitySource::GeometryAttribute:
                    for (uint32 k = 0; k < state.request.geometry_capability_count; ++k)
                    {
                        if (GLSLCodeModuleCapabilityResolver::MatchGeometryCapability(
                                requirement, state.request.geometry_capabilities[k]))
                        {
                            satisfied = true;
                            break;
                        }
                    }
                    detail = "geometry attribute mismatch";
                    break;

                case GLSLCodeModuleCapabilitySource::Resource:
                    satisfied = ContainsSemantic(state.request.resources,
                                                 state.request.resource_count,
                                                 requirement.semantic);
                    detail = "resource unavailable";
                    break;

                case GLSLCodeModuleCapabilitySource::Option:
                    satisfied = ContainsSemantic(state.request.options,
                                                 state.request.option_count,
                                                 requirement.semantic);
                    detail = "option not enabled";
                    break;

                case GLSLCodeModuleCapabilitySource::ProducedSemantic:
                    satisfied = CheckCanProvide(state, requirement.semantic, in_progress, &detail);
                    break;

                default:
                    detail = "unsupported requirement source";
                    break;
                }

                if (!satisfied)
                {
                    if (out_reason) *out_reason = detail ? detail : "requirement not satisfied";
                    return false;
                }
            }

            return true;
        }

        void CollectCandidates(const GLSLCodeModuleRegistry &registry,
                               const GLSLCodeModuleSemantic semantic,
                               ValueArray<const GLSLCodeModuleDefinition *> &out)
        {
            out.Clear();

            for (int i = 0; i < registry.GetCount(); ++i)
            {
                const auto *candidate = registry.GetModuleByIndex(i);
                if (IsProviderCandidate(candidate) && ProvidesSemantic(candidate, semantic))
                    out.Add(candidate);
            }
        }

        bool HasHigherPriority(const GLSLCodeModuleDefinition *lhs,
                               const GLSLCodeModuleDefinition *rhs)
        {
            if (lhs->priority != rhs->priority)
                return lhs->priority > rhs->priority;

            return static_cast<uint32>(lhs->id) < static_cast<uint32>(rhs->id);
        }

        void SortCandidates(ValueArray<const GLSLCodeModuleDefinition *> &candidates)
        {
            const int count = candidates.GetCount();

            for (int i = 0; i < count; ++i)
            {
                int best = i;
                for (int k = i + 1; k < count; ++k)
                {
                    if (HasHigherPriority(candidates[k], candidates[best]))
                        best = k;
                }

                if (best != i)
                {
                    const auto *tmp = candidates[i];
                    candidates[i] = candidates[best];
                    candidates[best] = tmp;
                }
            }
        }

        // Recursively select the best feasible provider for one produced
        // semantic. Dependencies are resolved and committed first so the
        // selection list stays in dependency order.
        bool ResolveSemantic(ResolverState &state,
                             const GLSLCodeModuleSemantic semantic,
                             const bool *parent_in_progress)
        {
            const int index = GetSemanticIndex(semantic);
            if (index < 0 || semantic == GLSLCodeModuleSemantic::Unknown)
                return false;

            if (state.provided[index])
                return true;

            if (parent_in_progress[index])
                return false;

            ValueArray<const GLSLCodeModuleDefinition *> candidates;
            CollectCandidates(state.registry, semantic, candidates);
            SortCandidates(candidates);

            bool in_progress[kSemanticTableSize];
            std::memcpy(in_progress, parent_in_progress, sizeof(in_progress));
            in_progress[index] = true;

            for (int i = 0; i < candidates.GetCount(); ++i)
            {
                const auto *candidate = candidates[i];
                if (state.IsSelected(candidate->id))
                    continue;

                const char *reason = nullptr;
                if (!CandidateFeasible(state, candidate, in_progress, &reason))
                {
                    GLSLCodeModuleRejectDiagnostic diagnostic;
                    diagnostic.requirement = semantic;
                    diagnostic.candidate = candidate;
                    diagnostic.reason = reason ? reason : "requirement not satisfied";
                    state.result.diagnostics.Add(diagnostic);
                    continue;
                }

                // Commit produced dependencies before this provider.
                bool dependency_failed = false;

                for (uint32 k = 0; k < candidate->semantic_requirement_count; ++k)
                {
                    const auto &requirement = candidate->semantic_requirements[k];
                    if (requirement.source != GLSLCodeModuleCapabilitySource::ProducedSemantic)
                        continue;

                    const int dependency_index = GetSemanticIndex(requirement.semantic);
                    if (dependency_index < 0)
                    {
                        dependency_failed = true;
                        break;
                    }

                    if (state.provided[dependency_index])
                        continue;

                    if (!ResolveSemantic(state, requirement.semantic, in_progress))
                    {
                        dependency_failed = true;
                        break;
                    }
                }

                if (dependency_failed)
                {
                    GLSLCodeModuleRejectDiagnostic diagnostic;
                    diagnostic.requirement = semantic;
                    diagnostic.candidate = candidate;
                    diagnostic.reason = "provider dependency could not be resolved";
                    state.result.diagnostics.Add(diagnostic);
                    continue;
                }

                state.selected_ids.Add(candidate->id);

                for (uint32 k = 0; k < candidate->semantic_provide_count; ++k)
                {
                    const int provide_index = GetSemanticIndex(candidate->semantic_provides[k]);
                    if (provide_index >= 0)
                        state.provided[provide_index] = true;
                }

                GLSLCodeModuleProviderSelection selection;
                selection.requirement = semantic;
                selection.provider = candidate;
                state.result.selections.Add(selection);
                return true;
            }

            return false;
        }
    }

    uint32 GLSLCodeModuleCapabilityResolver::GetNumericClassFromVkFormat(const VkFormat format)
    {
        switch (format)
        {
        case VF_V1F:
        case VF_V2F:
        case VF_V3F:
        case VF_V4F:
        case VF_V1HF:
        case VF_V2HF:
        case VF_V3HF:
        case VF_V4HF:
            return static_cast<uint32>(GLSLCodeModuleNumericClass::Float);

        case VF_V1UN8:
        case VF_V2UN8:
        case VF_V3UN8:
        case VF_V4UN8:
        case VF_V1UN16:
        case VF_V2UN16:
        case VF_V3UN16:
        case VF_V4UN16:
        case VF_V1SN8:
        case VF_V2SN8:
        case VF_V3SN8:
        case VF_V4SN8:
        case VF_V1SN16:
        case VF_V2SN16:
        case VF_V3SN16:
        case VF_V4SN16:
            return static_cast<uint32>(GLSLCodeModuleNumericClass::Normalized);

        case VF_V1I:
        case VF_V2I:
        case VF_V3I:
        case VF_V4I:
        case VF_V1I16:
        case VF_V2I16:
        case VF_V3I16:
        case VF_V4I16:
        case VF_V1I8:
        case VF_V2I8:
        case VF_V3I8:
        case VF_V4I8:
            return static_cast<uint32>(GLSLCodeModuleNumericClass::SignedInteger);

        case VF_V1U:
        case VF_V2U:
        case VF_V3U:
        case VF_V4U:
        case VF_V1U8:
        case VF_V2U8:
        case VF_V3U8:
        case VF_V4U8:
        case VF_V1U16:
        case VF_V2U16:
        case VF_V3U16:
        case VF_V4U16:
            return static_cast<uint32>(GLSLCodeModuleNumericClass::UnsignedInteger);

        // Packed formats: the packed bit is combined with the storage class the
        // decoder must expand (e.g. A2RGB10UN is both Normalized and Packed).
        case PF_RG4UN:
        case PF_RGBA4:
        case PF_BGRA4:
        case PF_RGB565:
        case PF_BGR565:
        case PF_RGB5A1:
        case PF_BGR5A1:
        case PF_A1RGB5:
        case PF_A2RGB10UN:
        case PF_A2RGB10SN:
        case PF_A2BGR10UN:
        case PF_A2BGR10SN:
            return static_cast<uint32>(GLSLCodeModuleNumericClass::Normalized)
                 | static_cast<uint32>(GLSLCodeModuleNumericClass::Packed);

        case PF_A2RGB10U:
        case PF_A2RGB10I:
        case PF_A2BGR10U:
        case PF_A2BGR10I:
            return static_cast<uint32>(GLSLCodeModuleNumericClass::UnsignedInteger)
                 | static_cast<uint32>(GLSLCodeModuleNumericClass::Packed);

        case PF_B10GR11UF:
        case PF_E5BGR9UF:
            return static_cast<uint32>(GLSLCodeModuleNumericClass::Float)
                 | static_cast<uint32>(GLSLCodeModuleNumericClass::Packed);

        default:
            return 0;
        }
    }

    namespace
    {
        GLSLCodeModuleSemantic MapVertexSemantic(const VertexSemantic semantic) noexcept
        {
            switch (semantic)
            {
            case VertexSemantic::Position:  return GLSLCodeModuleSemantic::Position;
            case VertexSemantic::Normal:    return GLSLCodeModuleSemantic::Normal;
            case VertexSemantic::Tangent:   return GLSLCodeModuleSemantic::Tangent;
            case VertexSemantic::Bitangent: return GLSLCodeModuleSemantic::Binormal;
            case VertexSemantic::Color:     return GLSLCodeModuleSemantic::Color;
            case VertexSemantic::Luminance: return GLSLCodeModuleSemantic::Luminance;
            case VertexSemantic::TexCoord:  return GLSLCodeModuleSemantic::UV0;
            default:                        return GLSLCodeModuleSemantic::Unknown;
            }
        }
    }

    bool GLSLCodeModuleCapabilityResolver::BuildGeometryCapabilities(
        const GeometryVertexFormat &format,
        ValueArray<GLSLCodeModuleGeometryCapability> &out)
    {
        out.Clear();

        const uint32 count = format.GetCount();
        for (uint32 i = 0; i < count; ++i)
        {
            const GeometryVertexAttributeFormat *attribute = format.Get(i);
            if (!attribute)
                continue;

            const GLSLCodeModuleSemantic semantic = MapVertexSemantic(attribute->semantic);
            if (semantic == GLSLCodeModuleSemantic::Unknown)
                continue;

            const uint32 numeric_class_mask = GetNumericClassFromVkFormat(attribute->format);
            if (numeric_class_mask == 0)
                continue;

            GLSLCodeModuleGeometryCapability capability;
            capability.semantic = semantic;
            capability.numeric_class_mask = numeric_class_mask;
            capability.component_count = attribute->vec_size
                ? attribute->vec_size
                : detail::InferVecSizeFromFormat(attribute->format);
            out.Add(capability);
        }

        return out.GetCount() > 0;
    }

    bool GLSLCodeModuleCapabilityResolver::MatchGeometryCapability(
        const GLSLCodeModuleSemanticRequirement &requirement,
        const GLSLCodeModuleGeometryCapability &capability)
    {
        if (requirement.source != GLSLCodeModuleCapabilitySource::GeometryAttribute)
            return false;

        if (requirement.semantic != capability.semantic)
            return false;

        if (requirement.numeric_class_mask != static_cast<uint32>(GLSLCodeModuleNumericClass::Any)
         && (requirement.numeric_class_mask & capability.numeric_class_mask) == 0)
            return false;

        if (requirement.max_component_count != 0)
        {
            if (capability.component_count < requirement.min_component_count
             || capability.component_count > requirement.max_component_count)
                return false;
        }
        else if (requirement.min_component_count != 0
              && capability.component_count != requirement.min_component_count)
        {
            return false;
        }

        return true;
    }

    bool GLSLCodeModuleCapabilityResolver::Resolve(
        const GLSLCodeModuleRegistry &registry,
        const GLSLCodeModuleResolutionRequest &request,
        GLSLCodeModuleResolutionResult &result) const
    {
        result.selections.Clear();
        result.diagnostics.Clear();
        result.resolved = false;

        if (!request.requirements || request.requirement_count == 0)
        {
            result.resolved = true;
            return true;
        }

        ResolverState state{ registry, request, result };

        bool ok = true;

        for (uint32 i = 0; i < request.requirement_count; ++i)
        {
            const auto &requirement = request.requirements[i];

            switch (requirement.source)
            {
            case GLSLCodeModuleCapabilitySource::GeometryAttribute:
            {
                bool satisfied = false;
                for (uint32 k = 0; k < request.geometry_capability_count; ++k)
                {
                    if (MatchGeometryCapability(requirement, request.geometry_capabilities[k]))
                    {
                        satisfied = true;
                        break;
                    }
                }

                if (!satisfied)
                {
                    GLSLCodeModuleRejectDiagnostic diagnostic;
                    diagnostic.requirement = requirement.semantic;
                    diagnostic.candidate = nullptr;
                    diagnostic.reason = "surface geometry requirement not satisfied";
                    result.diagnostics.Add(diagnostic);
                    ok = false;
                }
                break;
            }

            case GLSLCodeModuleCapabilitySource::Resource:
                if (!ContainsSemantic(request.resources, request.resource_count, requirement.semantic))
                {
                    GLSLCodeModuleRejectDiagnostic diagnostic;
                    diagnostic.requirement = requirement.semantic;
                    diagnostic.candidate = nullptr;
                    diagnostic.reason = "surface resource requirement not satisfied";
                    result.diagnostics.Add(diagnostic);
                    ok = false;
                }
                break;

            case GLSLCodeModuleCapabilitySource::Option:
                if (!ContainsSemantic(request.options, request.option_count, requirement.semantic))
                {
                    GLSLCodeModuleRejectDiagnostic diagnostic;
                    diagnostic.requirement = requirement.semantic;
                    diagnostic.candidate = nullptr;
                    diagnostic.reason = "surface option requirement not satisfied";
                    result.diagnostics.Add(diagnostic);
                    ok = false;
                }
                break;

            case GLSLCodeModuleCapabilitySource::ProducedSemantic:
            {
                const bool empty_in_progress[kSemanticTableSize] = {};
                if (!ResolveSemantic(state, requirement.semantic, empty_in_progress))
                {
                    GLSLCodeModuleRejectDiagnostic diagnostic;
                    diagnostic.requirement = requirement.semantic;
                    diagnostic.candidate = nullptr;
                    diagnostic.reason = "no provider chain could satisfy produced semantic";
                    result.diagnostics.Add(diagnostic);
                    ok = false;
                }
                break;
            }

            default:
                ok = false;
                break;
            }
        }

        result.resolved = ok;
        return ok;
    }
}
