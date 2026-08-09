#include<hgl/ecs/components/MaterialComponent.h>
#include<cstring>

namespace hgl::ecs
{
    MaterialComponent::MaterialComponent(const std::string &name)
        : Component(name)
    {
    }

    void MaterialComponent::MarkProgramDirty()
    {
        program_dirty = true;
        valid = false;
        ClearMaterializationInstanceData();
        ClearResolvedSSBOBindings();
        ClearActiveProfileBindingView();
        ++runtime_revision;
    }

    void MaterialComponent::MarkBindingsDirty()
    {
        bindings_dirty = true;
        valid = false;
        ClearMaterializationInstanceData();
        ClearResolvedSSBOBindings();
        ClearActiveProfileBindingView();
        ++runtime_revision;
    }

    void MaterialComponent::MarkResourcesDirty()
    {
        resources_dirty = true;
        valid = false;
        ClearMaterializationInstanceData();
        ClearResolvedSSBOBindings();
        ClearActiveProfileBindingView();
        ++runtime_revision;
    }

    void MaterialComponent::MarkInvalid()
    {
        valid = false;
        ClearMaterializationInstanceData();
        ClearResolvedSSBOBindings();
        ClearActiveProfileBindingView();
        ++runtime_revision;
    }

    void MaterialComponent::MarkValid()
    {
        valid = true;
    }

    void MaterialComponent::ClearResolvedSSBOBindings()
    {
        resolved_ssbo_bindings.clear();
        data_index_values.clear();
    }

    void MaterialComponent::ClearActiveProfileBindingView()
    {
        active_profile_binding_view = {};
        ClearShadowResourceAcquirePlan();
    }

    void MaterialComponent::ClearShadowResourceAcquirePlan()
    {
        shadow_resource_acquire_plan = {};
        has_shadow_resource_acquire_plan = false;
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
        recipe_hash = 0;
        ClearMaterializationInstanceData();
        ClearResolvedSSBOBindings();
        ClearActiveProfileBindingView();
    }

    void MaterialComponent::OnDetach()
    {
        program = nullptr;
        program_dirty = true;
        bindings_dirty = true;
        resources_dirty = true;
        valid = false;
        recipe_hash = 0;
        ClearMaterializationInstanceData();
        ClearResolvedSSBOBindings();
        ClearActiveProfileBindingView();
    }
}//namespace hgl::ecs
