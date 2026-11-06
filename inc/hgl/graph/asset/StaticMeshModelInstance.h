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
    namespace ecs
    {
        class ECSContext;
    }

    namespace graph
    {
        /**
         * @brief 把 `LoadModelAsPrimitiveAssets` /
         * `LoadModelFromMemory` 的场景产物实例化进 ECS 世界。
         *
         * 与 `LevelLoader::LevelInstance`（spec 场景）对称，这一条通路服务
         * `.model` MiniPack 场景。实例化规则：
         *
         * - **每个节点一个实体 + Transform**：局部 TRS 直取 `.model` 的
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
         *
         * **渐进激活**：场景走异步流送时，节点拓扑、几何、材质是三个不同时刻
         * 才到位的东西。`Instantiate()` 是一次性三合一（同步通路继续用它），
         * 需要渐进时按顺序单独调用：
         *
         * | 时刻 | 调用 | 世界侧效果 |
         * | --- | --- | --- |
         * | 发起加载后立刻 | `InstantiateSkeleton()` | 空壳实体 + Transform 级联 + 骨架包围盒 |
         * | 图元几何到位 | `AttachGeometries()` | 逐个图元挂 `GeometryData` |
         * | 材质行到位 | `AttachMaterials()` | 为已挂几何补 `MaterialData` |
         *
         * 三段都幂等：重复调用只挂此前缺的部分，已挂的不动。中间态（只有骨架、
         * 有几何无材质）必须是可渲染的合法状态 —— 渲染侧靠占位纹理保底。
         */
        class StaticMeshModelInstance
        {
        public:
            struct Options
            {
                /// 变换行的最终移动性（默认 Static：`.model` 场景节点通常不动）
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

            /// 骨架阶段的可选项（此时还没有图元，`Options` 里的材质项无从谈起）
            struct SkeletonOptions
            {
                ecs::Mobility mobility = ecs::Mobility::Static;
            };

        private:
            struct NodeRecord
            {
                ecs::EntityID    entity   = ecs::EntityID::Invalid();
                ecs::TransformID transform = ecs::INVALID_TRANSFORM_ID;
            };

            /// 已挂上 GeometryData 的图元 —— `AttachMaterials` 按这份顺序补材质，
            /// 挂载序号（轮转模式的行下标）就是这里的位置。
            struct PrimitiveRecord
            {
                int32_t       asset_index = -1;
                ecs::EntityID entity      = ecs::EntityID::Invalid();
            };

            ecs::ECSContext *world = nullptr;
            ManagedArray<PrimitiveAsset> owned_assets;   ///< 稳定指针的资产副本（逐项 new）
            std::vector<NodeRecord> node_records;        ///< 与入参 nodes 下标一一对应
            std::vector<ecs::EntityID> extra_entities;   ///< 多图元节点派生的子实体
            std::vector<PrimitiveRecord> primitive_records; ///< 已挂几何的图元（挂载顺序）
            std::vector<int32_t> attached_per_node;      ///< 每个节点已处理到的 primitiveIndices 位置

            BoundingVolumes skeleton_bounds;             ///< 骨架阶段算出的整体包围盒
            bool            skeleton_bounds_valid = false;
            ecs::Mobility   skeleton_mobility = ecs::Mobility::Static; ///< 派生图元子实体沿用

        public:
            StaticMeshModelInstance() = default;
            ~StaticMeshModelInstance();

            StaticMeshModelInstance(const StaticMeshModelInstance &) = delete;
            StaticMeshModelInstance &operator=(const StaticMeshModelInstance &) = delete;

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

            /**
             * @brief 渐进激活第一段：只建节点实体 + Transform 级联，不挂图元/材质。
             *
             * 实体先入世界，相机即可凭 `GetSkeletonBounds()` 自动对焦 —— 画面立刻
             * 有正确的空场景（而非白屏），几何/材质随后热更新补上。
             * @note 成功后 `IsInstantiated()` 为真但 `GetAttachedPrimitiveCount()` 为 0。
             */
            bool InstantiateSkeleton(ecs::ECSContext                  *world,
                                     const std::vector<StaticMeshNode> &nodes,
                                     const std::vector<int32_t>        &root_nodes,
                                     const SkeletonOptions             &options = {});

            /**
             * @brief 渐进激活第二段：为已到位的图元挂 `GeometryData`。
             *
             * 每节点按 `primitiveIndices` 顺序推进，遇到资产副本尚未落地
             * （下标 ≥ assets.size() 或 `!IsValid()`）的图元就停在该节点，
             * 下次调用再接着推进 —— 因此 `assets` 可以是逐步变长的快照。
             * @return 任一新挂上即 true；全部无可挂（或世界未就绪）返回 false。
             */
            bool AttachGeometries(const std::vector<PrimitiveAsset> &assets,
                                  const std::vector<StaticMeshNode> &nodes);

            /**
             * @brief 渐进激活第三段：为已挂几何的图元补 `MaterialData`。
             * @note 需先 `AttachGeometries()` 建出图元顺序；重复调用只补空的那部分。
             *       材质下标取自实例自持的资产副本，因此无需再传 `assets`。
             */
            bool AttachMaterials(const Options &options = {});

            bool IsInstantiated() const { return world != nullptr; }

            /// 已挂上几何的图元数（0 表示仍是空壳）
            int GetAttachedPrimitiveCount() const
            {
                return static_cast<int>(primitive_records.size());
            }

            /**
             * @brief 骨架阶段的整体包围盒。
             *
             * 由各节点的 `nodeBounds` 沿 `worldMatrix` 变换累积而来 —— `.model`
             * 的 pack 里带着节点包围盒，所以**不依赖几何是否已上传**，
             * 空壳阶段就能给出正确的对焦范围。
             * @return 无任何有效节点包围盒时返回 false（`out` 被清空）
             */
            bool GetSkeletonBounds(BoundingVolumes &out) const
            {
                if (!skeleton_bounds_valid)
                {
                    out.Clear();
                    return false;
                }
                out = skeleton_bounds;
                return true;
            }

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
