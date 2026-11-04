#pragma once

#include <hgl/ecs/core/entity/EntityHandle.h>
#include <hgl/ecs/support/transform/TransformID.h>
#include <hgl/graph/asset/PrimitiveAsset.h>
#include <hgl/graph/mesh/StaticMesh.h>
#include <hgl/graph/ssbo/GlobalSSBOTypes.h>
#include <hgl/type/ManagedArray.h>
#include <vector>
#include <cstdint>

namespace hgl
{
    namespace ecs
    {
        class ECSContext;
    }

    namespace graph
    {
        /**
         * @brief 把 `LoadStaticMeshSceneAsPrimitiveAssets` /
         * `LoadStaticMeshSceneFromMemory` 的场景产物实例化进 ECS 世界。
         *
         * 与 `SceneLoader::SceneInstance`（spec 场景）对称，这一条通路服务
         * `.scene` MiniPack 场景。实例化规则：
         *
         * - **每个节点一个实体 + Transform**：局部 TRS 直取 `.scene` 的
         *   TRSTable 真源（`node.hasTRS` 时），不做任何矩阵反分解；
         *   父子层级按 `parentIndex` 用 `SetParent` 完整还原。
         * - **图元挂载**：节点的首个有效图元直接挂在节点实体上；
         *   其余图元创建单位变换子实体挂在节点实体之下（一实体一 GeometryData）。
         * - **资产安全**：`assets` 被逐个复制进实例自持的 `ManagedArray`
         *   （堆上单对象，地址稳定），`GeometryData` 只引用实例持有的副本 ——
         *   调用方装载完即可丢弃自己的容器，不存在 vector 搬迁悬垂。
         *
         * 生命周期：`Release()`（或析构）先销毁实体、后释放资产副本；
         * 在实例存续期间不得销毁其引用的 Geometry/Recipe 等底层资源。
         */
        class StaticMeshSceneInstance
        {
        public:
            struct Options
            {
                /// 变换行的最终移动性（默认 Static：`.scene` 场景节点通常不动）
                ecs::Mobility mobility = ecs::Mobility::Static;
                /// 材质数据 SSBO 行（可空；非空时按图元挂载顺序轮转绑定）
                const GlobalSSBOBinding *material_rows = nullptr;
                uint32_t                 material_row_count = 0;
                /**
                 * @brief 多材质模式：`material_rows` 按**材质下标**索引
                 * （行 i = 源场景 materials[i] 的数据行），图元用其
                 * `PrimitiveAsset::GetMaterialIndex()` 选行；下标缺失或
                 * 越界时回退到挂载顺序轮转（既有行为）。
                 */
                bool rows_by_material_index = false;
            };

        private:
            struct NodeRecord
            {
                ecs::EntityID    entity   = ecs::EntityID::Invalid();
                ecs::TransformID transform = ecs::INVALID_TRANSFORM_ID;
            };

            ecs::ECSContext *world = nullptr;
            ManagedArray<PrimitiveAsset> owned_assets;   ///< 稳定指针的资产副本（逐项 new）
            std::vector<NodeRecord> node_records;        ///< 与入参 nodes 下标一一对应
            std::vector<ecs::EntityID> extra_entities;   ///< 多图元节点派生的子实体

        public:
            StaticMeshSceneInstance() = default;
            ~StaticMeshSceneInstance();

            StaticMeshSceneInstance(const StaticMeshSceneInstance &) = delete;
            StaticMeshSceneInstance &operator=(const StaticMeshSceneInstance &) = delete;

            /**
             * @brief 实例化整棵场景树；任何一步失败即回滚全部并返回 false。
             * @note 允许 `assets` 中含未声明条目（无效下标会被跳过并记日志），
             *       但节点必须至少有 Transform；世界池耗尽视为失败。
             */
            bool Instantiate(ecs::ECSContext               *world,
                             const std::vector<PrimitiveAsset> &assets,
                             const std::vector<StaticMeshNode> &nodes,
                             const std::vector<int32_t>        &root_nodes,
                             const Options                     &options = {});

            /// 销毁全部实体并释放资产副本（幂等）
            void Release();

            bool IsInstantiated() const { return world != nullptr; }

            int GetNodeCount() const { return static_cast<int>(node_records.size()); }

            ecs::EntityID GetNodeEntity(int node_index) const
            {
                return node_index >= 0 && node_index < static_cast<int>(node_records.size())
                     ? node_records[node_index].entity : ecs::EntityID::Invalid();
            }

            ecs::TransformID GetNodeTransform(int node_index) const
            {
                return node_index >= 0 && node_index < static_cast<int>(node_records.size())
                     ? node_records[node_index].transform : ecs::INVALID_TRANSFORM_ID;
            }

            /// 实例持有的资产副本（`GeometryData` 引用的即这些指针）
            const PrimitiveAsset *GetOwnedAsset(int asset_index) const
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
            static uint32_t ResolveMaterialRowIndex(const PrimitiveAsset &asset,
                                                    const Options &options,
                                                    uint32_t mount_order);
        };
    }//namespace graph
}//namespace hgl
