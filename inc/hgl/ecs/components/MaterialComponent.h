#pragma once

#include<hgl/ecs/core/Component.h>
#include<hgl/mtl/ShaderResourceSchema.h>
#include<hgl/mtl/MaterialRecipe.h>
#include<hgl/mtl/MaterialBindingContract.h>
#include<vector>

namespace hgl::graph
{
    class ShaderProgram;
}

namespace hgl::ecs
{
    enum class MaterialRuntimeState : uint8
    {
        Unresolved = 0,
        ProgramResolved,
        ResourcesPending,
        Ready,
        Failed
    };

    const char *GetMaterialRuntimeStateName(
        MaterialRuntimeState state) noexcept;

    class MaterialComponent : public Component
    {
    public:

        struct ResolvedSSBOBinding
        {
            const char *data_slot_name = nullptr;
            uint32_t data_slot = graph::mtl::DefaultMaterialDataSlot;
            graph::mtl::SSBOType ssbo_type = graph::mtl::SSBOType::UserDefined;
            uint32_t ssbo_id = 0;
            bool valid = false;
        };

        // Runtime shared program, resolved by ECS.
        hgl::graph::ShaderProgram *program = nullptr;

        // Runtime row indices, materialized independently for this primitive.
        // They must not be sourced from a shared recipe/spec cache entry.
        uint32_t texture_layer_row = uint32_t(-1);
        uint32_t data_index_row = uint32_t(-1);
        std::vector<uint32_t> data_index_values;

        // Dirty/lifecycle flags.
        bool program_dirty = true;
        bool bindings_dirty = true;
        bool resources_dirty = true;
        bool valid = false;
        MaterialRuntimeState runtime_state =
            MaterialRuntimeState::Unresolved;
        uint32_t runtime_revision = 0;
        uint64_t recipe_hash = 0;
        uint64_t program_build_context_hash = 0;
        graph::mtl::ResolvedBindingTable resolved_binding_table;
        std::vector<ResolvedSSBOBinding> resolved_ssbo_bindings;

        // Cached normalized recipe — avoids redundant NormalizeRecipe in CreatePipeline.
        graph::mtl::MaterialRecipe cached_normalized_recipe{};
        uint64_t cached_normalized_recipe_hash = 0;

        // P3: Cached effective recipe built with resolved program —
        // avoids redundant BuildResolvedRecipe in
        // MaterializeRecipeRowsForPrimitive and ResolveRuntimePipelineForPrimitive.
        graph::mtl::MaterialRecipe cached_effective_recipe{};
        uint64_t cached_effective_recipe_hash = 0;

        // P2-1: Cached pruned recipe projected back from the resolved binding
        // table (recipe → table → recipe). Computed once when the binding table
        // is rebuilt instead of re-running BuildBindingTableRecipe on every
        // materialize. Cleared together with resolved_binding_table.
        graph::mtl::MaterialRecipe cached_binding_recipe{};
        bool cached_binding_recipe_valid = false;

        // P3: Tracks the last observed PrimitiveComponent::material_authored_generation.
        // When this matches the primitive's current generation, all cached material
        // data is valid and ResolveMaterialProgramForPrimitive can skip entirely.
        uint32_t tracked_material_authored_generation = 0;

        // Epoch of the last materialization table rebuild in which this
        // primitive's rows were written. A mismatch with the system's current
        // materialize_epoch means the global tables were rebuilt while this
        // primitive was skipped (e.g. invisible), so its rows must be
        // re-materialized before rendering.
        uint64_t last_materialize_epoch = 0;

    public:

        MaterialComponent(const std::string &name = "MaterialRuntime");
        ~MaterialComponent() override = default;

    public:

        void MarkProgramDirty();
        void MarkBindingsDirty();
        void MarkResourcesDirty();
        void MarkInvalid();
        void MarkValid();
        void MarkProgramResolved();
        void MarkResourcesPending();
        void MarkFailed();
        void ClearMaterializationInstanceData();
        void ClearResolvedSSBOBindings();
        void ClearResolvedBindingTable();
        void SetResolvedSSBOBinding(const char *data_slot_name,
                                    uint32_t data_slot,
                                    graph::mtl::SSBOType ssbo_type,
                                    uint32_t ssbo_id);
        const ResolvedSSBOBinding *FindResolvedSSBOBinding(const char *data_slot_name,
                                                           uint32_t data_slot,
                                                           graph::mtl::SSBOType ssbo_type) const;

        void OnAttach() override;
        void OnDetach() override;
    };
}//namespace hgl::ecs
