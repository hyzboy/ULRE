#include<hgl/ecs/components/MaterialComponent.h>
#include<cstring>

namespace hgl::ecs
{
    MaterialComponent::MaterialComponent(const std::string &name)
        : Component(name)
    {
    }

    void MaterialComponent::MarkValid()
    {
        valid = true;
    }

    void MaterialComponent::MarkProgramResolved()
    {
        valid = false;
    }

    void MaterialComponent::MarkResourcesPending()
    {
        valid = false;
    }

    void MaterialComponent::MarkFailed()
    {
        valid = false;
    }

    void MaterialComponent::ClearResolvedSSBOBindings()
    {
        resolved_ssbo_bindings.clear();
        data_index_values.clear();
    }

    void MaterialComponent::ClearResolvedBindingTable()
    {
        resolved_binding_table = {};
        cached_binding_recipe = {};
        cached_binding_recipe_valid = false;
    }

    void MaterialComponent::ClearMaterializationRows()
    {
        data_index_row = uint32_t(-1);
        data_index_values.clear();
    }

    void MaterialComponent::SetResolvedSSBOBinding(const char *material_private_data_slot_name,
                                                   const uint32_t material_private_data_slot,
                                                   graph::mtl::SSBOType ssbo_type,
                                                   const uint32_t ssbo_id)
    {
        if (!material_private_data_slot_name || !*material_private_data_slot_name)
            return;

        for (auto &binding : resolved_ssbo_bindings)
        {
            if (binding.valid
             && binding.material_private_data_slot == material_private_data_slot
             && binding.material_private_data_slot_name
             && std::strcmp(binding.material_private_data_slot_name, material_private_data_slot_name) == 0)
            {
                binding.ssbo_type = ssbo_type;
                binding.ssbo_id = ssbo_id;
                return;
            }
        }

        ResolvedSSBOBinding binding{};
        binding.material_private_data_slot_name = material_private_data_slot_name;
        binding.material_private_data_slot = material_private_data_slot;
        binding.ssbo_type = ssbo_type;
        binding.ssbo_id = ssbo_id;
        binding.valid = true;
        resolved_ssbo_bindings.emplace_back(binding);
    }

    const MaterialComponent::ResolvedSSBOBinding *
        MaterialComponent::FindResolvedSSBOBinding(
            const char *material_private_data_slot_name,
            const uint32_t material_private_data_slot,
            graph::mtl::SSBOType ssbo_type) const
    {
        if (!material_private_data_slot_name || !*material_private_data_slot_name)
            return nullptr;

        for (const auto &binding : resolved_ssbo_bindings)
        {
            if (binding.valid
             && binding.material_private_data_slot == material_private_data_slot
             && binding.ssbo_type == ssbo_type
             && binding.material_private_data_slot_name
             && std::strcmp(binding.material_private_data_slot_name, material_private_data_slot_name) == 0)
                return &binding;
        }

        return nullptr;
    }

    void MaterialComponent::OnAttach()
    {
        program_dirty = true;
        runtime_dirty = true;
        valid = false;
        recipe_hash = 0;
        cached_effective_recipe = {};
        cached_effective_recipe_hash = 0;
        tracked_material_authored_generation = 0;
        ClearMaterializationRows();
        ClearResolvedSSBOBindings();
        ClearResolvedBindingTable();
    }

    void MaterialComponent::OnDetach()
    {
        program = nullptr;
        program_dirty = true;
        runtime_dirty = true;
        valid = false;
        recipe_hash = 0;
        cached_normalized_recipe = {};
        cached_effective_recipe = {};
        cached_effective_recipe_hash = 0;
        tracked_material_authored_generation = 0;
        ClearMaterializationRows();
        ClearResolvedSSBOBindings();
        ClearResolvedBindingTable();
    }
}//namespace hgl::ecs
