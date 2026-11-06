#include<hgl/ecs/systems/render/streaming/SceneStreamingSystem.h>
#include<hgl/ecs/core/System.h>
#include<hgl/graph/asset/ModelAsset.h>
#include<hgl/graph/geo/GeometryVertexFormat.h>
#include<hgl/mtl/MaterialRecipe.h>
#include<hgl/log/Log.h>
#include<hgl/log/Logger.h>
#include<hgl/type/String.h>

/**
 * 判据：`SceneStreamingSystem` 的守卫与外部设定。
 *
 * 本文件刻意**不碰设备**：流送系统的危险边界在「设备/上下文尚未就绪」那一侧
 * —— 那里最容易写出「拿空指针去查状态」的崩溃，而它离出事点很远。
 * 真机路径（读盘 → 解析 → 建 buffer）由阶段六的示例回归覆盖。
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

    // ── 判据 1：构造即自洽，析构可重入 ────────────────────────────────────

    void TestConstruction()
    {
        SceneStreamingSystem system;

        EXPECT(system.GetName() == AnsiString("SceneStreamingSystem"));

        // 纹理/几何流送都排在这个相位：本系统必须与它们同相位或更早，
        // 否则本帧拿到的字节要等下一帧才可能变成可画的图元。
        EXPECT(system.GetExecutionPhase() == ExecutionPhase::RenderPreBeginFrame);

        EXPECT(!system.HasBuildSettings());
        EXPECT(system.GetStreamCount() == 0);

        // 显式 Shutdown 之后再析构（析构里还会再调一次）不得重复释放。
        system.Shutdown();
        system.Shutdown();

        EXPECT(system.GetStreamCount() == 0);
        EXPECT(!system.HasBuildSettings());
    }

    // ── 判据 2：外部设定可读写 ────────────────────────────────────────────

    void TestBuildSettings()
    {
        SceneStreamingSystem system;

        graph::GeometryVertexFormat gvf;
        graph::mtl::MaterialRecipe  recipe;

        const graph::mtl::MaterialRecipe *table[2] = { &recipe, nullptr };

        system.SetBuildSettings(gvf, &recipe, table, 2, OS_TEXT("res/Scene"));
        EXPECT(system.HasBuildSettings());

        // 只给默认配方、不给表：多材质场景会全部退回默认配方，
        // 但系统必须接受这种「能用就行」的配置而不是拒绝。
        system.SetBuildSettings(gvf, &recipe, nullptr, 0);
        EXPECT(system.HasBuildSettings());

        // Shutdown 必须把设定一并清掉：残留设定会让重启后的首帧
        // 用上一份顶点格式去解析，布局对不上就是花屏。
        system.Shutdown();
        EXPECT(!system.HasBuildSettings());
    }

    // ── 判据 3：没有图形上下文时 Update 不得动句柄 ────────────────────────

    void TestUpdateWithoutContext()
    {
        graph::ModelAssetRegistry registry;
        graph::SetDefaultModelAssetRegistry(&registry);

        graph::ModelAsset *asset = graph::RequestModelAsset("pack(res):Probe/NoContext.Scene.model");
        EXPECT(asset != nullptr);

        {
            SceneStreamingSystem system;

            // 没有 ECSContext、没有 GraphicsContext：拿不到 LoadedFileManager，
            // 此时**不能**给句柄判失败（设备还没建好而已，不是场景有问题）。
            for (int i = 0; i < 3; ++i)
                system.Update(1.0f / 60.0f);

            EXPECT(asset->GetState() == graph::ModelAssetState::Unloaded);
            EXPECT(asset->GetFailReason().IsEmpty());
            EXPECT(system.GetStreamCount() == 0);
        }

        EXPECT(asset->GetState() == graph::ModelAssetState::Unloaded);

        graph::SetDefaultModelAssetRegistry(nullptr);

        EXPECT(graph::GetDefaultModelAssetRegistry() == nullptr);
        EXPECT(registry.GetCount() == 1);
        EXPECT(registry.GetPendingCount() == 1);
    }

    // ── 判据 4：注册表被清空后系统不得持悬垂指针 ──────────────────────────

    void TestRegistryClearWhileStreaming()
    {
        graph::ModelAssetRegistry registry;
        graph::SetDefaultModelAssetRegistry(&registry);

        SceneStreamingSystem system;

        {
            graph::ModelAsset *asset = graph::RequestModelAsset("pack(res):Probe/Dropped.Scene.model");
            EXPECT(asset != nullptr);
        }

        // 没有上下文时不会建流送条目；本判据验证的是「句柄被外部销毁」之后
        // 系统手里的指针不会变野 —— 一次 Update 就该安静地什么也不做。
        system.Update(1.0f / 60.0f);

        registry.Clear();

        system.Update(1.0f / 60.0f);
        system.Update(1.0f / 60.0f);

        EXPECT(system.GetStreamCount() == 0);

        system.Shutdown();

        graph::SetDefaultModelAssetRegistry(nullptr);
    }
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    logger::InitLogger(OS_TEXT("TestSceneStreamingSystem"));

    TestConstruction();
    TestBuildSettings();
    TestUpdateWithoutContext();
    TestRegistryClearWhileStreaming();

    if (g_fail > 0)
    {
        GLogError(u8"=== TestSceneStreamingSystem: %d 项判据失败 ===", g_fail);
        return 1;
    }

    GLogInfo(u8"=== TestSceneStreamingSystem: 全部测试通过 ===");
    return 0;
}
