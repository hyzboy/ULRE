#include "RenderCollectInternal.h"

#include <hgl/ecs/core/Context.h>
#include <hgl/ecs/components/GeometryData.h>
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
                ECSContext *context = entity ? entity->GetContext() : nullptr;

                return context ? context->GetGeometryData(entity) : nullptr;
            }

            MaterialRuntimeTable *GetRuntimeTable(ECSContext *world)
            {
                return world ? world->GetMaterialRuntimeTable() : nullptr;
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
