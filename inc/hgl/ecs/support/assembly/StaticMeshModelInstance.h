#pragma once

#include <hgl/ecs/core/entity/EntityHandle.h>
#include <hgl/ecs/support/transform/TransformID.h>
#include <hgl/graph/asset/PrimitiveAsset.h>
#include <hgl/graph/mesh/StaticMesh.h>
#include <hgl/graph/ssbo/GlobalSSBOTypes.h>
#include <hgl/math/geometry/BoundingVolumes.h>
#include <hgl/type/ManagedArray.h>
#include <vector>
#include <cstdint>

namespace hgl
{
    namespace graph
    {
        class PrimitiveAsset;
        struct StaticMeshNode;
    }

    namespace ecs
    {
        class ECSContext;
        class Entity;

        /**
         * @brief 把 `LoadModelAsPrimitiveAssets` /
         * `LoadModelFromMemory` 的场景产物实例化进 ECS 世界。
         *
         * 与 `LevelLoader::LevelInstance`（spec 场景）对称，这一条通路服务
         * `.model` MiniPack 场景。实例化规则：
         *
         * - **单根实体**：只保留 Gameplay 锚点 Entity + Transform；节点矩阵与图元
         *   直接打包进 GPU ModelTemplate/ModelPart 行。
         * - **零子节点实体**：CPU 不为模型节点、图元或 Meshlet 创建实体。
         * - **资产安全**：`assets` 被复制到实例自持的 `ManagedArray`，
         *   供模板构建阶段稳定访问 PrimitiveAsset 与 Geometry。
         *
         * 生命周期：`Release()`（或析构）先销毁实体、后释放资产副本；
         * 在实例存续期间不得销毁其引用的 Geometry/Recipe 等底层资源。
         *
         * **渐进激活**：场景走异步流送时，节点拓扑、几何、材质是三个不同时刻
         * 才到位的东西。`Instantiate()` 是同步通路的一次性 root + GPU 模板构建，
         * 需要渐进时按顺序单独调用：
         *
         * | 时刻 | 调用 | 世界侧效果 |
         * | --- | --- | --- |
         * | 发起加载后立刻 | `InstantiateSkeleton()` | 单根实体 + Transform + 模型包围盒 |
         * | 图元几何到位 | `AttachGeometries()` | 构建 Template/Part/Meshlet 表并挂 GPUModelRef |
         * | 材质行到位 | `AttachMaterials()` | 完成模板材质绑定状态 |
         *
         * 各段幂等；几何未全部到位时不创建半模板。
         */
        class StaticMeshModelInstance
        {
        public:
            struct Options
            {
                /// 变换行的最终移动性（默认 Static：`.model` 场景节点通常不动）
                ecs::Mobility mobility = ecs::Mobility::Static;
                /// 材质数据 SSBO 行（可空；非空时按图元挂载顺序轮转绑定）
                const graph::GlobalSSBOBinding *material_rows = nullptr;
                uint32_t                 material_row_count = 0;
                /**
                 * @brief 多材质模式：`material_rows` 按**材质下标**索引
                 * （行 i = 源场景 materials[i] 的数据行），图元用其
                 * `PrimitiveAsset::GetMaterialIndex()` 选行；下标缺失或
                 * 越界时回退到挂载顺序轮转（既有行为）。
                 */
                bool rows_by_material_index = false;
            };

            /// 骨架阶段的可选项（此时还没有图元，`Options` 里的材质项无从谈起）
            struct SkeletonOptions
            {
                ecs::Mobility mobility = ecs::Mobility::Static;
            };

        private:
            ecs::ECSContext *world = nullptr;
            ManagedArray<graph::PrimitiveAsset> owned_assets;   ///< 稳定指针的资产副本（逐项 new）
            ecs::EntityID root_entity = ecs::EntityID::Invalid();
            ecs::TransformID root_transform = ecs::INVALID_TRANSFORM_ID;
            uint32_t model_template_id = 0;
            int source_node_count = 0;
            int attached_primitive_count = 0;
            bool owns_root_entity = false;

            math::BoundingVolumes skeleton_bounds;             ///< 骨架阶段算出的整体包围盒
            bool            skeleton_bounds_valid = false;

            bool BuildTemplate(const std::vector<graph::StaticMeshNode> &nodes,
                               const Options &options);

        public:
            StaticMeshModelInstance() = default;
            ~StaticMeshModelInstance();

            StaticMeshModelInstance(const StaticMeshModelInstance &) = delete;
            StaticMeshModelInstance &operator=(const StaticMeshModelInstance &) = delete;

            /**
             * @brief 创建单根实体并把完整场景树打包为 GPU 模型模板。
             * @note 几何未就绪、材质运行期行无效或 GPU 池耗尽时整体失败。
             */
            bool Instantiate(ecs::ECSContext               *world,
                             const std::vector<graph::PrimitiveAsset> &assets,
                             const std::vector<graph::StaticMeshNode> &nodes,
                             const std::vector<int32_t>        &root_nodes,
                             const Options                     &options = {});

            /// 销毁全部实体并释放资产副本（幂等）
            void Release(bool root_entity_is_destroying = false);

            /**
             * @brief 渐进激活第一段：创建或复用唯一根实体，并预计算模型包围盒。
             * @note 可传入 ModelRefComponent 的 owner 作为 root，避免额外请求实体。
             */
            bool InstantiateSkeleton(ecs::ECSContext                  *world,
                                     const std::vector<graph::StaticMeshNode> &nodes,
                                     const std::vector<int32_t>        &root_nodes,
                                     const SkeletonOptions             &options = {},
                                     ecs::Entity                       *root_entity = nullptr);

            /**
             * @brief 几何全部就绪后构建 Template/Part/Meshlet 表，并绑定 GPUModelRef。
             * @return 模板本次建成返回 true；仍有几何流送时返回 false。
             */
            bool AttachGeometries(const std::vector<graph::PrimitiveAsset> &assets,
                                  const std::vector<graph::StaticMeshNode> &nodes,
                                  const Options &options = {});

            /// 模板构建时已把材质行装入 Part/绑定表；该入口用于装配阶段完成通知。
            bool AttachMaterials(const Options &options = {});

            bool IsInstantiated() const { return world != nullptr; }

            /// 已挂上几何的图元数（0 表示仍是空壳）
            int GetAttachedPrimitiveCount() const
            {
                return attached_primitive_count;
            }

            /**
             * @brief 骨架阶段的整体包围盒。
             *
             * 由各节点的 `nodeBounds` 沿 `worldMatrix` 变换累积而来 —— `.model`
             * 的 pack 里带着节点包围盒，所以**不依赖几何是否已上传**，
             * 空壳阶段就能给出正确的对焦范围。
             * @return 无任何有效节点包围盒时返回 false（`out` 被清空）
             */
            bool GetSkeletonBounds(math::BoundingVolumes &out) const
            {
                if (!skeleton_bounds_valid)
                {
                    out.Clear();
                    return false;
                }
                out = skeleton_bounds;
                return true;
            }

            int GetNodeCount() const { return source_node_count; }
            ecs::EntityID GetRootEntity() const { return root_entity; }
            ecs::TransformID GetRootTransform() const { return root_transform; }
            uint32_t GetModelTemplateID() const { return model_template_id; }

            /// 实例持有的资产副本（`GeometryData` 引用的即这些指针）
            const graph::PrimitiveAsset *GetOwnedAsset(int asset_index) const
            {
                return asset_index >= 0 && asset_index < owned_assets.GetCount()
                     ? owned_assets.At(asset_index) : nullptr;
            }

            /**
             * @brief 解析某个图元应绑定的材质数据行下标（纯查表，无副作用）。
             *
             * - `options.rows_by_material_index` 且图元带有有效材质下标
             *   （`GetMaterialIndex()` ∈ [0, material_row_count)）⇒ 行 = 材质下标；
             * - 否则回退为挂载顺序轮转（`mount_order % material_row_count`），
             *   即既有单材质行为。
             * @note `material_row_count == 0` 时返回 0（调用方应先判空跳过绑定）。
             */
            static uint32_t ResolveMaterialRowIndex(const graph::PrimitiveAsset &asset,
                                                    const Options &options,
                                                    uint32_t mount_order);
        };
    }//namespace ecs
}//namespace hgl
