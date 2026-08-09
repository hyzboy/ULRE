#include <hgl/shadergen/ResolvedModuleGraphBuilder.h>

#include <hgl/graph/glsl/GLSLCodeModuleMetadata.h>
#include <hgl/util/hash/FNV1a.h>
#include <cstring>

namespace hgl::graph::mtl
{
    namespace
    {
        struct ModuleGraphBuildState
        {
            const GLSLCodeModuleRegistry &registry;
            ResolvedModuleGraph &graph;
            ResolvedModuleGraphBuildDiagnostic &diagnostic;
            ValueArray<ShaderContractStableID> visiting;
            ValueArray<ShaderContractStableID> visited;
            ValueArray<const GLSLCodeModuleDefinition *> selected_definitions;
        };

        struct ModuleDependencySelection
        {
            const GLSLCodeModuleDefinition *target = nullptr;
            GLSLCodeModuleDependency dependency{};
        };

        bool operator==(
            const ModuleDependencySelection &lhs,
            const ModuleDependencySelection &rhs) noexcept
        {
            return lhs.target == rhs.target
                && lhs.dependency == rhs.dependency;
        }

        uint64 AppendString(
            uint64 hash,
            const char *value) noexcept
        {
            const uint32 length = value
                ? static_cast<uint32>(std::strlen(value)) : 0;
            hash = hgl::hash::FNV1aAppendValueBytes(hash, length);
            return length > 0
                ? hgl::hash::FNV1aAppendBytes(hash, value, length)
                : hash;
        }

        bool SetGraphFailure(
            ResolvedModuleGraphBuildDiagnostic &diagnostic,
            const ResolvedModuleGraphBuildError error,
            const char *module_name,
            const char *related_module_name = nullptr,
            const GLSLCodeModuleSemantic semantic =
                GLSLCodeModuleSemantic::Unknown)
        {
            diagnostic.error = error;
            diagnostic.module_name =
                module_name ? module_name : "";
            diagnostic.related_module_name =
                related_module_name ? related_module_name : "";
            diagnostic.semantic = semantic;
            return false;
        }

        bool ContainsID(
            const ValueArray<ShaderContractStableID> &ids,
            const ShaderContractStableID id) noexcept
        {
            for (int i = 0; i < ids.GetCount(); ++i)
            {
                if (ids[i] == id)
                    return true;
            }
            return false;
        }

        AnsiString GetModuleNameFromPath(const char *path)
        {
            if (!path || !path[0])
                return {};

            const char *name = path;
            const char *dot = nullptr;
            for (const char *cursor = path; *cursor; ++cursor)
            {
                if (*cursor == '/' || *cursor == '\\')
                {
                    name = cursor + 1;
                    dot = nullptr;
                }
                else if (*cursor == '.' && !dot)
                {
                    dot = cursor;
                }
            }

            return dot
                ? AnsiString(name, static_cast<int>(dot - name))
                : AnsiString(name);
        }

        const GLSLCodeModuleDefinition *FindModuleByPath(
            const GLSLCodeModuleRegistry &registry,
            const char *path)
        {
            const AnsiString name = GetModuleNameFromPath(path);
            return name.IsEmpty() ? nullptr : registry.FindByName(name.c_str());
        }

        void SortModulePointers(
            ValueArray<const GLSLCodeModuleDefinition *> &modules)
        {
            for (int i = 1; i < modules.GetCount(); ++i)
            {
                const GLSLCodeModuleDefinition *value = modules[i];
                const ShaderContractStableID value_id =
                    GetGLSLCodeModuleStableID(*value);
                int insert_at = i;
                while (insert_at > 0
                    && value_id < GetGLSLCodeModuleStableID(
                        *modules[insert_at - 1]))
                {
                    modules[insert_at] = modules[insert_at - 1];
                    --insert_at;
                }
                modules[insert_at] = value;
            }
        }

        void SortDependencies(
            ValueArray<ModuleDependencySelection> &dependencies)
        {
            for (int i = 1; i < dependencies.GetCount(); ++i)
            {
                const ModuleDependencySelection value = dependencies[i];
                const ShaderContractStableID value_id =
                    GetGLSLCodeModuleStableID(*value.target);
                int insert_at = i;
                while (insert_at > 0
                    && value_id < GetGLSLCodeModuleStableID(
                        *dependencies[insert_at - 1].target))
                {
                    dependencies[insert_at] = dependencies[insert_at - 1];
                    --insert_at;
                }
                dependencies[insert_at] = value;
            }
        }

        bool AddRoot(
            ValueArray<const GLSLCodeModuleDefinition *> &roots,
            const GLSLCodeModuleDefinition *definition,
            ResolvedModuleGraphBuildDiagnostic &diagnostic,
            const char *requested_name)
        {
            if (!definition)
                return SetGraphFailure(
                    diagnostic,
                    ResolvedModuleGraphBuildError::MissingRootModule,
                    requested_name);

            const ShaderContractStableID stable_id =
                GetGLSLCodeModuleStableID(*definition);
            for (int i = 0; i < roots.GetCount(); ++i)
            {
                const ShaderContractStableID existing_id =
                    GetGLSLCodeModuleStableID(*roots[i]);
                if (existing_id != stable_id)
                    continue;

                if (std::strcmp(roots[i]->name, definition->name) != 0)
                    return SetGraphFailure(
                        diagnostic,
                        ResolvedModuleGraphBuildError::StableIDConflict,
                        roots[i]->name,
                        definition->name);
                return true;
            }

            roots.Add(definition);
            return true;
        }

        uint64 GetResolvedConditionHash(
            const GLSLCodeModuleDefinition &definition) noexcept
        {
            if (definition.condition_count == 0)
                return 0;

            ValueArray<GLSLCodeModuleCondition> conditions(
                definition.conditions,
                static_cast<int>(definition.condition_count));
            for (int i = 1; i < conditions.GetCount(); ++i)
            {
                const GLSLCodeModuleCondition value = conditions[i];
                int insert_at = i;
                while (insert_at > 0)
                {
                    const GLSLCodeModuleCondition &other =
                        conditions[insert_at - 1];
                    int comparison =
                        static_cast<int>(value.domain)
                        - static_cast<int>(other.domain);
                    if (comparison == 0)
                        comparison = std::strcmp(value.key, other.key);
                    if (comparison == 0)
                    {
                        comparison =
                            static_cast<int>(value.operation)
                            - static_cast<int>(other.operation);
                    }
                    if (comparison == 0)
                        comparison = std::strcmp(value.value, other.value);
                    if (comparison >= 0)
                        break;

                    conditions[insert_at] = other;
                    --insert_at;
                }
                conditions[insert_at] = value;
            }

            uint64 hash = hgl::hash::FNV1aInit<uint64>();
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, static_cast<uint32>(conditions.GetCount()));
            for (int i = 0; i < conditions.GetCount(); ++i)
            {
                hash = hgl::hash::FNV1aAppendValueBytes(
                    hash, conditions[i].domain);
                hash = hgl::hash::FNV1aAppendValueBytes(
                    hash, conditions[i].operation);
                hash = AppendString(hash, conditions[i].key);
                hash = AppendString(hash, conditions[i].value);
            }
            return hash;
        }

        bool MergeRequirement(
            ResolvedModuleGraph &graph,
            const GLSLCodeModuleSemanticRequirement &incoming,
            ResolvedModuleGraphBuildDiagnostic &diagnostic,
            const char *module_name)
        {
            for (int i = 0;
                 i < graph.aggregated_semantic_requirements.GetCount();
                 ++i)
            {
                GLSLCodeModuleSemanticRequirement &existing =
                    graph.aggregated_semantic_requirements[i];
                if (existing.source != incoming.source
                 || existing.semantic != incoming.semantic)
                    continue;

                existing.numeric_class_mask &=
                    incoming.numeric_class_mask;
                if (existing.numeric_class_mask == 0)
                    return SetGraphFailure(
                        diagnostic,
                        ResolvedModuleGraphBuildError::RequirementConflict,
                        module_name,
                        nullptr,
                        incoming.semantic);

                if (incoming.min_component_count
                    > existing.min_component_count)
                {
                    existing.min_component_count =
                        incoming.min_component_count;
                }

                if (existing.max_component_count == 0)
                {
                    existing.max_component_count =
                        incoming.max_component_count;
                }
                else if (incoming.max_component_count > 0
                      && incoming.max_component_count
                            < existing.max_component_count)
                {
                    existing.max_component_count =
                        incoming.max_component_count;
                }

                if (existing.max_component_count > 0
                 && existing.min_component_count
                        > existing.max_component_count)
                {
                    return SetGraphFailure(
                        diagnostic,
                        ResolvedModuleGraphBuildError::RequirementConflict,
                        module_name,
                        nullptr,
                        incoming.semantic);
                }
                return true;
            }

            graph.aggregated_semantic_requirements.Add(incoming);
            return true;
        }

        bool ResolveModule(
            ModuleGraphBuildState &state,
            const GLSLCodeModuleDefinition &definition)
        {
            const ShaderContractStableID stable_id =
                GetGLSLCodeModuleStableID(definition);
            if (ContainsID(state.visited, stable_id))
                return true;
            if (ContainsID(state.visiting, stable_id))
                return SetGraphFailure(
                    state.diagnostic,
                    ResolvedModuleGraphBuildError::DependencyCycle,
                    definition.name);

            for (int i = 0;
                 i < state.selected_definitions.GetCount();
                 ++i)
            {
                const GLSLCodeModuleDefinition *selected =
                    state.selected_definitions[i];
                if (AreGLSLCodeModulesConflicting(definition, *selected))
                {
                    return SetGraphFailure(
                        state.diagnostic,
                        ResolvedModuleGraphBuildError::ModuleConflict,
                        definition.name,
                        selected->name);
                }
            }

            state.visiting.Add(stable_id);

            ValueArray<ModuleDependencySelection> dependencies;
            const uint32 dependency_count =
                GetNormalizedGLSLCodeModuleDependencyCount(definition);
            for (uint32 i = 0; i < dependency_count; ++i)
            {
                GLSLCodeModuleDependency dependency{};
                if (!GetNormalizedGLSLCodeModuleDependency(
                        definition, i, dependency))
                {
                    return SetGraphFailure(
                        state.diagnostic,
                        ResolvedModuleGraphBuildError::MissingDependency,
                        definition.name);
                }

                const GLSLCodeModuleDefinition *target =
                    state.registry.Find(dependency.module_id);
                if (!target)
                {
                    return SetGraphFailure(
                        state.diagnostic,
                        ResolvedModuleGraphBuildError::MissingDependency,
                        definition.name,
                        GetGLSLCodeModuleName(dependency.module_id));
                }
                dependencies.Add({target, dependency});
            }
            SortDependencies(dependencies);

            for (int i = 0; i < dependencies.GetCount(); ++i)
            {
                if (!ResolveModule(state, *dependencies[i].target))
                    return false;

                state.graph.dependencies.Add(
                    {
                        stable_id,
                        GetGLSLCodeModuleStableID(*dependencies[i].target),
                        dependencies[i].dependency.min_metadata_version,
                        dependencies[i].dependency.max_metadata_version
                    });
            }

            for (int i = 0;
                 i < state.selected_definitions.GetCount();
                 ++i)
            {
                const GLSLCodeModuleDefinition *selected =
                    state.selected_definitions[i];
                if (AreGLSLCodeModulesConflicting(definition, *selected))
                {
                    return SetGraphFailure(
                        state.diagnostic,
                        ResolvedModuleGraphBuildError::ModuleConflict,
                        definition.name,
                        selected->name);
                }
            }

            for (uint32 i = 0;
                 i < definition.semantic_requirement_count;
                 ++i)
            {
                if (!MergeRequirement(
                        state.graph,
                        definition.semantic_requirements[i],
                        state.diagnostic,
                        definition.name))
                    return false;
            }

            ResolvedModuleContractEntry module{};
            module.module_id = stable_id;
            module.module_content_hash =
                GetCanonicalGLSLCodeModuleContentHash(
                    definition, state.registry);
            module.resolved_condition_hash =
                GetResolvedConditionHash(definition);
            module.topological_order =
                static_cast<uint32>(state.graph.modules.GetCount());
            module.flags = definition.flags;
            if (module.module_content_hash == 0)
                return SetGraphFailure(
                    state.diagnostic,
                    ResolvedModuleGraphBuildError::StableIDConflict,
                    definition.name);

            state.graph.modules.Add(module);
            state.selected_definitions.Add(&definition);
            state.visiting.Delete(state.visiting.GetCount() - 1);
            state.visited.Add(stable_id);
            return true;
        }
    }

    const char *GetResolvedModuleGraphBuildErrorName(
        const ResolvedModuleGraphBuildError error) noexcept
    {
        switch (error)
        {
        case ResolvedModuleGraphBuildError::None: return "None";
        case ResolvedModuleGraphBuildError::MissingRootModule: return "MissingRootModule";
        case ResolvedModuleGraphBuildError::MissingDependency: return "MissingDependency";
        case ResolvedModuleGraphBuildError::DependencyCycle: return "DependencyCycle";
        case ResolvedModuleGraphBuildError::ModuleConflict: return "ModuleConflict";
        case ResolvedModuleGraphBuildError::RequirementConflict: return "RequirementConflict";
        case ResolvedModuleGraphBuildError::StableIDConflict: return "StableIDConflict";
        case ResolvedModuleGraphBuildError::InvalidCanonicalGraph: return "InvalidCanonicalGraph";
        }
        return "Unknown";
    }

    ShaderContractStableID GetGLSLCodeModuleStableID(
        const GLSLCodeModuleDefinition &definition) noexcept
    {
        return definition.name
            ? hgl::hash::FNV1aAppendBytes(
                hgl::hash::FNV1aInit<uint64>(),
                definition.name,
                std::strlen(definition.name))
            : 0;
    }

    uint64 GetCanonicalGLSLCodeModuleContentHash(
        const GLSLCodeModuleDefinition &definition,
        const GLSLCodeModuleRegistry &registry) noexcept
    {
        uint64 hash = hgl::hash::FNV1aInit<uint64>();
        hash = AppendString(hash, definition.name);
        hash = AppendString(hash, definition.glsl_code);
        hash = hgl::hash::FNV1aAppendValueBytes(hash, definition.kind);
        hash = hgl::hash::FNV1aAppendValueBytes(hash, definition.priority);
        hash = hgl::hash::FNV1aAppendValueBytes(hash, definition.flags);
        hash = hgl::hash::FNV1aAppendValueBytes(
            hash, definition.metadata_version);

        hash = hgl::hash::FNV1aAppendValueBytes(
            hash, definition.ubo_requirement_count);
        for (uint32 i = 0; i < definition.ubo_requirement_count; ++i)
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, definition.ubo_requirements[i]);

        hash = hgl::hash::FNV1aAppendValueBytes(
            hash, definition.ssbo_requirement_count);
        for (uint32 i = 0; i < definition.ssbo_requirement_count; ++i)
        {
            const GLSLCodeModuleSSBORequirement &requirement =
                definition.ssbo_requirements[i];
            hash = AppendString(hash, requirement.name);
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, requirement.ssbo_type);
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, requirement.data_slot);
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, requirement.stage_flags);
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, requirement.required);
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, requirement.allow_fallback);
        }

        hash = hgl::hash::FNV1aAppendValueBytes(
            hash, definition.texture_requirement_count);
        for (uint32 i = 0; i < definition.texture_requirement_count; ++i)
        {
            const GLSLCodeModuleTextureRequirement &requirement =
                definition.texture_requirements[i];
            hash = AppendString(hash, requirement.name);
            hash = AppendString(hash, requirement.glsl_type);
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, requirement.semantic);
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, requirement.slot);
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, requirement.stage_flags);
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, requirement.required);
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, requirement.allow_fallback);
        }

        hash = hgl::hash::FNV1aAppendValueBytes(
            hash, definition.texture_layer_requirement_count);
        for (uint32 i = 0;
             i < definition.texture_layer_requirement_count;
             ++i)
        {
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, definition.texture_layer_requirements[i]);
        }

        hash = hgl::hash::FNV1aAppendValueBytes(
            hash, definition.semantic_requirement_count);
        for (uint32 i = 0;
             i < definition.semantic_requirement_count;
             ++i)
        {
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, definition.semantic_requirements[i]);
        }

        hash = hgl::hash::FNV1aAppendValueBytes(
            hash, definition.semantic_provide_count);
        for (uint32 i = 0;
             i < definition.semantic_provide_count;
             ++i)
        {
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, definition.semantic_provides[i]);
        }

        ValueArray<ModuleDependencySelection> dependencies;
        const uint32 dependency_count =
            GetNormalizedGLSLCodeModuleDependencyCount(definition);
        for (uint32 i = 0; i < dependency_count; ++i)
        {
            GLSLCodeModuleDependency dependency{};
            if (!GetNormalizedGLSLCodeModuleDependency(
                    definition, i, dependency))
                return 0;
            const GLSLCodeModuleDefinition *target =
                registry.Find(dependency.module_id);
            if (!target)
                return 0;
            dependencies.Add({target, dependency});
        }
        SortDependencies(dependencies);
        hash = hgl::hash::FNV1aAppendValueBytes(
            hash, static_cast<uint32>(dependencies.GetCount()));
        for (int i = 0; i < dependencies.GetCount(); ++i)
        {
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, GetGLSLCodeModuleStableID(*dependencies[i].target));
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash,
                dependencies[i].dependency.min_metadata_version);
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash,
                dependencies[i].dependency.max_metadata_version);
        }

        hash = hgl::hash::FNV1aAppendValueBytes(
            hash, GetResolvedConditionHash(definition));

        ValueArray<ShaderContractStableID> conflict_ids;
        for (uint32 i = 0; i < definition.module_conflict_count; ++i)
        {
            const GLSLCodeModuleDefinition *target =
                registry.Find(definition.module_conflicts[i]);
            if (!target)
                return 0;
            conflict_ids.Add(GetGLSLCodeModuleStableID(*target));
        }
        for (int i = 1; i < conflict_ids.GetCount(); ++i)
        {
            const ShaderContractStableID value = conflict_ids[i];
            int insert_at = i;
            while (insert_at > 0 && value < conflict_ids[insert_at - 1])
            {
                conflict_ids[insert_at] = conflict_ids[insert_at - 1];
                --insert_at;
            }
            conflict_ids[insert_at] = value;
        }
        hash = hgl::hash::FNV1aAppendValueBytes(
            hash, static_cast<uint32>(conflict_ids.GetCount()));
        for (int i = 0; i < conflict_ids.GetCount(); ++i)
            hash = hgl::hash::FNV1aAppendValueBytes(
                hash, conflict_ids[i]);
        return hash;
    }

    bool BuildMaterialResolvedModuleGraph(
        const MaterialDefinition &definition,
        const GLSLCodeModuleRegistry &registry,
        ResolvedModuleGraph &out_graph,
        ResolvedModuleGraphBuildDiagnostic &out_diagnostic)
    {
        out_graph = {};
        out_diagnostic = {};

        for (int i = 0; i < registry.GetCount(); ++i)
        {
            const GLSLCodeModuleDefinition *left =
                registry.GetModuleByIndex(i);
            if (!left || !left->name)
                continue;

            const ShaderContractStableID left_id =
                GetGLSLCodeModuleStableID(*left);
            for (int j = 0; j < i; ++j)
            {
                const GLSLCodeModuleDefinition *right =
                    registry.GetModuleByIndex(j);
                if (right
                 && right->name
                 && left_id == GetGLSLCodeModuleStableID(*right)
                 && std::strcmp(left->name, right->name) != 0)
                {
                    return SetGraphFailure(
                        out_diagnostic,
                        ResolvedModuleGraphBuildError::StableIDConflict,
                        left->name,
                        right->name);
                }
            }
        }

        ValueArray<const GLSLCodeModuleDefinition *> roots;
        for (const GLSLCodeModuleID id : definition.code_module_requirements)
        {
            const GLSLCodeModuleDefinition *root = registry.Find(id);
            if (!AddRoot(
                    roots,
                    root,
                    out_diagnostic,
                    GetGLSLCodeModuleName(id)))
                return false;
        }

        const char *module_paths[] =
        {
            definition.fragment_source,
            definition.fragment_surface_module,
            definition.fragment_material_source_module,
            definition.fragment_ntb_module
        };
        for (const char *path : module_paths)
        {
            if (!path || !path[0])
                continue;
            const AnsiString name = GetModuleNameFromPath(path);
            if (!AddRoot(
                    roots,
                    FindModuleByPath(registry, path),
                    out_diagnostic,
                    name.c_str()))
                return false;
        }

        const MaterialTransformGraph transform =
            definition.has_transform_graph
                ? definition.transform_graph
                : MaterialTransformGraph::FromNodeConfig(
                    definition.vertex_node_config);
        const char *vertex_paths[] =
        {
            transform.source == VertexInputMode::Procedural
                ? "vertex/s1_input_procedural.glsl" : nullptr,
            transform.GetMappingModulePath(),
            transform.GetStage3ModulePath()
        };
        for (const char *path : vertex_paths)
        {
            if (!path)
                continue;
            const AnsiString name = GetModuleNameFromPath(path);
            if (!AddRoot(
                    roots,
                    FindModuleByPath(registry, path),
                    out_diagnostic,
                    name.c_str()))
                return false;
        }
        SortModulePointers(roots);

        ModuleGraphBuildState state{
            registry,
            out_graph,
            out_diagnostic
        };
        for (int i = 0; i < roots.GetCount(); ++i)
        {
            if (!ResolveModule(state, *roots[i]))
                return false;
        }

        if (!ValidateResolvedModuleGraph(out_graph))
            return SetGraphFailure(
                out_diagnostic,
                ResolvedModuleGraphBuildError::InvalidCanonicalGraph,
                definition.definition_id.c_str());

        return true;
    }
}
