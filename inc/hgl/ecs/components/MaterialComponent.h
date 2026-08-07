#pragma once

#include<hgl/ecs/core/Component.h>
#include<hgl/mtl/MaterialResourceLayout.h>
#include<vector>

namespace hgl::graph
{
    class ShaderProgram;
}

namespace hgl::ecs
{
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
        uint32_t runtime_revision = 0;
        uint64_t recipe_hash = 0;
        std::vector<ResolvedSSBOBinding> resolved_ssbo_bindings;

    public:

        MaterialComponent(const std::string &name = "MaterialRuntime");
        ~MaterialComponent() override = default;

    public:

        void MarkProgramDirty();
        void MarkBindingsDirty();
        void MarkResourcesDirty();
        void MarkInvalid();
        void MarkValid();
        void ClearMaterializationInstanceData();
        void ClearResolvedSSBOBindings();
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
