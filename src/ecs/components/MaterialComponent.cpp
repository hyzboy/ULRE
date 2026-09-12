#include<hgl/ecs/components/MaterialComponent.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/Entity.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/module/SSBOBufferRegistry.h>

namespace hgl::ecs
{
    namespace
    {
        void RetireTextureConfiguration(MaterialComponent &component)
        {
            if (!component.material_texture_configuration.IsValid())
                return;

            Entity *owner = component.GetOwner();
            ECSContext *context = owner ? owner->GetContext() : nullptr;
            auto *graphics_context = context
                ? context->GetGraphicsContext()
                : nullptr;
            auto *registry = graphics_context
                ? graphics_context->GetSSBOBufferRegistry()
                : nullptr;
            if (registry)
            {
                if (registry->IsMaterialTextureConfigurationValid(
                        component.material_texture_configuration))
                {
                    registry->RetireMaterialTextureConfiguration(
                        component.material_texture_configuration,
                        static_cast<uint64_t>(
                            context->GetRenderSubmissionSerial())
                            + graph::MaterialTextureConfigurationRetireEpochDelay);
                }
            }
        }
    }

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

    void MaterialComponent::ClearResolvedBindingTable()
    {
        resolved_binding_table = {};
        cached_binding_recipe = {};
        cached_binding_recipe_valid = false;
    }

    void MaterialComponent::ClearMaterializationRows()
    {
        data_index_row = uint32_t(-1);
        material_row_cpu = nullptr;
        material_row_gpu = 0;
        material_texture_configuration = {};
        material_texture_row_cpu = nullptr;
        material_texture_row_gpu = 0;
        material_texture_zero_row_gpu = 0;
        material_texture_configuration_hash = 0;
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
        ClearResolvedBindingTable();
    }

    void MaterialComponent::OnDetach()
    {
        RetireTextureConfiguration(*this);
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
        ClearResolvedBindingTable();
    }
}//namespace hgl::ecs
