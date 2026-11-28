// StaticMeshModelInstance.cpp — .model 场景 → 单根 ECS 实体 + GPU 模型模板

#include <hgl/ecs/support/assembly/StaticMeshModelInstance.h>
#include <hgl/ecs/components/GPUModelRefComponent.h>
#include <hgl/ecs/core/Context.h>
#include <hgl/ecs/core/entity/Entity.h>
#include <hgl/ecs/support/material/MaterialRuntimeTable.h>
#include <hgl/ecs/support/material/MaterialVariantTable.h>
#include <hgl/ecs/support/material/ModelTemplateBuilder.h>
#include <hgl/ecs/support/transform/TransformAccessor.h>
#include <hgl/graph/core/GraphicsContext.h>
#include <hgl/graph/geo/VKGeometry.h>
#include <hgl/graph/ssbo/ModelTemplateRows.h>
#include <hgl/log/Log.h>
#include <hgl/mtl/MaterialResolver.h>
#include <hgl/mtl/ScenePassProfileResolver.h>
#include <glm/gtc/quaternion.hpp>

DEFINE_LOGGER_MODULE(StaticMeshModel)
USE_MODULE_LOGGER(StaticMeshModel)

namespace hgl::ecs
{
    namespace
    {
        const math::Vector3f kUnitPosition(0.0f, 0.0f, 0.0f);
        const math::Quatf kUnitRotation(1.0f, 0.0f, 0.0f, 0.0f);
        const math::Vector3f kUnitScale(1.0f, 1.0f, 1.0f);

        graph::mtl::ShaderProgramPurpose GetForwardPurpose(
            const graph::PrimitiveVariantPurpose purpose)
        {
            switch (purpose)
            {
            case graph::PrimitiveVariantPurpose::DepthOnly:
                return graph::mtl::ShaderProgramPurpose::DepthOnly;
            case graph::PrimitiveVariantPurpose::ShadowCaster:
                return graph::mtl::ShaderProgramPurpose::ShadowDepth;
            default:
                return graph::mtl::ShaderProgramPurpose::ForwardColor;
            }
        }
    }

    StaticMeshModelInstance::~StaticMeshModelInstance()
    {
        Release();
    }

    void StaticMeshModelInstance::Release(const bool root_entity_is_destroying)
    {
        if (world && root_entity.IsValid())
        {
            ecs::Entity *root = world->GetEntity(root_entity);
            if (root)
            {
                if (auto *model_ref = root->GetComponent<GPUModelRefComponent>())
                    if (model_ref->GetTemplateID() == model_template_id)
                        model_ref->SetTemplateBinding(0u);

                if (owns_root_entity)
                {
                    world->DestroyEntity(root_entity);
                }
                else if (!root_entity_is_destroying)
                {
                    if (ecs::IsValidTransformID(root_transform))
                        world->Spatial().DestroyTransform(root_transform);

                    const ecs::BoundingBoxAccessor bounds =
                        world->Spatial().GetBoundingBoxByEntity(root_entity);
                    if (bounds.IsValid())
                        world->Spatial().DestroyBoundingBox(bounds.GetID());
                }
            }
        }

        if (world && model_template_id != 0u)
        {
            graph::GraphicsContext *graphics =
                world->Environment().GetGraphicsContext();
            graph::GlobalSSBOBufferRegistry *registry =
                graphics ? graphics->GetGlobalSSBOBufferRegistry() : nullptr;
            if (!DestroyMultiMaterialModelTemplate(registry, world, model_template_id))
                FLogError(u8"Release: GPU 模型模板注销失败（template=%u）",
                          model_template_id);
        }

        owned_assets.Clear();
        skeleton_bounds.Clear();
        skeleton_bounds_valid = false;
        source_node_count = 0;
        attached_primitive_count = 0;
        model_template_id = 0;
        root_entity = ecs::EntityID::Invalid();
        root_transform = ecs::INVALID_TRANSFORM_ID;
        owns_root_entity = false;
        world = nullptr;
    }

    bool StaticMeshModelInstance::InstantiateSkeleton(
        ecs::ECSContext *ctx,
        const std::vector<graph::StaticMeshNode> &nodes,
        const std::vector<int32_t> &root_nodes,
        const SkeletonOptions &options,
        ecs::Entity *existing_root)
    {
        if (world)
        {
            FLogWarning(u8"InstantiateSkeleton: 实例已存在，请先 Release()");
            return false;
        }
        if (!ctx)
        {
            FLogError(u8"InstantiateSkeleton: ECSContext 为空");
            return false;
        }
        if (existing_root
            && ctx->GetEntity(existing_root->GetEntityID()) != existing_root)
        {
            FLogError(u8"InstantiateSkeleton: 外部根实体不属于该 ECS 世界");
            return false;
        }

        world = ctx;
        source_node_count = static_cast<int>(nodes.size());
        owns_root_entity = existing_root == nullptr;
        root_entity = existing_root
            ? existing_root->GetEntityID()
            : ecs::EntityID::Invalid();
        (void)root_nodes;

        if (!existing_root)
        {
            ecs::Entity *entity =
                world->CreateEntity<ecs::Entity>("StaticMeshModelRoot");
            if (!entity)
            {
                FLogError(u8"InstantiateSkeleton: 根实体创建失败");
                Release();
                return false;
            }
            root_entity = entity->GetEntityID();
        }

        root_transform = world->Spatial().GetTransformID(root_entity);
        const bool created_transform = !ecs::IsValidTransformID(root_transform);
        if (created_transform)
            root_transform = world->Spatial().CreateTransform(root_entity, ecs::Mobility::Movable);
        if (!ecs::IsValidTransformID(root_transform))
        {
            FLogError(u8"InstantiateSkeleton: 根 Transform 创建失败");
            Release();
            return false;
        }

        ecs::TransformAccessor transform = world->Spatial().GetTransform(root_transform);
        if (!transform.IsValid())
        {
            FLogError(u8"InstantiateSkeleton: 根 Transform 句柄无效");
            Release();
            return false;
        }
        if (created_transform || owns_root_entity)
            transform.SetLocalTRS(kUnitPosition, kUnitRotation, kUnitScale);
        transform.SetMobility(options.mobility);

        math::AABB merged_box;
        merged_box.Clear();
        bool any_box = false;
        for (const graph::StaticMeshNode &node : nodes)
        {
            if (!node.boundsValid || node.nodeBounds.aabb.IsEmpty())
                continue;

            merged_box.Merge(node.nodeBounds.aabb.Transformed(node.worldMatrix));
            any_box = true;
        }

        if (any_box)
        {
            skeleton_bounds.SetFromAABB(merged_box);
            skeleton_bounds_valid = true;
            world->Spatial().GetOrCreateBoundingBox(root_entity).SetLocalBounds(merged_box);
        }

        FLogInfo(u8"InstantiateSkeleton: 单根模型锚点就绪（模型节点 %zu，包围盒 %s）",
                 nodes.size(), any_box ? "有效" : "缺失");
        return true;
    }

    bool StaticMeshModelInstance::BuildTemplate(
        const std::vector<graph::StaticMeshNode> &nodes,
        const Options &options)
    {
        if (!world || !root_entity.IsValid() || model_template_id != 0u)
            return false;

        graph::GraphicsContext *graphics =
            world->Environment().GetGraphicsContext();
        graph::GlobalSSBOBufferRegistry *registry =
            graphics ? graphics->GetGlobalSSBOBufferRegistry() : nullptr;
        MaterialRuntimeTable *runtime_table = world->RenderData().RuntimeMaterials();
        MaterialVariantTable *variant_table = world->RenderData().Variants();
        if (!graphics || !registry || !runtime_table || !variant_table)
        {
            FLogError(u8"BuildTemplate: GraphicsContext 或材质运行期表未就绪");
            return false;
        }

        ValueArray<ModelTemplatePartInput> parts;
        ValueArray<MaterialRuntimeRowID> acquired_rows;
        uint32_t mount_order = 0;
        uint32_t meshlet_geometry_count = 0;
        uint64_t total_meshlet_count = 0;

        auto release_acquired_rows = [&]()
        {
            for (int i = 0; i < acquired_rows.GetCount(); ++i)
                runtime_table->Release(acquired_rows[i]);
            acquired_rows.Clear();
        };

        for (const graph::StaticMeshNode &node : nodes)
        {
            for (const int32_t primitive_index : node.primitiveIndices)
            {
                if (primitive_index < 0
                    || primitive_index >= owned_assets.GetCount())
                {
                    FLogError(u8"BuildTemplate: 图元索引越界（index=%d）", primitive_index);
                    release_acquired_rows();
                    return false;
                }

                const graph::PrimitiveAsset *asset = owned_assets.At(primitive_index);
                graph::Geometry *geometry = asset ? asset->GetGeometry() : nullptr;
                const graph::PrimitiveVariant *variant =
                    asset ? asset->GetDefaultVariant() : nullptr;
                if (!asset || !asset->IsGeometryReady() || !geometry
                    || !variant || !variant->material_recipe)
                {
                    FLogWarning(u8"BuildTemplate: 图元几何或配方尚未就绪（index=%d）",
                                primitive_index);
                    release_acquired_rows();
                    return false;
                }

                if (!geometry->EnsureMeshDrawParams(registry, graphics->GetDevice()))
                {
                    FLogError(u8"BuildTemplate: Geometry 全局池注册失败（index=%d）",
                              primitive_index);
                    release_acquired_rows();
                    return false;
                }
                if (asset->GetPrimitiveType() == graph::PrimitiveType::Triangles
                    && geometry->GetMeshletCount() == 0u)
                {
                    FLogError(u8"BuildTemplate: 三角形 Geometry 缺少 Meshlet 数据（index=%d）",
                              primitive_index);
                    release_acquired_rows();
                    return false;
                }
                if (geometry->GetMeshletCount() != 0u)
                {
                    ++meshlet_geometry_count;
                    total_meshlet_count += geometry->GetMeshletCount();
                }

                graph::mtl::MaterialRecipe recipe = *variant->material_recipe;
                graph::GlobalSSBOBinding material_binding =
                    recipe.material_ssbo_binding;
                if (options.material_rows && options.material_row_count > 0)
                {
                    const uint32_t row_index =
                        ResolveMaterialRowIndex(*asset, options, mount_order);
                    material_binding = options.material_rows[row_index];
                }
                if (material_binding.IsValid())
                    recipe.material_ssbo_binding = material_binding;

                const uint32_t data_index = recipe.material_ssbo_binding.IsValid()
                    ? recipe.material_ssbo_binding.data_index
                    : 0u;
                const AnsiString &ambient_provider =
                    world->Environment().GetAmbientProviderName();
                const char *ambient_name = ambient_provider.IsEmpty()
                    ? graph::mtl::kAmbientLightProviderNone
                    : ambient_provider.c_str();
                const uint64_t build_context =
                    graph::mtl::HashMaterialProgramBuildContext(
                        asset->GetPrimitiveType(),
                        &geometry->GetGeometryVertexFormat(),
                        graphics->GetPhysicalDeviceProfile(),
                        GetForwardPurpose(variant->purpose),
                        ambient_name);
                const uint64_t recipe_hash =
                    graph::mtl::HashMaterialRecipe(recipe);
                const MaterialRuntimeKey key{
                    build_context, recipe_hash, data_index};
                const MaterialRuntimeRowID row_id = runtime_table->Intern(key);
                if (row_id == INVALID_MATERIAL_RUNTIME_ROW_ID)
                {
                    FLogError(u8"BuildTemplate: MaterialRuntimeTable 容量不足");
                    release_acquired_rows();
                    return false;
                }
                acquired_rows.Add(row_id);

                MaterialRuntimeRow *runtime_row = runtime_table->GetMutable(row_id);
                if (!runtime_row)
                {
                    FLogError(u8"BuildTemplate: MaterialRuntimeRow 登记后不可读取");
                    release_acquired_rows();
                    return false;
                }
                if (runtime_row->cached_effective_recipe_hash == 0u)
                {
                    runtime_row->recipe_hash = recipe_hash;
                    runtime_row->cached_normalized_recipe = recipe;
                    runtime_row->cached_effective_recipe = recipe;
                    runtime_row->cached_effective_recipe_hash = recipe_hash;
                }
                if (runtime_row->data_index_row == uint32_t(-1))
                    runtime_row->data_index_row = data_index;

                ModelTemplatePartInput part;
                part.geometry = geometry;
                part.primitive_type = asset->GetPrimitiveType();
                part.local_matrix = node.worldMatrix;
                if (node.boundsValid)
                    part.local_bounds = node.nodeBounds.aabb;
                else
                    part.local_bounds.Clear();
                part.material.runtime_row = row_id;
                part.material.material_id = data_index;
                part.material.texture_id =
                    runtime_row->material_texture_configuration.IsValid()
                        ? runtime_row->material_texture_configuration.row_index
                        : 0u;
                part.material.can_cast_shadow = true;
                part.material.is_opaque = true;
                parts.Add(part);
                ++mount_order;
            }
        }

        if (parts.IsEmpty())
        {
            FLogError(u8"BuildTemplate: 模型没有可绘制图元");
            release_acquired_rows();
            return false;
        }

        ModelTemplateBuildStatus status = ModelTemplateBuildStatus::Failed;
        const uint32_t template_id = BuildMultiMaterialModelTemplate(
            registry, world, parts.GetData(),
            static_cast<uint32_t>(parts.GetCount()), &status);
        release_acquired_rows();
        if (template_id == graph::ssbo::kInvalidModelTemplateID)
        {
            FLogError(u8"BuildTemplate: 多材质 GPU 模型模板构建失败（status=%u）",
                      static_cast<uint32_t>(status));
            return false;
        }

        ecs::Entity *root = world->GetEntity(root_entity);
        GPUModelRefComponent *model_ref =
            root ? root->GetComponent<GPUModelRefComponent>() : nullptr;
        if (!model_ref && root)
            model_ref = root->AddComponent<GPUModelRefComponent>();
        if (!model_ref)
        {
            FLogError(u8"BuildTemplate: 根实体无法挂载 GPUModelRefComponent");
            DestroyMultiMaterialModelTemplate(registry, world, template_id);
            return false;
        }

        model_template_id = template_id;
        model_ref->SetTemplateBinding(model_template_id);
        attached_primitive_count = parts.GetCount();
        FLogInfo(u8"BuildTemplate: 单根实体绑定 GPU 模型（template=%u, parts=%d, meshlet_geometries=%u, meshlets=%llu）",
                 model_template_id, attached_primitive_count, meshlet_geometry_count,
                 static_cast<unsigned long long>(total_meshlet_count));
        return true;
    }

    bool StaticMeshModelInstance::AttachGeometries(
        const std::vector<graph::PrimitiveAsset> &assets,
        const std::vector<graph::StaticMeshNode> &nodes,
        const Options &options)
    {
        if (!world || !root_entity.IsValid())
        {
            FLogError(u8"AttachGeometries: 根实体未就绪，请先 InstantiateSkeleton()");
            return false;
        }
        if (nodes.size() != static_cast<size_t>(source_node_count))
        {
            FLogError(u8"AttachGeometries: 节点数与骨架不一致（骨架 %d / 入参 %zu）",
                      source_node_count, nodes.size());
            return false;
        }
        if (model_template_id != 0u)
            return false;

        for (int i = owned_assets.GetCount(); i < static_cast<int>(assets.size()); ++i)
        {
            if (!owned_assets.Create(assets[static_cast<size_t>(i)]))
            {
                FLogError(u8"AttachGeometries: 资产副本分配失败（index=%d）", i);
                return false;
            }
        }

        bool all_geometry_ready = true;
        for (const graph::StaticMeshNode &node : nodes)
        {
            for (const int32_t primitive_index : node.primitiveIndices)
            {
                if (primitive_index < 0
                    || primitive_index >= owned_assets.GetCount())
                {
                    FLogError(u8"AttachGeometries: 图元索引越界（index=%d）",
                              primitive_index);
                    return false;
                }

                const graph::PrimitiveAsset *asset = owned_assets.At(primitive_index);
                if (!asset || !asset->IsGeometryReady())
                {
                    all_geometry_ready = false;
                    continue;
                }
            }
        }

        if (!all_geometry_ready)
            return false;

        return BuildTemplate(nodes, options);
    }

    bool StaticMeshModelInstance::AttachMaterials(const Options &options)
    {
        (void)options;
        return world && root_entity.IsValid() && model_template_id != 0u;
    }

    bool StaticMeshModelInstance::Instantiate(
        ecs::ECSContext *ctx,
        const std::vector<graph::PrimitiveAsset> &assets,
        const std::vector<graph::StaticMeshNode> &nodes,
        const std::vector<int32_t> &root_nodes,
        const Options &options)
    {
        SkeletonOptions skeleton_options;
        skeleton_options.mobility = options.mobility;
        if (!InstantiateSkeleton(ctx, nodes, root_nodes, skeleton_options))
            return false;

        if (!AttachGeometries(assets, nodes, options))
        {
            FLogError(u8"Instantiate: GPU 模型模板未能构建");
            Release();
            return false;
        }

        if (!AttachMaterials(options))
        {
            FLogError(u8"Instantiate: 模型模板材质绑定未完成");
            Release();
            return false;
        }

        FLogInfo(u8"Instantiate: 单根模型实例化完成（节点 %d，图元 %d，template=%u）",
                 source_node_count, attached_primitive_count, model_template_id);
        return true;
    }

    uint32_t StaticMeshModelInstance::ResolveMaterialRowIndex(
        const graph::PrimitiveAsset &asset,
        const Options &options,
        const uint32_t mount_order)
    {
        if (options.material_row_count == 0)
            return 0u;

        if (options.rows_by_material_index)
        {
            const int32_t material_index = asset.GetMaterialIndex();
            if (material_index >= 0
                && static_cast<uint32_t>(material_index) < options.material_row_count)
                return static_cast<uint32_t>(material_index);
        }

        return mount_order % options.material_row_count;
    }
}
