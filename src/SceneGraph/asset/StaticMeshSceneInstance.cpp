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
        owned_assets.Clear();
        world = nullptr;
    }

    bool StaticMeshSceneInstance::Instantiate(ecs::ECSContext                  *ctx,
                                              const std::vector<PrimitiveAsset> &assets,
                                              const std::vector<StaticMeshNode> &nodes,
                                              const std::vector<int32_t>        &root_nodes,
                                              const Options                     &options)
    {
        if (world)
        {
            FLogWarning(u8"Instantiate: 实例已存在，请先 Release()");
            return false;
        }
        if (!ctx)
        {
            FLogError(u8"Instantiate: ECSContext 为空");
            return false;
        }

        world = ctx;
        node_records.assign(nodes.size(), NodeRecord{});
        (void)root_nodes; // 层级完全由 parentIndex 决定；根集合仅作入参兼容

        // 1) 资产复制进自持容器：逐项堆分配，地址稳定，vector 搬迁不再造成悬垂。
        for (const PrimitiveAsset &source : assets)
            owned_assets.Create(source);
        if (owned_assets.GetCount() != static_cast<int>(assets.size()))
        {
            FLogError(u8"Instantiate: 资产副本分配失败");
            Release();
            return false;
        }

        // 2) 每节点一个实体 + Transform：局部 TRS 直取节点真源字段。
        //    先按 Movable 建行写入，再切到目标移动性，避开 Static 行
        //    「渲染消费后再写」的 D4 一次性告警路径。
        for (size_t i = 0; i < nodes.size(); ++i)
        {
            const StaticMeshNode &node = nodes[i];

            ecs::Entity *entity = world->CreateEntity<ecs::Entity>(
                node.name.empty() ? "SceneNode" : node.name.c_str());
            if (!entity)
            {
                FLogError(u8"Instantiate: 节点实体创建失败（index=%zu）", i);
                Release();
                return false;
            }
            NodeRecord &record = node_records[i];
            record.entity = entity->GetEntityID();

            ecs::TransformID transform_id =
                world->CreateTransform(record.entity, ecs::Mobility::Movable);
            if (!ecs::IsValidTransformID(transform_id))
            {
                FLogError(u8"Instantiate: 节点 Transform 创建失败（index=%zu）", i);
                Release();
                return false;
            }
            record.transform = transform_id;

            ecs::TransformAccessor transform = world->GetTransform(transform_id);
            if (!transform.IsValid())
            {
                FLogError(u8"Instantiate: 节点 Transform 句柄无效（index=%zu）", i);
                Release();
                return false;
            }
            if (node.hasTRS)
                transform.SetLocalTRS(node.translation, node.rotation, node.scale);
            else
                transform.SetLocalTRS(kUnitPos, kUnitRot, kUnitScale);
            transform.SetMobility(options.mobility);
        }

        // 3) 父子层级：按 parentIndex 还原（加载器保证父先于子）。
        for (size_t i = 0; i < nodes.size(); ++i)
        {
            const int32_t parent = nodes[i].parentIndex;
            if (parent < 0)
                continue;
            if (parent >= static_cast<int32_t>(nodes.size()))
            {
                FLogError(u8"Instantiate: 父下标越界（node=%zu, parent=%d）", i, parent);
                Release();
                return false;
            }
            if (!ecs::IsValidTransformID(node_records[i].transform)
                || !ecs::IsValidTransformID(node_records[parent].transform))
            {
                FLogError(u8"Instantiate: 父/子 Transform 缺失（node=%zu）", i);
                Release();
                return false;
            }
            ecs::TransformAccessor child = world->GetTransform(node_records[i].transform);
            child.SetParent(node_records[parent].transform);
        }

        // 4) 图元挂载：首个进节点实体，其余派生单位变换子实体
        uint32_t row_counter = 0;
        uint32_t primitive_seq = 0;
        for (size_t i = 0; i < nodes.size(); ++i)
        {
            const StaticMeshNode &node = nodes[i];
            bool first = true;
            for (int32_t primitive_index : node.primitiveIndices)
            {
                if (primitive_index < 0
                    || primitive_index >= static_cast<int32_t>(owned_assets.GetCount()))
                {
                    FLogWarning(u8"Instantiate: 图元下标越界被跳过（node=%zu, pi=%d）",
                                i, primitive_index);
                    continue;
                }
                PrimitiveAsset *asset = owned_assets.At(primitive_index);
                if (!asset || !asset->IsValid())
                    continue;

                ecs::Entity *entity = nullptr;
                if (first)
                {
                    entity = world->GetEntity(node_records[i].entity);
                }
                else
                {
                    // 一实体一 GeometryData：额外图元挂到单位变换子实体上，
                    // 子实体置于节点之下，保持正确的世界变换。
                    AnsiString child_name =
                        AnsiString("SceneNodePrim_")
                        + AnsiString::numberOf(static_cast<unsigned>(primitive_seq));
                    ecs::Entity *child_entity =
                        world->CreateEntity<ecs::Entity>(child_name.c_str());
                    if (!child_entity)
                    {
                        FLogError(u8"Instantiate: 派生图元实体创建失败（node=%zu）", i);
                        Release();
                        return false;
                    }
                    ecs::TransformID child_transform = world->CreateTransform(
                        child_entity->GetEntityID(), ecs::Mobility::Movable);
                    if (!ecs::IsValidTransformID(child_transform))
                    {
                        FLogError(u8"Instantiate: 派生图元 Transform 创建失败（node=%zu）", i);
                        Release();
                        return false;
                    }
                    ecs::TransformAccessor child_accessor =
                        world->GetTransform(child_transform);
                    child_accessor.SetLocalTRS(kUnitPos, kUnitRot, kUnitScale);
                    child_accessor.SetParent(node_records[i].transform);
                    child_accessor.SetMobility(options.mobility);
                    extra_entities.push_back(child_entity->GetEntityID());
                    entity = child_entity;
                }
                ++primitive_seq;

                if (!entity)
                {
                    FLogError(u8"Instantiate: 节点实体句柄丢失（node=%zu）", i);
                    Release();
                    return false;
                }

                ecs::GeometryData *geometry_data =
                    world->GetOrCreateGeometryData(entity);
                if (!geometry_data)
                {
                    FLogError(u8"Instantiate: GeometryData 创建失败（node=%zu）", i);
                    Release();
                    return false;
                }
                geometry_data->SetPrimitiveAsset(asset);
                geometry_data->SetPrimitiveVariantIndex(0);

                if (options.material_rows && options.material_row_count > 0)
                {
                    // 多材质模式优先按图元的源材质下标选行；
                    // 下标缺失/越界回退到挂载顺序轮转（既有行为）。
                    ecs::MaterialData::MaterialDataAuthoringResource resource{};
                    resource = options.material_rows[
                        ResolveMaterialRowIndex(*asset, options, row_counter)];
                    ecs::MaterialData *material_data =
                        world->GetOrCreateMaterialData(entity);
                    if (!material_data || !material_data->SetDataResource(resource))
                    {
                        FLogError(u8"Instantiate: 材质行绑定失败（node=%zu）", i);
                        Release();
                        return false;
                    }
                    ++row_counter;
                }

                first = false;
            }
        }

        FLogInfo(u8"Instantiate: 场景实例化完成（节点 %zu，派生图元实体 %zu，资产副本 %d）",
                 nodes.size(), extra_entities.size(), owned_assets.GetCount());
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
