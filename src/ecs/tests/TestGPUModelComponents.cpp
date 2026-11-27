#include <hgl/ecs/components/GPUModelRefComponent.h>
#include <hgl/ecs/components/GPUInstancedModelBatchComponent.h>
#include <hgl/ecs/components/GeometryData.h>
#include <hgl/ecs/components/ShadowProxy.h>
#include <hgl/ecs/core/Context.h>
#include <hgl/ecs/core/entity/Entity.h>
#include <hgl/ecs/support/storage/EntityGPUTable.h>
#include <hgl/ecs/support/material/MaterialRuntimePipelineTable.h>
#include <hgl/vk/buffer/RingLayout.h>
#include <hgl/log/Log.h>
#include <hgl/log/Logger.h>
#include <hgl/type/ValueArray.h>

/**
 * 判据（落地计划 T2.1/T2.2/T2.3）：GPU 模型引用组件与动静 TRS 接入约定。
 *
 * 本文件刻意**不碰设备**：组件全部是 CPU-only 行（gpu_row_bytes = 0），
 * 覆盖的是——
 *   1) GPUModelRefComponent 默认值与部件可见掩码位语义；
 *   2) GPUInstancedModelBatchComponent 的 CPU/GPU 双权威实例计数与脏标记；
 *   3) 两组件可正常挂载/枚举（类型表登记生效）；
 *   4) 动静 TRS 规范：模型实体以 Mobility::Static/Movable 建行，
 *      SetMobility 在段名单之间换边（组件自身不存 TRS ⇒ 与 Transform
 *      管线解耦，L2W 由既有求值链负责）。
 */

static int g_fail = 0;

#define EXPECT(cond)                                                            \
    do                                                                          \
    {                                                                           \
        if (!(cond))                                                            \
        {                                                                       \
            GLogError(u8"判据失败：%s（%s:%d）", #cond, __FILE__, __LINE__);    \
            ++g_fail;                                                           \
        }                                                                       \
    } while (0)

namespace
{
    using namespace hgl;
    using namespace hgl::ecs;

    class GPUResourcesTestSource final : public IWorldGPUResourcesSource
    {
        AnsiString name = "GPUResourcesTest";
        EntityGPUTable entities;
        EntityRenderIDTable render_ids;
        LightRowTable lights;
        graph::ShadowInfo shadow{};

    public:
        int graphics_queries = 0;
        graph::GraphicsContext *GPUResourcesGraphicsContext() override { ++graphics_queries; return nullptr; }
        graph::VulkanDevice *GPUResourcesDevice() override { return nullptr; }
        const AnsiString &GPUResourcesWorldName() const override { return name; }
        EntityGPUTable &GPUResourcesEntityTable() override { return entities; }
        EntityRenderIDTable &GPUResourcesRenderIDTable() override { return render_ids; }
        LightRowTable &GPUResourcesLightTable() override { return lights; }
        const graph::ShadowInfo &GPUResourcesShadowInfo() const override { return shadow; }
        CameraInfoStorage *GPUResourcesCameraStorage() override { return nullptr; }
        uint64_t GPUResourcesTransformAddress() override { return 0; }
        uint32_t GPUResourcesEnvironmentProfile() const override { return 0; }
        uint64_t GPUResourcesAmbientAddress(uint32_t) const override { return 0; }
        uint64_t GPUResourcesModelExpansionAddress() override { return 0; }
    };

    void TestGPUResourcesSourceBoundary()
    {
        GPUResourcesTestSource first;
        GPUResourcesTestSource second;
        WorldGPUResources resources(&first);
        EXPECT(!resources.InitializeWorldAddressesTable());
        EXPECT(first.graphics_queries == 1 && second.graphics_queries == 0);

        WorldGPUResources other_resources(&second);
        EXPECT(!other_resources.InitializeWorldAddressesTable());
        EXPECT(first.graphics_queries == 1 && second.graphics_queries == 1);
    }

    class RenderDataTestSource final : public IWorldRenderDataSource
    {
        AnsiString name = "RenderDataTest";

    public:
        int invalidations = 0;
        const AnsiString &RenderDataWorldName() const override { return name; }
        void RenderDataPipelineInvalidated() override { ++invalidations; }
    };

    void TestRenderDataSourceBoundary()
    {
        RenderDataTestSource source;
        int world_identity = 0;
        WorldRenderData data(&source, &world_identity);

        data.InvalidateEntityRuntimePipeline(EntityID{});
        EXPECT(source.invalidations == 1);
        auto *camera = data.EnsureFallbackCamera();
        EXPECT(camera != nullptr);
        EXPECT(camera && camera->world_owner == &world_identity);
        EXPECT(data.GetDefaultCamera() == camera);
        EXPECT(data.EnsureFallbackCamera() == camera);
    }

    // ── 判据 1：GPUModelRefComponent 默认值与掩码位语义 ────────────────────

    void TestModelRefDefaultsAndMask()
    {
        GPUModelRefComponent ref;

        EXPECT(ref.GetTemplateID() == 0u);
        EXPECT(ref.part_visibility_mask == 0xFFFFFFFFFFFFFFFFull);
        EXPECT(ref.custom_data_index == 0u);
        EXPECT(ref.prev_transform_id == 0u);

        ref.HidePart(0u);
        ref.HidePart(63u);
        EXPECT((ref.part_visibility_mask & 1ull) == 0ull);
        EXPECT((ref.part_visibility_mask & (1ull << 63)) == 0ull);
        EXPECT(ref.IsPartVisible(1u));
        EXPECT(!ref.IsPartVisible(0u));

        // 越界 index：掩码不生效（前 64 之外的部件恒可见）
        ref.HidePart(64u);
        ref.HidePart(1000u);
        EXPECT(ref.IsPartVisible(64u));
        EXPECT(ref.IsPartVisible(1000u));

        ref.ShowPart(0u);
        EXPECT(ref.IsPartVisible(0u));
        EXPECT(!ref.IsPartVisible(63u));

        ref.ShowAllParts();
        EXPECT(ref.part_visibility_mask == 0xFFFFFFFFFFFFFFFFull);
    }

    // ── 判据 2：批次组件的实例计数（CPU 权威 + GPU 权威优先）与脏标记 ──────

    void TestBatchInstanceAccounting()
    {
        GPUInstancedModelBatchComponent batch;

        EXPECT(batch.template_id == 0u);
        EXPECT(batch.GetInstanceCount() == 0u);
        EXPECT(batch.cpu_dirty);

        batch.AddMatrix(math::Matrix4f(1.0f));
        batch.AddMatrix(math::Matrix4f(2.0f));
        EXPECT(batch.GetInstanceCount() == 2u);

        batch.cpu_dirty = false;
        batch.SetMatrixAt(0u, math::Matrix4f(3.0f));
        EXPECT(batch.cpu_dirty);
        EXPECT(batch.cpu_matrices.GetData()[1] == math::Matrix4f(2.0f));

        // 越界写：不生效、不置脏
        batch.cpu_dirty = false;
        batch.SetMatrixAt(99u, math::Matrix4f(4.0f));
        EXPECT(!batch.cpu_dirty);

        // GPU 权威缓冲存在时优先于 cpu_matrices 计数
        batch.device_matrices = reinterpret_cast<graph::DeviceBuffer*>(1);
        batch.device_instance_count = 5u;
        EXPECT(batch.GetInstanceCount() == 5u);
        batch.device_matrices = nullptr;
        EXPECT(batch.GetInstanceCount() == 2u);

        batch.ClearMatrices();
        EXPECT(batch.GetInstanceCount() == 0u);
        EXPECT(batch.cpu_dirty);
    }

    // ── 判据 3：组件挂载与枚举（类型表登记生效）────────────────────────────

    void TestComponentMountAndEnumeration()
    {
        ECSContext ctx;

        Entity *e1 = ctx.CreateEntity<Entity>("ModelA");
        EXPECT(e1->GetContextAs<ECSContext>() == &ctx);
        ECSCoreContext core("CoreOnly");
        Entity *core_entity = core.CreateEntity();
        EXPECT(core_entity && !core_entity->GetContextAs<ECSContext>());
        auto *geometry = ctx.GetOrCreateGeometryData(e1);
        auto *material = ctx.GetOrCreateMaterialData(e1);
        EXPECT(geometry && material);
        const auto authored = material->GetAuthoredGeneration();
        geometry->SetPrimitiveVariantPurpose(graph::PrimitiveVariantPurpose::DepthOnly);
        EXPECT(material->GetAuthoredGeneration() == authored + 1);
        auto *shadow = e1->AddComponent<ShadowProxy>();
        EXPECT(shadow);
        const auto shadow_revision = ctx.GetStaticSceneRevision();
        shadow->SetMaxDistance(25.0f);
        EXPECT(ctx.GetStaticSceneRevision() == shadow_revision + 1);
        shadow->SetMaxDistance(25.0f);
        EXPECT(ctx.GetStaticSceneRevision() == shadow_revision + 1);
        geometry->SetPrimitiveVariantPurpose(graph::PrimitiveVariantPurpose::DepthOnly);
        EXPECT(material->GetAuthoredGeneration() == authored + 1);
        GPUModelRefComponent *ref = e1->AddComponent<GPUModelRefComponent>();
        EXPECT(ref != nullptr);
        EXPECT(ref->GetTemplateID() == 0u);
        const auto initial_template_revision = ctx.GetPrimitiveTopologyLedger().GetTopologyRevision(TopologyChange::ModelTemplate);
        ref->SetTemplateBinding(7u);
        EXPECT(ctx.GetPrimitiveTopologyLedger().GetTopologyRevision(TopologyChange::ModelTemplate) == initial_template_revision + 1);
        ref->SetTemplateBinding(7u);
        EXPECT(ctx.GetPrimitiveTopologyLedger().GetTopologyRevision(TopologyChange::ModelTemplate) == initial_template_revision + 1);
        ref->SetTemplateBinding(7u, true);
        EXPECT(ref->IsAutomaticallyNormalized());
        EXPECT(ctx.GetPrimitiveTopologyLedger().GetTopologyRevision(TopologyChange::ModelTemplate) == initial_template_revision + 2);

        Entity *e2 = ctx.CreateEntity<Entity>("BatchB");
        GPUInstancedModelBatchComponent *batch = e2->AddComponent<GPUInstancedModelBatchComponent>();
        EXPECT(batch != nullptr);
        batch->template_id = 9u;
        batch->AddMatrix(math::Matrix4f(1.0f));

        ValueArray<GPUModelRefComponent *> refs;
        ctx.GetComponents<GPUModelRefComponent>(refs);
        EXPECT(refs.GetCount() == 1);
        EXPECT(refs.GetData()[0]->GetTemplateID() == 7u);

        ValueArray<GPUInstancedModelBatchComponent *> batches;
        ctx.GetComponents<GPUInstancedModelBatchComponent>(batches);
        EXPECT(batches.GetCount() == 1);
        EXPECT(batches.GetData()[0]->template_id == 9u);
    }

    // ── 判据 4：动静 TRS 规范（T2.3）───────────────────────────────────────
    //   · Mobility::Static 建行 ⇒ 行号进静态段名单；初始化写 TRS 后不再标脏
    //   · Mobility::Movable 建行 ⇒ 进可动段名单
    //   · SetMobility 换段：旧段名单移除、新段名单加入（渲染上传按段分流）
    //   · 组件侧不存 TRS ⇒ GPUModelRefComponent 与变换行互不复制数据

    void TestStaticDynamicTransformConvention()
    {
        ECSContext ctx;

        Entity *stat = ctx.CreateEntity<Entity>("StaticModel");
        GPUModelRefComponent *sref = stat->AddComponent<GPUModelRefComponent>();
        sref->SetTemplateBinding(3u);
        const TransformID sid = ctx.CreateTransform(stat->GetEntityID(), Mobility::Static);
        TransformAccessor stf = ctx.GetTransformByEntity(stat->GetEntityID());
        EXPECT(stf.IsStatic());
        stf.SetLocalPosition(glm::vec3(10.0f, 0.0f, 2.0f));

        Entity *dyn = ctx.CreateEntity<Entity>("MovableModel");
        GPUModelRefComponent *dref = dyn->AddComponent<GPUModelRefComponent>();
        dref->SetTemplateBinding(4u);
        const TransformID did = ctx.CreateTransform(dyn->GetEntityID(), Mobility::Movable);
        TransformAccessor dtf = ctx.GetTransformByEntity(dyn->GetEntityID());
        EXPECT(dtf.IsMovable());

        // 段名单归属
        auto inList = [](const ValueArray<TransformID> &list, TransformID id)
        {
            for (const TransformID t : list) if (t == id) return true;
            return false;
        };
        EXPECT(inList(ctx.GetStaticTransforms(), sid));
        EXPECT(!inList(ctx.GetMovableTransforms(), sid));
        EXPECT(inList(ctx.GetMovableTransforms(), did));

        // 换段：静态 → 可动
        stf.SetMobility(Mobility::Movable);
        EXPECT(stf.IsMovable());
        EXPECT(!inList(ctx.GetStaticTransforms(), sid));
        EXPECT(inList(ctx.GetMovableTransforms(), sid));

        // 换段：可动 → 静态
        dtf.SetMobility(Mobility::Static);
        EXPECT(dtf.IsStatic());
        EXPECT(!inList(ctx.GetMovableTransforms(), did));
        EXPECT(inList(ctx.GetStaticTransforms(), did));

        // 世界矩阵求值不依赖组件字段（组件零 TRS 复制）
        const glm::mat4 m = stf.GetWorldMatrix();
        EXPECT(glm::distance(glm::vec3(m[3][0], m[3][1], m[3][2]),
                             glm::vec3(10.0f, 0.0f, 2.0f)) < 1e-4f);
    }

    // ── 判据 5：RingLayout 历史帧基址计算与循环回绕（T7.1）──────────────────

    void TestRingLayoutPrevBase()
    {
        graph::RingLayout ring(3);
        const uint32_t static_count = 100;
        const uint32_t dynamic_count = 50;

        ring.SetFrameIndex(0);
        EXPECT(ring.GetFrameIndex() == 0u);
        EXPECT(ring.GetPrevFrameIndex() == 2u);
        EXPECT(ring.GetBaseIndex(static_count, dynamic_count) == 100u);
        EXPECT(ring.GetPrevBaseIndex(static_count, dynamic_count) == 200u);

        ring.SetFrameIndex(1);
        EXPECT(ring.GetFrameIndex() == 1u);
        EXPECT(ring.GetPrevFrameIndex() == 0u);
        EXPECT(ring.GetBaseIndex(static_count, dynamic_count) == 150u);
        EXPECT(ring.GetPrevBaseIndex(static_count, dynamic_count) == 100u);

        ring.SetFrameIndex(2);
        EXPECT(ring.GetFrameIndex() == 2u);
        EXPECT(ring.GetPrevFrameIndex() == 1u);
        EXPECT(ring.GetBaseIndex(static_count, dynamic_count) == 200u);
        EXPECT(ring.GetPrevBaseIndex(static_count, dynamic_count) == 150u);
    }

    // ── 判据 6：EntityGPUTable 双帧变换行下发与事实保持（T7.1）──────────────

    void TestEntityGPUTablePrevTransform()
    {
        EntityGPUTable table;
        EntityID id{ 1, 1 };
        table.Activate(id);

        EntityGPU *row = table.Row(id);
        EXPECT(row != nullptr);
        EXPECT(row->prev_transform_id == 0u);

        // 静态实体行号：前后帧恒等（row == prev_row）
        table.SetTransformRow(id, 10u, 10u);
        EXPECT(row->row[uint32_t(ComponentType::Transform)] == 10u);
        EXPECT(row->prev_transform_id == 10u);

        // 可动实体行号：随环帧槽分流（当前帧 512，前一帧 256）
        table.SetTransformRow(id, 512u, 256u);
        EXPECT(row->row[uint32_t(ComponentType::Transform)] == 512u);
        EXPECT(row->prev_transform_id == 256u);

        // SetModelFacts 默认 0 时不冲掉已算出的 prev_transform_id
        table.SetModelFacts(id, 42u, 0xFFFFFFFFull, 5u, 0u);
        EXPECT(row->model_template_id == 42u);
        EXPECT(row->custom_data_index == 5u);
        EXPECT(row->prev_transform_id == 256u);

        // 显式传入非零 override 时覆盖
        table.SetModelFacts(id, 42u, 0xFFFFFFFFull, 5u, 999u);
        EXPECT(row->prev_transform_id == 999u);
    }

    void TestComponentRefreshPreservesTransformRow()
    {
        ECSContext ctx;
        Entity *entity = ctx.CreateEntity<Entity>("TransformRefresh");
        EXPECT(entity != nullptr);
        if (!entity)
            return;

        EntityGPUTable &table = ctx.GetEntityGPUTable();
        const EntityID id = entity->GetEntityID();
        table.SetTransformRow(id, 512u, 256u);

        EXPECT(entity->AddComponent<GeometryData>() != nullptr);
        const EntityGPU *row = table.Row(id);
        EXPECT(row->row[uint32_t(ComponentType::Transform)] == 512u);
        EXPECT(row->type[uint32_t(ComponentType::Transform)] == uint8_t(ComponentType::Transform));
        EXPECT(row->prev_transform_id == 256u);
        EXPECT(row->row[uint32_t(ComponentType::Geometry)] == entity->GetComponentRow(ComponentType::Geometry));
        EXPECT(row->type[uint32_t(ComponentType::Geometry)] == uint8_t(ComponentType::Geometry));

        entity->DetachAllComponents();
        EXPECT(row->row[uint32_t(ComponentType::Transform)] == 512u);
        EXPECT(row->type[uint32_t(ComponentType::Transform)] == uint8_t(ComponentType::Transform));
        EXPECT(row->prev_transform_id == 256u);
        EXPECT(row->row[uint32_t(ComponentType::Geometry)] == 0u);
        EXPECT(row->type[uint32_t(ComponentType::Geometry)] == 0u);
    }

    void TestEntityRenderDataCleanup()
    {
        ECSContext ctx;
        auto *first = ctx.CreateEntity("RenderDataWithSlot");
        auto *second = ctx.CreateEntity("RenderDataWithoutSlot");
        EXPECT(first && second);
        if (!first || !second)
            return;

        const EntityID first_id = first->GetEntityID();
        const EntityID second_id = second->GetEntityID();
        auto *first_material = ctx.GetOrCreateMaterialData(first);
        EXPECT(first_material != nullptr);
        EXPECT(first_material && first_material->GetContext() == &ctx);
        EXPECT(ctx.GetOrCreateMaterialData(second) != nullptr);
        ctx.GetOrCreateMaterialRuntimeSlot(first_id);

        graph::mtl::MaterialPipelineConfig config;
        config.wireframe = true;
        auto &pipelines = ctx.GetMaterialRuntimePipelineTable();
        pipelines.Set(first_id, nullptr, nullptr, config);
        pipelines.Set(second_id, nullptr, nullptr, config);
        EXPECT(pipelines.GetConfig(first_id, nullptr).wireframe);
        EXPECT(pipelines.GetConfig(second_id, nullptr).wireframe);

        ctx.DestroyEntity(first_id);
        EXPECT(!ctx.GetMaterialData(first_id));
        EXPECT(!ctx.GetMaterialRuntimeSlot(first_id));
        EXPECT(!pipelines.GetConfig(first_id, nullptr).wireframe);

        ctx.Shutdown();
        EXPECT(!ctx.GetMaterialData(second_id));
        EXPECT(!pipelines.GetConfig(second_id, nullptr).wireframe);
    }
}//namespace

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    logger::InitLogger(OS_TEXT("TestGPUModelComponents"));

    TestModelRefDefaultsAndMask();
    TestBatchInstanceAccounting();
    TestComponentMountAndEnumeration();
    TestStaticDynamicTransformConvention();
    TestRingLayoutPrevBase();
    TestEntityGPUTablePrevTransform();
    TestComponentRefreshPreservesTransformRow();
    TestEntityRenderDataCleanup();
    TestGPUResourcesSourceBoundary();
    TestRenderDataSourceBoundary();

    if (g_fail > 0)
    {
        GLogError(u8"=== TestGPUModelComponents: %d 项判据失败 ===", g_fail);
        return 1;
    }

    GLogInfo(u8"=== TestGPUModelComponents: 全部测试通过 ===");
    return 0;
}
