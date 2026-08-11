#include<hgl/ecs/components/MaterialComponent.h>
#include<cstring>

namespace hgl::ecs
{
    const char *GetMaterialRuntimeStateName(
        const MaterialRuntimeState state) noexcept
    {
        switch (state)
        {
        case MaterialRuntimeState::Unresolved:
            return "Unresolved";
        case MaterialRuntimeState::ProgramResolved:
            return "ProgramResolved";
        case MaterialRuntimeState::ResourcesPending:
            return "ResourcesPending";
        case MaterialRuntimeState::Ready:
            return "Ready";
        case MaterialRuntimeState::Failed:
            return "Failed";
        }
        return "Unknown";
    }

    MaterialComponent::MaterialComponent(const std::string &name)
        : Component(name)
    {
    }

    void MaterialComponent::MarkProgramDirty()
    {
        program_dirty = true;
        valid = false;
        runtime_state = MaterialRuntimeState::Unresolved;
        ClearMaterializationInstanceData();
        ClearResolvedSSBOBindings();
        ClearResolvedBindingTable();
        cached_normalized_recipe = {};
        cached_normalized_recipe_hash = 0;
        cached_effective_recipe = {};
        cached_effective_recipe_hash = 0;
        tracked_material_authored_generation = 0;
        ++runtime_revision;
    }

    void MaterialComponent::MarkBindingsDirty()
    {
        bindings_dirty = true;
        valid = false;
        runtime_state = MaterialRuntimeState::Unresolved;
        ClearMaterializationInstanceData();
        ClearResolvedSSBOBindings();
        ClearResolvedBindingTable();
        ++runtime_revision;
    }

    void MaterialComponent::MarkResourcesDirty()
    {
        resources_dirty = true;
        valid = false;
        runtime_state = MaterialRuntimeState::ResourcesPending;
        ClearMaterializationInstanceData();
        ClearResolvedSSBOBindings();
        ++runtime_revision;
    }

    void MaterialComponent::MarkInvalid()
    {
        valid = false;
        runtime_state = MaterialRuntimeState::Failed;
        ClearMaterializationInstanceData();
        ClearResolvedSSBOBindings();
        ClearResolvedBindingTable();
        ++runtime_revision;
    }

    void MaterialComponent::MarkValid()
    {
        valid = true;
        runtime_state = MaterialRuntimeState::Ready;
    }

    void MaterialComponent::MarkProgramResolved()
    {
        valid = false;
        runtime_state = MaterialRuntimeState::ProgramResolved;
    }

    void MaterialComponent::MarkResourcesPending()
    {
        valid = false;
        runtime_state = MaterialRuntimeState::ResourcesPending;
    }

    void MaterialComponent::MarkFailed()
    {
        valid = false;
        runtime_state = MaterialRuntimeState::Failed;
    }

    void MaterialComponent::ClearResolvedSSBOBindings()
    {
        resolved_ssbo_bindings.clear();
        data_index_values.clear();
    }

    void MaterialComponent::ClearResolvedBindingTable()
    {
        resolved_binding_table = {};
        ClearActiveResourceAcquirePlan();
    }

    void MaterialComponent::ClearActiveResourceAcquirePlan()
    {
        active_resource_acquire_plan = {};
        has_active_resource_acquire_plan = false;
    }

    void MaterialComponent::ClearMaterializationInstanceData()
    {
        texture_layer_row = uint32_t(-1);
        data_index_row = uint32_t(-1);
        data_index_values.clear();
    }

    void MaterialComponent::SetResolvedSSBOBinding(const char *data_slot_name,
                                                   const uint32_t data_slot,
                                                   graph::mtl::SSBOType ssbo_type,
                                                   const uint32_t ssbo_id)
    {
        if (!data_slot_name || !*data_slot_name)
            return;

        for (auto &binding : resolved_ssbo_bindings)
        {
            if (binding.valid
             && binding.data_slot == data_slot
             && binding.data_slot_name
             && std::strcmp(binding.data_slot_name, data_slot_name) == 0)
            {
                binding.ssbo_type = ssbo_type;
                binding.ssbo_id = ssbo_id;
                return;
            }
        }

        ResolvedSSBOBinding binding{};
        binding.data_slot_name = data_slot_name;
        binding.data_slot = data_slot;
        binding.ssbo_type = ssbo_type;
        binding.ssbo_id = ssbo_id;
        binding.valid = true;
        resolved_ssbo_bindings.emplace_back(binding);
    }

    const MaterialComponent::ResolvedSSBOBinding *
        MaterialComponent::FindResolvedSSBOBinding(
            const char *data_slot_name,
            const uint32_t data_slot,
            graph::mtl::SSBOType ssbo_type) const
    {
        if (!data_slot_name || !*data_slot_name)
            return nullptr;

        for (const auto &binding : resolved_ssbo_bindings)
        {
            if (binding.valid
             && binding.data_slot == data_slot
             && binding.ssbo_type == ssbo_type
             && binding.data_slot_name
             && std::strcmp(binding.data_slot_name, data_slot_name) == 0)
                return &binding;
        }

        return nullptr;
    }

    void MaterialComponent::OnAttach()
    {
        program_dirty = true;
        bindings_dirty = true;
        resources_dirty = true;
        valid = false;
        runtime_state = MaterialRuntimeState::Unresolved;
        recipe_hash = 0;
        cached_effective_recipe = {};
        cached_effective_recipe_hash = 0;
        tracked_material_authored_generation = 0;
        ClearMaterializationInstanceData();
        ClearResolvedSSBOBindings();
        ClearResolvedBindingTable();
    }

    void MaterialComponent::OnDetach()
    {
        program = nullptr;
        program_dirty = true;
        bindings_dirty = true;
        resources_dirty = true;
        valid = false;
        runtime_state = MaterialRuntimeState::Unresolved;
        recipe_hash = 0;
        cached_normalized_recipe = {};
        cached_normalized_recipe_hash = 0;
        cached_effective_recipe = {};
        cached_effective_recipe_hash = 0;
        tracked_material_authored_generation = 0;
        ClearMaterializationInstanceData();
        ClearResolvedSSBOBindings();
        ClearResolvedBindingTable();
    }
}//namespace hgl::ecs
