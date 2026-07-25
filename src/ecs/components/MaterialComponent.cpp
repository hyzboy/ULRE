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
        ++runtime_revision;
    }

    void MaterialComponent::MarkBindingsDirty()
    {
        bindings_dirty = true;
        valid = false;
        ++runtime_revision;
    }

    void MaterialComponent::MarkResourcesDirty()
    {
        resources_dirty = true;
        valid = false;
        ++runtime_revision;
    }

    void MaterialComponent::MarkInvalid()
    {
        valid = false;
        ++runtime_revision;
    }

    void MaterialComponent::MarkValid()
    {
        valid = true;
    }

    void MaterialComponent::OnAttach()
    {
        program_dirty = true;
        bindings_dirty = true;
        resources_dirty = true;
        valid = false;
    }

    void MaterialComponent::OnDetach()
    {
        program = nullptr;
        material_instance_row = uint32_t(-1);
        texture_layer_row = uint32_t(-1);
        data_index_row = uint32_t(-1);
        program_dirty = true;
        bindings_dirty = true;
        resources_dirty = true;
        valid = false;
    }
}//namespace hgl::ecs
