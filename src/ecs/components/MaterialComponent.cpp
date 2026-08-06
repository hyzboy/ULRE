#include<hgl/ecs/components/MaterialComponent.h>

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
        ClearResolvedSSBOBindings();
        ++runtime_revision;
    }

    void MaterialComponent::MarkBindingsDirty()
    {
        bindings_dirty = true;
        valid = false;
        ClearResolvedSSBOBindings();
        ++runtime_revision;
    }

    void MaterialComponent::MarkResourcesDirty()
    {
        resources_dirty = true;
        valid = false;
        ClearResolvedSSBOBindings();
        ++runtime_revision;
    }

    void MaterialComponent::MarkInvalid()
    {
        valid = false;
        ClearResolvedSSBOBindings();
        ++runtime_revision;
    }

    void MaterialComponent::MarkValid()
    {
        valid = true;
    }

    void MaterialComponent::ClearResolvedSSBOBindings()
    {
        resolved_ssbo_bindings.clear();
    }

    void MaterialComponent::SetResolvedSSBOBinding(uint32_t ssbo_slot, graph::mtl::SSBOType ssbo_type, uint32_t ssbo_id)
    {
        const size_t index = static_cast<size_t>(ssbo_slot);
        if (index >= resolved_ssbo_bindings.size())
            resolved_ssbo_bindings.resize(index + 1);

        auto &binding = resolved_ssbo_bindings[index];
        binding.ssbo_type = ssbo_type;
        binding.ssbo_id = ssbo_id;
        binding.valid = true;
    }

    const MaterialComponent::ResolvedSSBOBinding *MaterialComponent::FindResolvedSSBOBinding(uint32_t ssbo_slot,
                                                                                             graph::mtl::SSBOType ssbo_type) const
    {
        const size_t index = static_cast<size_t>(ssbo_slot);
        if (index >= resolved_ssbo_bindings.size())
            return nullptr;

        const auto &binding = resolved_ssbo_bindings[index];
        if (!binding.valid || binding.ssbo_type != ssbo_type)
            return nullptr;

        return &binding;
    }

    void MaterialComponent::OnAttach()
    {
        program_dirty = true;
        bindings_dirty = true;
        resources_dirty = true;
        valid = false;
        recipe_hash = 0;
        ClearResolvedSSBOBindings();
    }

    void MaterialComponent::OnDetach()
    {
        program = nullptr;
        texture_layer_row = uint32_t(-1);
        ssbo_index_row = uint32_t(-1);
        program_dirty = true;
        bindings_dirty = true;
        resources_dirty = true;
        valid = false;
        recipe_hash = 0;
        ClearResolvedSSBOBindings();
    }
}//namespace hgl::ecs
