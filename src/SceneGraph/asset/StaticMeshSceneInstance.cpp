// StaticMeshSceneInstance.cpp —— `.scene` 场景 → ECS 实例化
//
// 数据流约定（见头文件说明）：节点局部 TRS 与 parentIndex 层级是唯一真源，
// 全程不做矩阵反分解；资产复制进实例自持的 ManagedArray 以保证指针稳定。

#include <hgl/graph/asset/StaticMeshSceneInstance.h>
#include <hgl/ecs/core/Context.h>
#include <hgl/ecs/components/GeometryData.h>
#include <hgl/ecs/components/MaterialData.h>
#include <hgl/ecs/support/transform/TransformAccessor.h>
#include <hgl/log/Log.h>
#include <glm/gtc/quaternion.hpp>

DEFINE_LOGGER_MODULE(StaticMeshScene)
USE_MODULE_LOGGER(StaticMeshScene)

namespace hgl::graph
{
    namespace
    {
        /// 单位变换（多图元派生子实体用）
        const glm::vec3  kUnitPos(0.0f, 0.0f, 0.0f);
        const glm::quat  kUnitRot(1.0f, 0.0f, 0.0f, 0.0f);
        const glm::vec3  kUnitScale(1.0f, 1.0f, 1.0f);
    }//anonymous namespace

    StaticMeshSceneInstance::~StaticMeshSceneInstance()
    {
        Release();
    }

    void StaticMeshSceneInstance::Release()
    {
        // 顺序敏感：先销毁实体（GeometryData 仍引用 owned_assets 里的副本），
        // 再释放资产副本。
        if (world)
        {
            for (ecs::EntityID entity : extra_entities)
                world->DestroyEntity(entity);
            for (const NodeRecord &record : node_records)
            {
                if (record.entity.IsValid())
                    world->DestroyEntity(record.entity);
            }
        }
        extra_entities.clear();
        node_records.clear();
        primitive_records.clear();
        attached_per_node.clear();
        owned_assets.Clear();
        skeleton_bounds.Clear();
        skeleton_bounds_valid = false;
        world = nullptr;
    }

    bool StaticMeshSceneInstance::InstantiateSkeleton(ecs::ECSContext                  *ctx,
                                                      const std::vector<StaticMeshNode> &nodes,
                                                      const std::vector<int32_t>        &root_nodes,
                                                      const SkeletonOptions             &options)
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

        world = ctx;
        node_records.assign(nodes.size(), NodeRecord{});
        attached_per_node.assign(nodes.size(), 0);
        skeleton_mobility = options.mobility;
        (void)root_nodes; // 层级完全由 parentIndex 决定；根集合仅作入参兼容

        // 1) 每节点一个实体 + Transform：局部 TRS 直取节点真源字段。
        //    先按 Movable 建行写入，再切到目标移动性，避开 Static 行
        //    「渲染消费后再写」的 D4 一次性告警路径。
        for (size_t i = 0; i < nodes.size(); ++i)
        {
            const StaticMeshNode &node = nodes[i];

            ecs::Entity *entity = world->CreateEntity<ecs::Entity>(
                node.name.empty() ? "SceneNode" : node.name.c_str());
            if (!entity)
            {
                FLogError(u8"InstantiateSkeleton: 节点实体创建失败（index=%zu）", i);
                Release();
                return false;
            }
            NodeRecord &record = node_records[i];
            record.entity = entity->GetEntityID();

            ecs::TransformID transform_id =
                world->CreateTransform(record.entity, ecs::Mobility::Movable);
            if (!ecs::IsValidTransformID(transform_id))
            {
                FLogError(u8"InstantiateSkeleton: 节点 Transform 创建失败（index=%zu）", i);
                Release();
                return false;
            }
            record.transform = transform_id;

            ecs::TransformAccessor transform = world->GetTransform(transform_id);
            if (!transform.IsValid())
            {
                FLogError(u8"InstantiateSkeleton: 节点 Transform 句柄无效（index=%zu）", i);
                Release();
                return false;
            }
            if (node.hasTRS)
                transform.SetLocalTRS(node.translation, node.rotation, node.scale);
            else
                transform.SetLocalTRS(kUnitPos, kUnitRot, kUnitScale);
            transform.SetMobility(options.mobility);
        }

        // 2) 父子层级：按 parentIndex 还原（加载器保证父先于子）。
        for (size_t i = 0; i < nodes.size(); ++i)
        {
            const int32_t parent = nodes[i].parentIndex;
            if (parent < 0)
                continue;
            if (parent >= static_cast<int32_t>(nodes.size()))
            {
                FLogError(u8"InstantiateSkeleton: 父下标越界（node=%zu, parent=%d）", i, parent);
                Release();
                return false;
            }
            if (!ecs::IsValidTransformID(node_records[i].transform)
                || !ecs::IsValidTransformID(node_records[parent].transform))
            {
                FLogError(u8"InstantiateSkeleton: 父/子 Transform 缺失（node=%zu）", i);
                Release();
                return false;
            }
            ecs::TransformAccessor child = world->GetTransform(node_records[i].transform);
            child.SetParent(node_records[parent].transform);
        }

        // 3) 骨架包围盒：节点包围盒是 pack 里的真源，与几何是否上传无关，
        //    所以空壳阶段就能给出正确对焦范围（不然要等第一个图元落地才敢定位相机）。
        AABB merged_box;
        merged_box.Clear();
        bool any_box = false;
        for (const StaticMeshNode &node : nodes)
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
        }

        FLogInfo(u8"InstantiateSkeleton: 骨架就绪（节点 %zu，包围盒 %s）",
                 nodes.size(), any_box ? "有效" : "缺失");
        return true;
    }

    bool StaticMeshSceneInstance::AttachGeometries(const std::vector<PrimitiveAsset> &assets,
                                                    const std::vector<StaticMeshNode> &nodes)
    {
        if (!world)
        {
            FLogError(u8"AttachGeometries: 世界未就绪，请先 InstantiateSkeleton()");
            return false;
        }
        if (node_records.size() != nodes.size())
        {
            FLogError(u8"AttachGeometries: 节点数与骨架不一致（骨架 %zu / 入参 %zu）",
                      node_records.size(), nodes.size());
            return false;
        }
        if (attached_per_node.size() != nodes.size())
            attached_per_node.assign(nodes.size(), 0);

        // 资产副本按需追加：ManagedArray 追加不移动既有元素，指针保持稳定。
        for (int i = owned_assets.GetCount(); i < static_cast<int>(assets.size()); ++i)
        {
            if (!owned_assets.Create(assets[i]))
            {
                FLogError(u8"AttachGeometries: 资产副本分配失败（index=%d）", i);
                return false;
            }
        }

        bool attached_any = false;

        for (size_t i = 0; i < nodes.size(); ++i)
        {
            const StaticMeshNode &node = nodes[i];
            const int pi_count = static_cast<int>(node.primitiveIndices.size());

            for (int pos = attached_per_node[i]; pos < pi_count; ++pos)
            {
                const int32_t primitive_index = node.primitiveIndices[pos];

                if (primitive_index < 0
                    || primitive_index >= owned_assets.GetCount())
                {
                    // 资产还没到（流送中是常态）：本节点停在这里，下次接着来。
                    break;
                }

                PrimitiveAsset *asset = owned_assets.At(primitive_index);
                if (!asset || !asset->IsValid())
                    break;

                ecs::Entity *entity = world->GetEntity(node_records[i].entity);

                if (entity && world->GetGeometryData(entity))
                {
                    // 节点实体已被首图元占用 ⇒ 后续图元派生子实体。
                    entity = nullptr;
                }

                if (!entity)
                {
                    // 一实体一 GeometryData：额外图元挂到单位变换子实体上，
                    // 子实体置于节点之下，保持正确的世界变换。
                    AnsiString child_name =
                        AnsiString("SceneNodePrim_")
                        + AnsiString::numberOf(static_cast<unsigned>(primitive_records.size()));
                    ecs::Entity *child_entity =
                        world->CreateEntity<ecs::Entity>(child_name.c_str());
                    if (!child_entity)
                    {
                        FLogError(u8"AttachGeometries: 派生图元实体创建失败（node=%zu）", i);
                        return false;
                    }
                    ecs::TransformID child_transform = world->CreateTransform(
                        child_entity->GetEntityID(), ecs::Mobility::Movable);
                    if (!ecs::IsValidTransformID(child_transform))
                    {
                        FLogError(u8"AttachGeometries: 派生图元 Transform 创建失败（node=%zu）", i);
                        return false;
                    }
                    ecs::TransformAccessor child_accessor =
                        world->GetTransform(child_transform);
                    child_accessor.SetLocalTRS(kUnitPos, kUnitRot, kUnitScale);
                    child_accessor.SetParent(node_records[i].transform);
                    child_accessor.SetMobility(skeleton_mobility);
                    extra_entities.push_back(child_entity->GetEntityID());
                    entity = child_entity;
                }

                ecs::GeometryData *geometry_data =
                    world->GetOrCreateGeometryData(entity);
                if (!geometry_data)
                {
                    FLogError(u8"AttachGeometries: GeometryData 创建失败（node=%zu）", i);
                    return false;
                }
                geometry_data->SetPrimitiveAsset(asset);
                geometry_data->SetPrimitiveVariantIndex(0);

                PrimitiveRecord record;
                record.asset_index = primitive_index;
                record.entity      = entity->GetEntityID();
                primitive_records.push_back(record);

                attached_per_node[i] = pos + 1;
                attached_any = true;
            }
        }

        if (attached_any)
        {
            FLogInfo(u8"AttachGeometries: 已挂几何 %d 个（派生实体 %zu）",
                     GetAttachedPrimitiveCount(), extra_entities.size());
        }
        return attached_any;
    }

    bool StaticMeshSceneInstance::AttachMaterials(const Options &options)
    {
        if (!world)
        {
            FLogError(u8"AttachMaterials: 世界未就绪，请先 InstantiateSkeleton()");
            return false;
        }
        if (!options.material_rows || options.material_row_count == 0)
            return false;

        bool attached_any = false;

        for (size_t k = 0; k < primitive_records.size(); ++k)
        {
            const PrimitiveRecord &record = primitive_records[k];
            if (!record.entity.IsValid())
                continue;

            ecs::Entity *entity = world->GetEntity(record.entity);
            if (!entity)
                continue;

            // 幂等：已绑过材质的图元不再重绑（重启流送时也不会把行号打乱）。
            if (world->GetMaterialData(entity))
                continue;

            const PrimitiveAsset *asset = owned_assets.At(record.asset_index);
            if (!asset)
                continue;

            const uint32_t row = ResolveMaterialRowIndex(
                *asset, options, static_cast<uint32_t>(k));

            ecs::MaterialData::MaterialDataAuthoringResource resource{};
            resource = options.material_rows[row];

            ecs::MaterialData *material_data = world->GetOrCreateMaterialData(entity);
            if (!material_data || !material_data->SetDataResource(resource))
            {
                FLogError(u8"AttachMaterials: 材质行绑定失败（图元 #%zu, row=%u）", k, row);
                return false;
            }
            attached_any = true;
        }

        if (attached_any)
            FLogInfo(u8"AttachMaterials: 已挂材质 %d 个", GetAttachedPrimitiveCount());
        return attached_any;
    }

    bool StaticMeshSceneInstance::Instantiate(ecs::ECSContext                  *ctx,
                                              const std::vector<PrimitiveAsset> &assets,
                                              const std::vector<StaticMeshNode> &nodes,
                                              const std::vector<int32_t>        &root_nodes,
                                              const Options                     &options)
    {
        SkeletonOptions skeleton_options;
        skeleton_options.mobility = options.mobility;

        if (!InstantiateSkeleton(ctx, nodes, root_nodes, skeleton_options))
            return false;

        if (!AttachGeometries(assets, nodes))
        {
            // 同步通路里「一个图元都没挂上」等同于失败：渐进通路才允许空壳停留。
            if (GetAttachedPrimitiveCount() <= 0 && !assets.empty())
            {
                FLogError(u8"Instantiate: 没有任何图元被挂载");
                Release();
                return false;
            }
        }

        if (options.material_rows && options.material_row_count > 0)
        {
            if (!AttachMaterials(options))
            {
                FLogError(u8"Instantiate: 材质绑定失败");
                Release();
                return false;
            }
        }

        FLogInfo(u8"Instantiate: 场景实例化完成（节点 %zu，图元 %d，派生图元实体 %zu，资产副本 %d）",
                 nodes.size(), GetAttachedPrimitiveCount(),
                 extra_entities.size(), owned_assets.GetCount());
        return true;
    }

    uint32_t StaticMeshSceneInstance::ResolveMaterialRowIndex(const PrimitiveAsset &asset,
                                                              const Options &options,
                                                              uint32_t mount_order)
    {
        if (options.material_row_count == 0)
            return 0;

        if (options.rows_by_material_index)
        {
            const int32_t mat_index = asset.GetMaterialIndex();
            if (mat_index >= 0
                && static_cast<uint32_t>(mat_index) < options.material_row_count)
                return static_cast<uint32_t>(mat_index);
        }

        return mount_order % options.material_row_count;
    }

}//namespace hgl::graph
