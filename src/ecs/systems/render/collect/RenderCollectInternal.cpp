#include "RenderCollectInternal.h"

#include <hgl/ecs/core/Context.h>
#include <hgl/ecs/components/GeometryData.h>
#include <hgl/ecs/components/GPUModelRefComponent.h>
#include <hgl/graph/asset/PrimitiveAsset.h>
#include <hgl/graph/geo/VKGeometry.h>

namespace hgl::ecs
{
    namespace detail
    {
        namespace render_collect
        {
            const char *GetOwnerName(const Entity *entity)
            {
                if (!entity)
                    return "<null-entity>";

                return entity->GetName().c_str();
            }

            GeometryData *FindGeometryOf(Entity *entity)
            {
                ECSContext *context = entity ? entity->GetContextAs<hgl::ecs::ECSContext>() : nullptr;

                return context ? context->GetGeometryData(entity) : nullptr;
            }

            MaterialData *FindMaterialDataOf(Entity *entity)
            {
                ECSContext *context = entity ? entity->GetContextAs<ECSContext>() : nullptr;
                return context ? context->GetMaterialData(entity) : nullptr;
            }

            MaterialRuntimeTable *GetRuntimeTable(ECSContext *world)
            {
                return world ? world->RenderData().RuntimeMaterials() : nullptr;
            }

            void CollectPrimitiveEntities(ECSContext *world, ValueArray<Entity *> &entities)
            {
                ValueArray<GeometryData *> geometry_rows;
                world->GetComponents<GeometryData>(geometry_rows);
                for (GeometryData *geometry : geometry_rows)
                    if (geometry && geometry->GetOwner())
                        entities.Add(geometry->GetOwner());

                ValueArray<EntityID> carrier_ids;
                world->ForEachAliveEntity([&carrier_ids](const Entity *alive)
                {
                    if (alive && !alive->GetComponent<GeometryData>()
                     && alive->GetComponent<GPUModelRefComponent>())
                        carrier_ids.Add(alive->GetEntityID());
                });
                for (EntityID id : carrier_ids)
                    if (Entity *carrier = world->GetEntity(id))
                        entities.Add(carrier);
            }

            void ResolveMaterialFacts(ECSContext *world, EntityID entity_id,
                                      const MaterialRuntimeRow *row,
                                      uint32_t &material_id, uint32_t &texture_id)
            {
                material_id = row && row->data_index_row != uint32_t(-1) ? row->data_index_row : 0u;
                texture_id = row && row->material_texture_configuration.IsValid()
                    ? row->material_texture_configuration.row_index : 0u;
                const MaterialData *data = world ? world->GetMaterialData(entity_id) : nullptr;
                if (data && data->has_data_resource && data->data_resource.data_index != uint32_t(-1))
                    material_id = data->data_resource.data_index;
            }

            // program 解析（forward 与 ShadowCaster 双槽）共用的构建上下文输入：
            // primitive 类型与顶点格式来自 asset（无 asset 时 Triangles + 空格式）。
            void GetPrimitiveProgramBuildInputs(
                Entity *primitive_comp,
                graph::PrimitiveType &out_primitive_type,
                const graph::GeometryVertexFormat *&out_geometry_vertex_format)
            {
                out_primitive_type = graph::PrimitiveType::Triangles;
                out_geometry_vertex_format = nullptr;

                const GeometryData *geometry = FindGeometryOf(primitive_comp);
                if (const auto *asset = geometry ? geometry->GetPrimitiveAsset() : nullptr)
                {
                    if (auto *asset_geometry = asset->GetGeometry())
                        out_geometry_vertex_format =
                            &asset_geometry->GetGeometryVertexFormat();
                    out_primitive_type = asset->GetPrimitiveType();
                }
            }
        }//namespace render_collect
    }//namespace detail
}//namespace hgl::ecs
