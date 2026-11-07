/**
 * TestMaterialAsyncLoadLane —— 材质装载车道（池 + 编译器 + 泵送节奏）的回归测试。
 *
 * 覆盖六件事：
 *   1. 懒启动幂等：未启动时空闲、`EnsureStarted()` 之后 `IsRunning()`，重复调用不重建；
 *   2. 整目录投递：配方目录 / 数据块目录各扫一遍，只挑认得的后缀；
 *   3. 门禁语义：投递成功即在途（`GetPendingCount() > 0`），`Pump()` 追平后归零；
 *   4. `Shutdown()` 收尾**不丢成品**：投递后不泵直接收，Drain 完的那一轮泵要把
 *      已完成结果落进还活着的注册表；
 *   5. 无注册表收尾不崩（上下文销毁路径上注册表可能先走）；
 *   6. 收尾后可重新启动并继续投递。
 *
 * 测试**不**验证性能，只验证「车道封装之后语义没变，收尾顺序安全」。
 */
#include <hgl/graph/asset/MaterialAsyncLoadLane.h>
#include <hgl/graph/asset/MaterialDataBlockRegistry.h>
#include <hgl/graph/module/TextureManager.h>
#include <hgl/mtl/MaterialRecipeRegistry.h>

#include <hgl/filesystem/FileSystem.h>
#include <hgl/filesystem/Path.h>
#include <hgl/io/FileOutputStream.h>
#include <hgl/log/Log.h>
#include <hgl/log/Logger.h>
#include <hgl/time/Time.h>
#include <hgl/utf.h>

#include <cstring>

using namespace hgl;
using namespace hgl::graph::mtl;

using hgl::uint8;

namespace
{
    int g_pass = 0;
    int g_fail = 0;

    void Check(const bool ok, const char *label)
    {
        if (ok)
        {
            ++g_pass;
            return;
        }

        ++g_fail;
        GLogError(u8"TestMaterialAsyncLoadLane 失败: %s", label);
    }

    constexpr const char *kTempRoot = "build/test_matlane";   ///< 落 build/（已被忽略）

    constexpr char kParentRecipe[] =
        "schema = 1\n"
        "name = \"Lane.Base\"\n"
        "domain = \"UnlitTexture\"\n"
        "\n"
        "[render_state]\n"
        "alpha_cutoff = 0.5\n"
        "\n"
        "[[textures]]\n"
        "name = \"base_color\"\n"
        "asset = \"res/image/base.Tex2D\"\n"
        "layer = 2\n"
        "auto_mipmaps = true\n";

    constexpr char kChildRecipe[] =
        "schema = 1\n"
        "name = \"Lane.Child\"\n"
        "parent = \"rcp:@Lane.Base\"\n"
        "\n"
        "[render_state]\n"
        "dither = true\n"
        "\n"
        "[[textures]]\n"
        "name = \"base_color\"\n"
        "asset = \"res/image/other.Tex2D\"\n";

    constexpr char kPbrBlock[] =
        "schema = 1\n"
        "name = \"lane_pbr\"\n"
        "target = \"PBRSurface\"\n"
        "\n"
        "[[rows]]\n"
        "base_color = [1.0, 0.0, 0.0, 1.0]\n"
        "metallic = 0.0\n"
        "roughness = 0.5\n"
        "\n"
        "[[rows]]\n"
        "base_color = [0.0, 1.0, 0.0, 1.0]\n"
        "metallic = 1.0\n"
        "roughness = 0.1\n";

    constexpr char kBrokenBlock[] =
        "schema = 1\n"
        "name = \"lane_broken\"\n"
        "target = \"PBRSurface\"\n"
        "\n"
        "[[rows]]\n"
        "metallic_v2 = 0.5\n";

    bool WriteFile(const OSString &path, const char *text)
    {
        io::FileOutputStream *fos = io::CreateFileOutputStream(path);

        if (fos == nullptr)
            return false;

        const int size = static_cast<int>(strlen(text));

        const bool ok = (fos->Write(text, size) == size);

        delete fos;

        return ok;
    }

    OSString TempPath(const char *rel)
    {
        OSString path = ToOSString(kTempRoot);

        path += OS_TEXT("/");
        path += ToOSString(rel);

        return path;
    }

    /// 写一棵测试素材树：两份配方（父 / 子继承）、一份合法数据块、一份坏数据块
    bool PrepareTree()
    {
        if (!filesystem::MakePath(ToOSString(kTempRoot))
            || !filesystem::MakePath(TempPath("recipe"))
            || !filesystem::MakePath(TempPath("data")))
        {
            GLogError(u8"创建临时目录失败");
            return false;
        }

        return WriteFile(TempPath("recipe/base.recipe.toml"), kParentRecipe)
            && WriteFile(TempPath("recipe/child.recipe.toml"), kChildRecipe)
            && WriteFile(TempPath("data/lane.material_data.toml"), kPbrBlock)
            && WriteFile(TempPath("data/broken.material_data.toml"), kBrokenBlock);
    }

    /// 反复 `Pump()` 直到车道空闲；超时返回 `false`
    bool PumpUntilIdle(MaterialAsyncLoadLane &lane,
                       MaterialRecipeRegistry *recipes,
                       MaterialDataBlockRegistry *blocks,
                       hgl::graph::TextureManager *texture_manager,
                       const int timeout_ms = 5000)
    {
        for (int i = 0; i < timeout_ms; ++i)
        {
            lane.Pump(recipes, blocks, texture_manager);

            if (lane.IsIdle())
            {
                lane.Pump(recipes, blocks, texture_manager);
                return lane.IsIdle();
            }

            hgl::SleepSecond(0.001);
        }

        return false;
    }

    // ── 1. 懒启动幂等 ──────────────────────────────────────────────────────
    void TestLazyStart()
    {
        MaterialAsyncLoadLane lane;

        Check(!lane.IsRunning(), "未启动时 IsRunning 为 false");
        Check(lane.IsIdle(), "未启动时视为空闲");
        Check(lane.GetPendingCount() == 0, "未启动时在途为 0");
        Check(lane.Pump(nullptr, nullptr, nullptr) == 0, "未启动时泵送无副作用");

        Check(lane.EnsureStarted(1), "首次启动成功");
        Check(lane.IsRunning(), "启动后 IsRunning 为 true");
        Check(lane.EnsureStarted(1), "重复启动幂等（仍返回 true）");
        Check(lane.IsRunning(), "重复启动后仍在运行");

        Check(lane.BeginRecipeDirectory(OS_TEXT("build/test_matlane/not_exist")) == -1,
              "不存在的目录返回 -1");
        Check(lane.IsIdle(), "投递失败不留在途");

        lane.Shutdown(nullptr, nullptr, nullptr);
        Check(!lane.IsRunning(), "收尾后 IsRunning 为 false");
    }

    // ── 2 & 3. 整目录投递 + 门禁 ──────────────────────────────────────────
    void TestDirectorySubmit()
    {
        MaterialAsyncLoadLane lane;
        Check(lane.EnsureStarted(2), "目录用例：启动");

        MaterialRecipeRegistry recipes;
        MaterialDataBlockRegistry blocks;
        hgl::graph::TextureManager texture_manager(nullptr);

        const int recipe_count = lane.BeginRecipeDirectory(TempPath("recipe"));
        Check(recipe_count == 2, "目录用例：配方目录挑出两份");

        const int block_count = lane.BeginDataBlockDirectory(TempPath("data"));
        Check(block_count == 2, "目录用例：数据块目录挑出两份");

        Check(lane.GetSubmittedRecipeCount() == 2, "目录用例：投递计数累积");
        Check(lane.GetSubmittedDataBlockCount() == 2, "目录用例：数据块投递计数累积");
        Check(lane.GetPendingCount() > 0, "目录用例：投递成功即在途（门禁要看得住）");
        Check(!lane.IsIdle(), "目录用例：在途时 IsIdle 为 false");

        Check(PumpUntilIdle(lane, &recipes, &blocks, &texture_manager),
              "目录用例：追平所有任务");
        Check(lane.IsIdle(), "目录用例：追平后空闲");
        Check(lane.GetPendingCount() == 0, "目录用例：追平后在途归零");

        Check(recipes.GetCount() == 2, "目录用例：两条配方都进表");
        Check(blocks.GetCount() == 2, "目录用例：两份数据块都进表（含失败句柄）");

        const MaterialRecipeAsset *child = recipes.FindAssetByName("Lane.Child");
        Check(child != nullptr && child->IsReady(), "目录用例：子配方最终就绪");
        Check(recipes.FinalizePending() == 0, "目录用例：无悬空配方");

        if (child != nullptr && child->IsReady())
        {
            const MaterialRecipe &recipe = child->GetRecipe();
            Check(recipe.textures.size() == 1, "目录用例：纹理槽数量");
            Check(recipe.textures[0].array_layer == 2, "目录用例：layer 从父配方继承");
        }

        MaterialDataBlockAsset *ok_block = blocks.FindByName("lane_pbr");
        Check(ok_block != nullptr, "目录用例：合法数据块建出句柄");
        if (ok_block != nullptr)
            Check(ok_block->GetRowCount() == 2, "目录用例：数据块行数");

        Check(lane.GetFailedCount() == 1, "目录用例：坏数据块计入一次失败");
        Check(lane.GetPumpedCommitCount() >= 2, "目录用例：提交计数累积");

        texture_manager.Release();
        lane.Shutdown(&recipes, &blocks, &texture_manager);
        Check(lane.GetPendingCount() == 0, "目录用例：收尾后在途为 0");
    }

    // ── 4. 收尾不丢成品 ───────────────────────────────────────────────────
    void TestShutdownKeepsFinishedWork()
    {
        MaterialAsyncLoadLane lane;
        Check(lane.EnsureStarted(2), "收尾用例：启动");

        MaterialRecipeRegistry recipes;
        MaterialDataBlockRegistry blocks;
        hgl::graph::TextureManager texture_manager(nullptr);

        Check(lane.BeginRecipeDirectory(TempPath("recipe")) == 2, "收尾用例：投递配方");
        Check(lane.BeginDataBlockDirectory(TempPath("data")) == 2, "收尾用例：投递数据块");

        // 故意**不**泵：收尾必须自己 Drain 完再泵一轮，否则成品全丢。
        lane.Shutdown(&recipes, &blocks, &texture_manager);

        Check(!lane.IsRunning(), "收尾用例：收尾后不再运行");
        Check(lane.IsIdle(), "收尾用例：收尾后空闲");
        Check(recipes.GetCount() == 2, "收尾用例：配方成品没丢");
        Check(blocks.GetCount() == 2, "收尾用例：数据块成品没丢");
        Check(recipes.FindAssetByName("Lane.Child") != nullptr, "收尾用例：子配方已落表");

        // 重复收尾必须安全（`GraphicsContext` 与析构可能各调一次）
        lane.Shutdown(&recipes, &blocks, &texture_manager);
        Check(!lane.IsRunning(), "收尾用例：重复收尾安全");

        texture_manager.Release();
    }

    // ── 5. 无注册表收尾 ───────────────────────────────────────────────────
    void TestShutdownWithoutRegistry()
    {
        MaterialAsyncLoadLane lane;
        Check(lane.EnsureStarted(1), "空收尾用例：启动");
        Check(lane.BeginRecipeFile(TempPath("recipe/base.recipe.toml")),
              "空收尾用例：投递单文件");

        lane.Shutdown(nullptr, nullptr, nullptr);
        Check(!lane.IsRunning(), "空收尾用例：无注册表收尾不崩");
        Check(lane.IsIdle(), "空收尾用例：收尾后空闲");
    }

    // ── 6. 收尾后可重启 ───────────────────────────────────────────────────
    void TestRestartAfterShutdown()
    {
        MaterialAsyncLoadLane lane;
        Check(lane.EnsureStarted(1), "重启用例：首次启动");
        lane.Shutdown(nullptr, nullptr, nullptr);

        Check(lane.EnsureStarted(2), "重启用例：收尾后重新启动");
        Check(lane.IsRunning(), "重启用例：重新运行");

        MaterialRecipeRegistry recipes;
        hgl::graph::TextureManager texture_manager(nullptr);

        Check(lane.BeginRecipeFile(TempPath("recipe/base.recipe.toml")),
              "重启用例：再次投递");
        Check(PumpUntilIdle(lane, &recipes, nullptr, &texture_manager),
              "重启用例：追平");
        Check(recipes.GetCount() == 1, "重启用例：成品进表");

        texture_manager.Release();
        lane.Shutdown(&recipes, nullptr, &texture_manager);
    }
}

int main()
{
    hgl::logger::InitLogger(OS_TEXT("TestMaterialAsyncLoadLane"));

    if (!PrepareTree())
    {
        GLogError(u8"=== TestMaterialAsyncLoadLane: 测试素材准备失败 ===");
        return 1;
    }

    TestLazyStart();
    TestDirectorySubmit();
    TestShutdownKeepsFinishedWork();
    TestShutdownWithoutRegistry();
    TestRestartAfterShutdown();

    if (g_fail > 0 || g_pass == 0)
    {
        GLogError(u8"=== TestMaterialAsyncLoadLane: 通过 %d / %d 项，%d 项失败 ===",
                  g_pass, g_pass + g_fail, g_fail);
        return 1;
    }

    GLogInfo(u8"=== TestMaterialAsyncLoadLane: 全部 %d 项测试通过 ===", g_pass);
    return 0;
}
