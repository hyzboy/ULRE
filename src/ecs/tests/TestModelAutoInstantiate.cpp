#include<hgl/ecs/systems/render/streaming/ModelStreamingSystem.h>
#include<hgl/ecs/components/ModelRefComponent.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/entity/Entity.h>
#include<hgl/graph/asset/ModelAsset.h>
#include<hgl/log/Log.h>
#include<hgl/log/Logger.h>
#include<hgl/type/String.h>

/**
 * 判据：`ModelRefComponent` 自动装配（InstantiateModelRefs，Update 阶段⑦）的
 * 请求段守卫与终态语义。
 *
 * 本文件刻意**不碰设备**：进程里没有 `GraphicsContext` ⇒ IO 泵（①~⑥）空转，
 * 但装配段守卫必须照常推进 —— 策略拦截、防重复投递、防悬垂、失败不重试，
 * 这些正是「宿主把装配交给系统」后最容易写错的边界。
 * 三段产物语义（骨架 → 几何 → 材质）需要真机资产管线，由 LoadModel 示例
 * 的帧哈希回归覆盖。
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

    // ── 计数策略：拦截节奏 + 失败上报次数都可观察 ──────────────────────────

    class ProbePolicy final : public ModelLoadPolicy
    {
    public:
        int allow_priority = 0;      ///< OnBeforeRequest 的返回值（负数 ⇒ 压下）
        int before_calls   = 0;
        int failed_calls   = 0;
        AnsiString last_reason;

        int OnBeforeRequest(const ModelRefComponent & /*component*/,
                            const AnsiString & /*uri*/) override
        {
            ++before_calls;
            return allow_priority;
        }

        void OnModelFailed(ModelRefComponent & /*component*/,
                           const char *reason) override
        {
            ++failed_calls;
            last_reason = AnsiString(reason);
        }
    };//class ProbePolicy

    // ── 判据 1：组件表为空 ⇒ 装配段安静早退，不碰任何全局 ──────────────────

    void TestEmptyRefsFastPath()
    {
        // 本用例连默认表都不登记：若装配段误走请求/守卫路径，会以空表投请求。
        ECSContext ctx;
        auto system = ctx.RegisterSystem<ModelStreamingSystem>();

        system->Update(0.0f);
        system->Update(0.0f);

        EXPECT(graph::GetDefaultModelAssetRegistry() == nullptr);
    }

    // ── 判据 2：策略负值逐帧再问；放行后投递且不再询问 ────────────────────

    void TestPolicyDeferAndSubmit()
    {
        graph::ModelAssetRegistry registry;
        graph::SetDefaultModelAssetRegistry(&registry);

        ECSContext ctx;
        auto system = ctx.RegisterSystem<ModelStreamingSystem>();

        Entity *root = ctx.CreateEntity<Entity>("Deferred");
        ModelRefComponent *ref = root->AddComponent<ModelRefComponent>();
        ref->uri = AnsiString("pack(res):Probe/Auto.Defer.Scene.model");

        ProbePolicy policy;
        policy.allow_priority = -1;
        ref->policy = &policy;

        system->Update(0.0f);
        EXPECT(policy.before_calls == 1);
        EXPECT(!ref->request_started);
        EXPECT(ref->asset == nullptr);
        EXPECT(!ref->IsSettled());

        // 负数只是「本帧压下」：不置 request_started、不判失败，下帧继续问。
        system->Update(0.0f);
        EXPECT(policy.before_calls == 2);
        EXPECT(!ref->request_started);

        policy.allow_priority = 3;
        system->Update(0.0f);
        EXPECT(ref->request_started);
        EXPECT(ref->asset != nullptr);
        // 无设备 ⇒ 泵不跑：句柄只进表停在 Unloaded，不该被误判失败。
        EXPECT(ref->asset->GetState() == graph::ModelAssetState::Unloaded);
        EXPECT(!ref->failed_reported);

        // 已提交 ⇒ 请求段封口：OnBeforeRequest 不再被问。
        const int calls = policy.before_calls;
        system->Update(0.0f);
        system->Update(0.0f);
        EXPECT(policy.before_calls == calls);

        // 收尾：先拆世界再清表，保证没有谁在中间拿悬垂指针。
        graph::SetDefaultModelAssetRegistry(nullptr);
    }

    // ── 判据 3：无策略直接投；空 uri 原地停住、永不落终态 ──────────────────

    void TestRequestGuardsWithoutPolicy()
    {
        graph::ModelAssetRegistry registry;
        graph::SetDefaultModelAssetRegistry(&registry);

        ECSContext ctx;
        auto system = ctx.RegisterSystem<ModelStreamingSystem>();

        Entity *e1 = ctx.CreateEntity<Entity>("NoPolicy");
        ModelRefComponent *ref1 = e1->AddComponent<ModelRefComponent>();
        ref1->uri = AnsiString("pack(res):Probe/Auto.NoPolicy.Scene.model");

        Entity *e2 = ctx.CreateEntity<Entity>("EmptyUri");
        ModelRefComponent *ref2 = e2->AddComponent<ModelRefComponent>();
        // uri 留空：视为「调用方自己管句柄」，系统不得代投也不得判失败。

        system->Update(0.0f);
        EXPECT(ref1->request_started);
        EXPECT(ref1->asset != nullptr);
        EXPECT(!ref2->request_started);
        EXPECT(ref2->asset == nullptr);
        EXPECT(!ref2->failed_reported);

        system->Update(0.0f);
        EXPECT(!ref2->IsSettled());

        graph::SetDefaultModelAssetRegistry(nullptr);
    }

    // ── 判据 4：句柄被 registry 回收 ⇒ 防悬垂守卫落终态，只报一次 ──────────

    void TestDanglingHandleFailsOnce()
    {
        graph::ModelAssetRegistry registry;
        graph::SetDefaultModelAssetRegistry(&registry);

        ECSContext ctx;
        auto system = ctx.RegisterSystem<ModelStreamingSystem>();

        Entity *root = ctx.CreateEntity<Entity>("Dangling");
        ModelRefComponent *ref = root->AddComponent<ModelRefComponent>();
        ref->uri = AnsiString("pack(res):Probe/Auto.Dangling.Scene.model");

        ProbePolicy policy;
        ref->policy = &policy;

        system->Update(0.0f);
        EXPECT(ref->asset != nullptr);

        // Clear() 直接析构句柄；组件只剩观察指针 —— 下一帧必须先对表。
        registry.Clear();

        system->Update(0.0f);
        EXPECT(policy.failed_calls == 1);
        EXPECT(!policy.last_reason.IsEmpty());
        EXPECT(ref->failed_reported);
        EXPECT(ref->asset == nullptr);
        EXPECT(ref->IsSettled());

        // 终态不重试：不再询问策略、不再重复上报。
        const int before = policy.before_calls;
        system->Update(0.0f);
        system->Update(0.0f);
        EXPECT(policy.failed_calls == 1);
        EXPECT(policy.before_calls == before);

        graph::SetDefaultModelAssetRegistry(nullptr);
    }

    // ── 判据 5：默认表缺席 ⇒ 投递被拒即刻终态 ──────────────────────────────

    void TestRequestWithoutRegistryFails()
    {
        graph::SetDefaultModelAssetRegistry(nullptr);

        ECSContext ctx;
        auto system = ctx.RegisterSystem<ModelStreamingSystem>();

        Entity *root = ctx.CreateEntity<Entity>("NoRegistry");
        ModelRefComponent *ref = root->AddComponent<ModelRefComponent>();
        ref->uri = AnsiString("pack(res):Probe/Auto.NoRegistry.Scene.model");

        ProbePolicy policy;
        ref->policy = &policy;

        system->Update(0.0f);
        EXPECT(policy.failed_calls == 1);
        EXPECT(ref->failed_reported);
        EXPECT(ref->asset == nullptr);
        EXPECT(ref->request_started);    ///< 已封口：即使没拿到句柄也不得反复重投

        system->Update(0.0f);
        EXPECT(policy.failed_calls == 1);
        EXPECT(policy.before_calls == 1);
    }
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    logger::InitLogger(OS_TEXT("TestModelAutoInstantiate"));

    TestEmptyRefsFastPath();
    TestPolicyDeferAndSubmit();
    TestRequestGuardsWithoutPolicy();
    TestDanglingHandleFailsOnce();
    TestRequestWithoutRegistryFails();

    if (g_fail > 0)
    {
        GLogError(u8"=== TestModelAutoInstantiate: %d 项判据失败 ===", g_fail);
        return 1;
    }

    GLogInfo(u8"=== TestModelAutoInstantiate: 全部测试通过 ===");
    return 0;
}
