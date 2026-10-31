#include <hgl/ecs/support/RenderableBuilder.h>
#include <hgl/ecs/components/GeometryData.h>
#include <hgl/ecs/components/MaterialData.h>
#include <hgl/graph/core/GraphicsContext.h>
#include <hgl/graph/asset/PrimitiveAssetRegistry.h>
#include <hgl/graph/asset/GeometryAssetRegistry.h>
#include <hgl/graph/asset/MaterialDataBlockRegistry.h>
#include <hgl/graph/module/GlobalSSBOBufferRegistry.h>
#include <hgl/asset/AssetURI.h>
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

        if (!asset || !asset->IsDeclared() || variant_index >= asset->GetVariantCount())
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

    RenderableBuilder &RenderableBuilder::WithPrimitiveAssetPath(
        const hgl::OSString &primitive_path,
        const uint32_t variant_index)
    {
        if (failed || !entity)
            return *this;

        hgl::graph::GraphicsContext *gc = world->GetGraphicsContext();
        if (!gc)
        {
            Fail("WithPrimitiveAssetPath: no GraphicsContext");
            return *this;
        }

        hgl::graph::PrimitiveAssetRegistry *registry = gc->GetPrimitiveAssetRegistry();
        if (!registry)
        {
            Fail("WithPrimitiveAssetPath: no PrimitiveAssetRegistry");
            return *this;
        }

        const hgl::graph::PrimitiveAsset *asset = registry->GetOrLoad(primitive_path);
        if (!asset)
        {
            Fail("WithPrimitiveAssetPath: GetOrLoad");
            return *this;
        }

        return WithPrimitiveAsset(asset, variant_index);
    }

    RenderableBuilder &RenderableBuilder::WithGeometry(
        const hgl::OSString &geometry_uri,
        const hgl::graph::mtl::MaterialRecipe &recipe,
        const hgl::graph::PrimitiveType primitive_type)
    {
        if (failed || !entity)
            return *this;

        if (geometry_uri.IsEmpty() || recipe.recipe_name.empty())
        {
            Fail("WithGeometry: empty uri or recipe name");
            return *this;
        }

        hgl::graph::GraphicsContext *gc = world->GetGraphicsContext();
        if (!gc)
        {
            Fail("WithGeometry: no GraphicsContext");
            return *this;
        }

        hgl::graph::PrimitiveAssetRegistry *registry = gc->GetPrimitiveAssetRegistry();
        if (!registry)
        {
            Fail("WithGeometry: no PrimitiveAssetRegistry");
            return *this;
        }

        // 资源 id 必须同时编码「几何 + 配方」：只按几何去重会让同几何不同配方的
        // 实体共享到第一个配方。`name` 非空 ⇒ 注册器用 `prm:@<name>` 作 id。
        hgl::graph::PrimitiveAssetSpec spec;

        spec.name  = hgl::asset::AssetURISegmentToAnsi(geometry_uri);
        spec.name += "@";
        spec.name += recipe.recipe_name.c_str();

        spec.geometry_path  = geometry_uri;
        spec.recipe_name    = recipe.recipe_name.c_str();
        spec.primitive_type = primitive_type;

        const hgl::graph::PrimitiveAsset *asset = registry->GetOrCreate(spec);
        if (!asset)
        {
            Fail("WithGeometry: PrimitiveAssetRegistry::GetOrCreate");
            return *this;
        }

        return WithPrimitiveAsset(asset).WithRecipe(recipe);
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

    RenderableBuilder &RenderableBuilder::WithMaterialDataRow(
        const hgl::OSString &block_uri,
        uint32_t row_index)
    {
        if (failed || !entity)
            return *this;

        hgl::graph::GraphicsContext *gc = world->GetGraphicsContext();
        if (!gc)
        {
            Fail("WithMaterialDataRow: no GraphicsContext");
            return *this;
        }

        hgl::graph::mtl::MaterialDataBlockRegistry *blocks =
            gc->GetMaterialDataBlockRegistry();
        hgl::graph::GlobalSSBOBufferRegistry *pool =
            gc->GetGlobalSSBOBufferRegistry();
        if (!blocks || !pool)
        {
            Fail("WithMaterialDataRow: no MaterialDataBlockRegistry");
            return *this;
        }

        hgl::graph::mtl::MaterialDataBlockAsset *block = blocks->GetOrLoadURI(block_uri);
        if (!block)
        {
            Fail("WithMaterialDataRow: GetOrLoadURI");
            return *this;
        }

        // 整块上传是幂等的：已经上船的块直接返回 true，只有新块才真占地。
        if (!block->Upload(pool))
        {
            Fail("WithMaterialDataRow: Upload");
            return *this;
        }

        if (row_index >= block->GetRowCount())
        {
            Fail("WithMaterialDataRow: row index out of range");
            return *this;
        }

        hgl::graph::GlobalSSBOBinding binding{};
        binding.ssbo_type  = block->GetGlobalSSBOType();
        binding.ssbo_id    = block->GetSSBOId();
        binding.data_index = block->RowIDAt(row_index);

        return WithMaterialDataResource(binding);
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
