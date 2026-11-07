#include <hgl/ecs/components/GPUModelRefComponent.h>
#include <hgl/ecs/components/GPUInstancedModelBatchComponent.h>
#include <hgl/ecs/core/Context.h>
#include <hgl/ecs/core/entity/Entity.h>
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

    // ── 判据 1：GPUModelRefComponent 默认值与掩码位语义 ────────────────────

    void TestModelRefDefaultsAndMask()
    {
        GPUModelRefComponent ref;

        EXPECT(ref.template_id == 0u);
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
        GPUModelRefComponent *ref = e1->AddComponent<GPUModelRefComponent>();
        EXPECT(ref != nullptr);
        EXPECT(ref->template_id == 0u);
        ref->template_id = 7u;

        Entity *e2 = ctx.CreateEntity<Entity>("BatchB");
        GPUInstancedModelBatchComponent *batch = e2->AddComponent<GPUInstancedModelBatchComponent>();
        EXPECT(batch != nullptr);
        batch->template_id = 9u;
        batch->AddMatrix(math::Matrix4f(1.0f));

        ValueArray<GPUModelRefComponent *> refs;
        ctx.GetComponents<GPUModelRefComponent>(refs);
        EXPECT(refs.GetCount() == 1);
        EXPECT(refs.GetData()[0]->template_id == 7u);

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
        sref->template_id = 3u;
        const TransformID sid = ctx.CreateTransform(stat->GetEntityID(), Mobility::Static);
        TransformAccessor stf = ctx.GetTransformByEntity(stat->GetEntityID());
        EXPECT(stf.IsStatic());
        stf.SetLocalPosition(glm::vec3(10.0f, 0.0f, 2.0f));

        Entity *dyn = ctx.CreateEntity<Entity>("MovableModel");
        GPUModelRefComponent *dref = dyn->AddComponent<GPUModelRefComponent>();
        dref->template_id = 4u;
        const TransformID did = ctx.CreateTransform(dyn->GetEntityID(), Mobility::Movable);
        TransformAccessor dtf = ctx.GetTransformByEntity(dyn->GetEntityID());
        EXPECT(dtf.IsMovable());

        // 段名单归属
        auto inList = [](const std::vector<TransformID> &list, TransformID id)
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

    if (g_fail > 0)
    {
        GLogError(u8"=== TestGPUModelComponents: %d 项判据失败 ===", g_fail);
        return 1;
    }

    GLogInfo(u8"=== TestGPUModelComponents: 全部测试通过 ===");
    return 0;
}
