#include <hgl/ecs/support/RenderableBuilder.h>
#include <hgl/ecs/components/GeometryData.h>
#include <hgl/ecs/components/MaterialData.h>
#include <hgl/log/Log.h>

namespace hgl::ecs
{
    RenderableBuilder::RenderableBuilder(
        ECSContext *context,
        const char *name)
        : world(context)
    {
        if (!world || !name || !name[0])
        {
            Fail("constructor");
            return;
        }

        entity = world->CreateEntity<Entity>(name);
        if (!entity)
            Fail("CreateEntity");
    }

    RenderableBuilder::~RenderableBuilder()
    {
        if (entity && !built && world)
            world->DestroyEntity(entity->GetEntityID());
    }

    void RenderableBuilder::Fail(const char *operation)
    {
        failed = true;
        GLogError("[RenderableBuilder] %s failed", operation);
    }

    RenderableBuilder &RenderableBuilder::WithTransform(
        const glm::vec3 &position,
        const glm::quat &rotation,
        const glm::vec3 &scale,
        const Mobility mobility)
    {
        if (failed || !entity)
            return *this;

        if (has_transform)
        {
            Fail("WithTransform called more than once");
            return *this;
        }

        const TransformID transform_id =
            world->CreateTransform(entity->GetEntityID(), mobility);
        if (!IsValidTransformID(transform_id))
        {
            Fail("CreateTransform");
            return *this;
        }

        const TransformAccessor transform = world->GetTransform(transform_id);
        if (!transform.IsValid())
        {
            world->DestroyTransform(transform_id);
            Fail("GetTransform");
            return *this;
        }

        transform.SetLocalTRS(position, rotation, scale);
        has_transform = true;
        return *this;
    }

    RenderableBuilder &RenderableBuilder::WithPrimitiveAsset(
        const hgl::graph::PrimitiveAsset *asset,
        const uint32_t variant_index)
    {
        if (failed || !entity)
            return *this;

        if (!asset || !asset->IsValid() || variant_index >= asset->GetVariantCount())
        {
            Fail("WithPrimitiveAsset");
            return *this;
        }

        GeometryData *geometry_data = world->GetOrCreateGeometryData(entity);
        if (!geometry_data)
        {
            Fail("GetOrCreateGeometryData");
            return *this;
        }

        geometry_data->SetPrimitiveAsset(asset);
        geometry_data->SetPrimitiveVariantIndex(variant_index);
        return *this;
    }

    RenderableBuilder &RenderableBuilder::WithRecipe(
        const hgl::graph::mtl::MaterialRecipe &recipe)
    {
        if (failed || !entity)
            return *this;

        MaterialData *material_data = world->GetOrCreateMaterialData(entity);
        if (!material_data)
        {
            Fail("GetOrCreateMaterialData");
            return *this;
        }

        material_data->SetRecipe(recipe);
        return *this;
    }

    RenderableBuilder &RenderableBuilder::WithMaterialDataResource(
        const hgl::graph::GlobalSSBOBinding &binding)
    {
        if (failed || !entity)
            return *this;

        if (!binding.IsValid())
        {
            Fail("WithMaterialDataResource");
            return *this;
        }

        MaterialData *material_data = world->GetOrCreateMaterialData(entity);
        if (!material_data)
        {
            Fail("GetOrCreateMaterialData");
            return *this;
        }

        MaterialData::MaterialDataAuthoringResource resource{};
        resource = binding;
        material_data->SetDataResource(resource);
        return *this;
    }

    Entity *RenderableBuilder::Build()
    {
        if (failed || !entity)
            return nullptr;

        built = true;
        Entity *result = entity;
        entity = nullptr;
        return result;
    }
}
