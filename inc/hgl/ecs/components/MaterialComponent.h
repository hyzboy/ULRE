#pragma once

#include<hgl/ecs/core/Component.h>

namespace hgl::graph
{
    class MaterialProgram;
    class DescriptorBindingSet;
}

namespace hgl::ecs
{
    class MaterialComponent : public Component
    {
    public:

        // Runtime shared program, resolved by ECS.
        hgl::graph::MaterialProgram *program = nullptr;

        // Runtime row indices, materialized by ECS.
        uint32_t material_instance_row = uint32_t(-1);
        uint32_t texture_layer_row = uint32_t(-1);
        uint32_t data_index_row = uint32_t(-1);

        // Dirty/lifecycle flags.
        bool program_dirty = true;
        bool bindings_dirty = true;
        bool resources_dirty = true;
        bool valid = false;
        uint32_t runtime_revision = 0;

        // Compatibility bridge (internal-use transitional field).
        hgl::graph::DescriptorBindingSet *dbs_compat = nullptr;

    public:

        MaterialComponent(const std::string &name = "MaterialRuntime");
        ~MaterialComponent() override = default;

    public:

        void MarkProgramDirty();
        void MarkBindingsDirty();
        void MarkResourcesDirty();
        void MarkInvalid();
        void MarkValid();

        void OnAttach() override;
        void OnDetach() override;
    };
}//namespace hgl::ecs

