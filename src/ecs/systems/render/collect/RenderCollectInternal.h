#pragma once

/**
 * RenderCollectInternal.h —— 收集系统**跨实现单元**的内部辅助。
 *
 * `RenderCollectSystem` 的实现按职责拆到多个 TU。这里只登记跨实现单元共用的辅助：
 *
 *   · 实体/组件查询、实体枚举和材质事实解析；
 *   · 「primitive 类型 + 顶点格式」的采集口径（program 构建与几何级错误判据共用一份）；
 *   · 模板组 program 获取及材质配方解析、校验；
 *   · 阴影 caster 行未就绪时的重试降频常量。
 *
 * 其余辅助仍留在使用它们的 TU 内，保持内部链接（可内联、不扩大重编面）。
 * 本头文件是**实现细节**，不属于 `inc/` 公开接口。
 */

#include <hgl/ecs/core/Context.h>
#include <hgl/ecs/components/GeometryData.h>
#include <hgl/ecs/support/material/MaterialRuntimeTable.h>
#include <hgl/graph/GraphTypes.h>
#include <hgl/graph/geo/GeometryVertexFormat.h>
#include <cstdint>

namespace hgl::ecs
{
    class Entity;
    class MaterialVariantTable;

    namespace detail
    {
        namespace render_collect
        {
            /// 阴影 caster 行未就绪时的重试降频窗口：前 N 帧每帧 bump，之后每 M 帧一次。
            inline constexpr uint32_t kShadowRetryFullBumpFrames = 120;
            inline constexpr uint32_t kShadowRetryBumpPeriod     = 60;

            /// 实体名（nullptr ⇒ `"<null-entity>"`）
            const char *GetOwnerName(const Entity *entity);

            MaterialData *FindMaterialDataOf(Entity *entity);

            /// 实体的几何资产组件（无实体/无世界/未挂载 ⇒ nullptr）
            GeometryData *FindGeometryOf(Entity *entity);

            /// 世界级运行期材质表（无世界 ⇒ nullptr）
            MaterialRuntimeTable *GetRuntimeTable(ECSContext *world);

            uint32_t MaterialAuthoredGenerationOf(Entity *entity);
            extern uint32_t g_fallback_recipe_builds;
            MaterialVariantTable *GetVariantTable(ECSContext *world);
            graph::ShaderProgram *GetForwardProgram(MaterialVariantTable *table,
                                                    const MaterialRuntimeRow *row);
            const AnsiString &ResolveAmbientProviderModule(ECSContext *world);
            uint64_t HashAmbientProviderModule(const AnsiString &name);

            void CollectPrimitiveEntities(ECSContext *world, ValueArray<Entity *> &entities);

            void ResolveMaterialFacts(ECSContext *world, EntityID entity_id,
                                      const MaterialRuntimeRow *row,
                                      uint32_t &material_id, uint32_t &texture_id);

            graph::ShaderProgram *EnsureTemplateGroupProgram(
                ECSContext *world, MaterialVariantTable *variant_table,
                MaterialRuntimeTable *runtime_table, MaterialRuntimeRowID row_id,
                bool shadow_pass, graph::PrimitiveType primitive_type,
                const graph::GeometryVertexFormat *vertex_format);

            bool ResolveResolvedMaterial(const graph::mtl::MaterialRecipe &recipe,
                                         graph::mtl::ResolvedMaterial &definition);
            bool ValidateMaterialRecipeForRuntime(const graph::mtl::MaterialRecipe &recipe,
                                                  const graph::ShaderProgram *program,
                                                  const graph::mtl::ResolvedMaterial &definition,
                                                  const char *owner_name);

            /// program 构建与几何级错误判据共用的采集口径（无 asset ⇒ Triangles + 空格式）
            void GetPrimitiveProgramBuildInputs(
                Entity *primitive_comp,
                graph::PrimitiveType &out_primitive_type,
                const graph::GeometryVertexFormat *&out_geometry_vertex_format);
        }//namespace render_collect
    }//namespace detail
}//namespace hgl::ecs
