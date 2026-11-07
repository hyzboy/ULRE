/**
 * TestMaterialAsyncCompiler —— 材质异步解析落地层（Tier 2 → Tier 3 桥）的回归测试。
 *
 * 覆盖六件事：
 *   1. 挂接/摘除契约：未挂池不投递、重复挂接被拒、有在途任务不许摘；
 *   2. 配方走工作线程解析后，主线程 `Pump()` 的提交结果与同步路径**完全一致**
 *      （含「子配方先到、父配方后到」的多轮继承推进）；
 *   3. 数据块的三种落地面：TOML 成功、TOML 失败（留 `Failed` 句柄）、二进制坏
 *      magic（留 `Failed` 句柄），以及「解析失败 ≠ 任务失败」的区分；
 *   4. 无注册表时不崩、计入失败；
 *   5. 文件投递的两段式流水线：加载线程读盘 → 中转纯内存解析任务 → CPU 池 →
 *      主线程 `Pump()` 提交；坏路径以失败结算、在途名额守恒。
 *
 * 测试**不**验证性能，只验证「搬上工作线程后语义没变」——性能归 `-L perf`。
 */
#include <hgl/graph/asset/MaterialAsyncCompile.h>
#include <hgl/graph/asset/MaterialDataBlockRegistry.h>
#include <hgl/graph/module/TextureManager.h>
#include <hgl/mtl/MaterialRecipeRegistry.h>

#include <hgl/asset/LoadedFileManager.h>
#include <hgl/filesystem/FileSystem.h>
#include <hgl/io/FileOutputStream.h>
#include <hgl/log/Log.h>
#include <hgl/log/Logger.h>
#include <hgl/time/Time.h>

#include <cstring>

using namespace hgl::graph::mtl;

using hgl::uint8;
using hgl::uint32;
using hgl::uint64;

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
        GLogError(u8"TestMaterialAsyncCompiler 失败: %s", label);
    }

    /// 反复 `Pump()` 直到没有在途任务；超时返回 `false`
    bool PumpUntilIdle(MaterialAsyncCompiler &compiler,
                       MaterialRecipeRegistry *recipes,
                       MaterialDataBlockRegistry *blocks,
                       hgl::graph::TextureManager *texture_manager,
                       const int timeout_ms = 5000)
    {
        for (int i = 0; i < timeout_ms; ++i)
        {
            compiler.Pump(recipes, blocks, texture_manager);

            if (compiler.IsIdle())
            {
                compiler.Pump(recipes, blocks, texture_manager);
                return compiler.IsIdle();
            }

            hgl::SleepSecond(0.001);
        }

        return false;
    }

    constexpr char kParentRecipe[] =
        "schema = 1\n"
        "name = \"Async.Base\"\n"
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
        "name = \"Async.Child\"\n"
        "parent = \"rcp:@Async.Base\"\n"
        "\n"
        "[render_state]\n"
        "dither = true\n"
        "\n"
        "[[textures]]\n"
        "name = \"base_color\"\n"
        "asset = \"res/image/other.Tex2D\"\n";

    constexpr char kBrokenRecipe[] =
        "schema = 1\n"
        "name = \n"
        "domain = \"UnlitTexture\"\n";

    constexpr char kPbrBlock[] =
        "schema = 1\n"
        "name = \"pbr_async\"\n"
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
        "name = \"broken_async\"\n"
        "target = \"PBRSurface\"\n"
        "\n"
        "[[rows]]\n"
        "metallic_v2 = 0.5\n";

    // ── 1. 挂接 / 摘除契约 ─────────────────────────────────────────────────
    void TestAttachDetach()
    {
        hgl::asset::AssetCPUTaskPool pool;
        Check(pool.Start(2), "任务池启动");

        MaterialAsyncCompiler compiler;

        Check(!compiler.SubmitRecipeFile(OS_TEXT("not_attached.recipe.toml")),
              "未挂接任务池时投递应失败");
        Check(compiler.Attach(&pool), "挂接任务池");
        Check(compiler.IsAttached(), "挂接后状态为已挂接");
        Check(!compiler.Attach(&pool), "重复挂接应被拒");
        Check(compiler.GetChannelID() != 0, "IO 通道号非零");

        // 有在途任务时不许摘除，否则成品会落进已解开的挂接里没人认领
        Check(compiler.SubmitRecipeSource(OS_TEXT("detach-pending"), kParentRecipe,
                                          static_cast<int>(strlen(kParentRecipe))),
              "在途任务提交");
        Check(!compiler.Detach(), "有在途任务时摘除应失败");

        MaterialRecipeRegistry registry;
        hgl::graph::TextureManager texture_manager(nullptr);
        Check(PumpUntilIdle(compiler, &registry, nullptr, &texture_manager), "追平在途任务");
        Check(compiler.Detach(), "空闲后摘除成功");

        texture_manager.Release();
        pool.Stop();
    }

    // ── 2. 配方：子先到、父后到（跨任务的多轮继承推进）────────────────────
    void TestRecipeChildBeforeParent()
    {
        hgl::asset::AssetCPUTaskPool pool;
        Check(pool.Start(2), "配方用例：任务池启动");

        MaterialAsyncCompiler compiler;
        Check(compiler.Attach(&pool), "配方用例：挂接");

        MaterialRecipeRegistry registry;
        hgl::graph::TextureManager texture_manager(nullptr);

        // 子先投、父后投：两个任务的完成顺序不确定，提交顺序也不确定，
        // 但 `RegisterParsedRecipe` 的多轮推进必须保证最终都就绪。
        Check(compiler.SubmitRecipeSource(OS_TEXT("async-child"), kChildRecipe,
                                          static_cast<int>(strlen(kChildRecipe))),
              "配方用例：投递子配方");
        Check(compiler.SubmitRecipeSource(OS_TEXT("async-parent"), kParentRecipe,
                                          static_cast<int>(strlen(kParentRecipe))),
              "配方用例：投递父配方");

        Check(PumpUntilIdle(compiler, &registry, nullptr, &texture_manager),
              "配方用例：追平所有任务");
        Check(compiler.GetRecipeCommittedCount() == 2, "配方用例：两条都提交成功");
        Check(compiler.GetFailedCount() == 0, "配方用例：无失败");
        Check(registry.GetCount() == 2, "配方用例：注册表里有两条");

        const MaterialRecipeAsset *child = registry.FindAssetByName("Async.Child");
        Check(child != nullptr && child->IsReady(), "配方用例：子配方最终就绪");
        Check(registry.FinalizePending() == 0, "配方用例：无悬空配方");

        if (child && child->IsReady())
        {
            const MaterialRecipe &recipe = child->GetRecipe();
            Check(recipe.domain == MaterialDomain::UnlitTexture, "配方用例：domain 继承父");
            Check(recipe.textures.size() == 1, "配方用例：纹理槽数量");
            Check(recipe.textures[0].array_layer == 2, "配方用例：layer 沿用父");
        }

        texture_manager.Release();
        Check(compiler.Detach(), "配方用例：摘除");
        pool.Stop();
    }

    // ── 3. 配方坏输入：不建句柄、计入失败 ─────────────────────────────────
    void TestRecipeParseFailure()
    {
        hgl::asset::AssetCPUTaskPool pool;
        Check(pool.Start(1), "坏配方用例：任务池启动");

        MaterialAsyncCompiler compiler;
        Check(compiler.Attach(&pool), "坏配方用例：挂接");

        MaterialRecipeRegistry registry;
        hgl::graph::TextureManager texture_manager(nullptr);

        Check(compiler.SubmitRecipeSource(OS_TEXT("async-broken"), kBrokenRecipe,
                                          static_cast<int>(strlen(kBrokenRecipe))),
              "坏配方用例：投递");

        Check(PumpUntilIdle(compiler, &registry, nullptr, &texture_manager),
              "坏配方用例：追平");
        Check(compiler.GetRecipeCommittedCount() == 0, "坏配方用例：没有提交");
        Check(compiler.GetFailedCount() == 1, "坏配方用例：计入一次失败");
        Check(registry.GetCount() == 0, "坏配方用例：注册表为空");

        texture_manager.Release();
        Check(compiler.Detach(), "坏配方用例：摘除");
        pool.Stop();
    }

    // ── 4. 数据块：TOML 成功 / TOML 失败 / 二进制失败 ──────────────────────
    void TestDataBlockLanes()
    {
        hgl::asset::AssetCPUTaskPool pool;
        Check(pool.Start(2), "数据块用例：任务池启动");

        MaterialAsyncCompiler compiler;
        Check(compiler.Attach(&pool), "数据块用例：挂接");

        MaterialDataBlockRegistry blocks;

        Check(compiler.SubmitDataBlockSource(OS_TEXT("pbr.material_data.toml"),
                                             reinterpret_cast<const uint8 *>(kPbrBlock),
                                             static_cast<uint64>(strlen(kPbrBlock))),
              "数据块用例：投递合法 TOML");
        Check(PumpUntilIdle(compiler, nullptr, &blocks, nullptr), "数据块用例：追平合法 TOML");

        MaterialDataBlockAsset *ok_asset = blocks.FindByName("pbr_async");
        Check(ok_asset != nullptr, "数据块用例：合法 TOML 建出句柄");
        if (ok_asset)
        {
            Check(ok_asset->HasSpec(), "数据块用例：TOML 来源带授权形态");
            Check(ok_asset->GetRowCount() == 2, "数据块用例：行数");
            Check(ok_asset->GetState() != hgl::asset::AssetState::Failed,
                  "数据块用例：合法 TOML 不是失败态");
        }
        Check(compiler.GetDataBlockCommittedCount() == 1, "数据块用例：合法 TOML 计入提交");

        // 坏 TOML：解析失败 ≠ 任务失败，句柄必须留表并带诊断（TOML 在 `name` 之前就
        // 失败时形态是空的，所以按序号取，而不是 `FindByName`）。
        Check(compiler.SubmitDataBlockSource(OS_TEXT("broken.material_data.toml"),
                                             reinterpret_cast<const uint8 *>(kBrokenBlock),
                                             static_cast<uint64>(strlen(kBrokenBlock))),
              "数据块用例：投递坏 TOML");
        Check(PumpUntilIdle(compiler, nullptr, &blocks, nullptr), "数据块用例：追平坏 TOML");
        Check(compiler.GetFailedCount() == 1, "数据块用例：坏 TOML 计入一次失败");
        Check(blocks.GetCount() == 2, "数据块用例：坏 TOML 仍然建句柄留表");

        MaterialDataBlockAsset *bad_asset = blocks.GetAssetAt(1);
        Check(bad_asset != nullptr, "数据块用例：坏 TOML 句柄可取");
        if (bad_asset)
        {
            Check(bad_asset->GetState() == hgl::asset::AssetState::Failed,
                  "数据块用例：坏 TOML 句柄为失败态");
            Check(!bad_asset->GetFailureReason().IsEmpty(),
                  "数据块用例：坏 TOML 句柄带诊断文案");
        }

        // 坏二进制：magic 不对 ⇒ 失败句柄留表
        const uint8 bad_binary[32] = {'X', 'X', 'X', 'X'};
        Check(compiler.SubmitDataBlockSource(OS_TEXT("bad.material_data.mdb"),
                                             bad_binary,
                                             sizeof(bad_binary)),
              "数据块用例：投递坏二进制");
        Check(PumpUntilIdle(compiler, nullptr, &blocks, nullptr), "数据块用例：追平坏二进制");
        Check(compiler.GetFailedCount() == 2, "数据块用例：坏二进制计入失败");
        Check(blocks.GetCount() == 3, "数据块用例：三份输入都留了句柄");

        Check(compiler.Detach(), "数据块用例：摘除");
        pool.Stop();
    }

    // ── 5. 没有注册表时不崩、计入失败 ──────────────────────────────────────
    void TestPumpWithoutRegistry()
    {
        hgl::asset::AssetCPUTaskPool pool;
        Check(pool.Start(1), "无注册表用例：任务池启动");

        MaterialAsyncCompiler compiler;
        Check(compiler.Attach(&pool), "无注册表用例：挂接");
        Check(compiler.SubmitRecipeSource(OS_TEXT("no-registry"), kParentRecipe,
                                          static_cast<int>(strlen(kParentRecipe))),
              "无注册表用例：投递");

        Check(PumpUntilIdle(compiler, nullptr, nullptr, nullptr), "无注册表用例：追平");
        Check(compiler.GetFailedCount() == 1, "无注册表用例：计入失败而不崩");

        Check(compiler.Detach(), "无注册表用例：摘除");
        pool.Stop();
    }

    // ── 6. 文件投递：加载线程读盘 → 中转 → CPU 池解析 → Pump 提交 ─────────
    //    阶段 3 的核心行为：解析任务不再自己碰盘，读盘全程在加载线程上。
    void TestFileRelayEndToEnd()
    {
        hgl::asset::AssetCPUTaskPool pool;
        Check(pool.Start(2), "文件中转用例：任务池启动");

        hgl::LoadedFileManager lfm;
        Check(lfm.Start(1), "文件中转用例：读盘底座启动");

        MaterialAsyncCompiler compiler;
        Check(compiler.Attach(&pool), "文件中转用例：挂接任务池");
        compiler.AttachFileManager(&lfm);

        MaterialRecipeRegistry registry;
        hgl::graph::TextureManager texture_manager(nullptr);

        hgl::OSString dir;
        dir = OS_TEXT("build/test_matcompiler");
        Check(hgl::filesystem::MakePath(dir), "文件中转用例：建临时目录");

        hgl::OSString recipe_path = dir;
        recipe_path += OS_TEXT("/relay.recipe.toml");

        hgl::io::FileOutputStream *fos = hgl::io::CreateFileOutputStream(recipe_path);
        Check(fos != nullptr, "文件中转用例：写配方文件");
        if (fos)
        {
            const int size = static_cast<int>(strlen(kParentRecipe));
            Check(fos->Write(kParentRecipe, size) == size, "文件中转用例：写入内容");
            delete fos;
        }

        Check(compiler.SubmitRecipeFile(recipe_path), "文件中转用例：投递配方文件");
        Check(compiler.GetPendingCount() == 1, "文件中转用例：投递即在途");

        // 读盘腿 + 解析腿都要泵：加载线程跑完自己会中转，缺泵只影响缓存命中的派发时机
        bool settled = false;
        for (int i = 0; i < 5000 && !settled; ++i)
        {
            lfm.Pump();
            compiler.Pump(&registry, nullptr, &texture_manager);

            if (compiler.IsIdle() && lfm.GetPendingJobCount() == 0)
            {
                lfm.Pump();
                compiler.Pump(&registry, nullptr, &texture_manager);
                settled = compiler.IsIdle();
            }

            hgl::SleepSecond(0.001);
        }

        Check(settled, "文件中转用例：追平");
        Check(compiler.GetRecipeCommittedCount() == 1, "文件中转用例：读盘→解析→提交成功");
        Check(registry.GetCount() == 1, "文件中转用例：配方进表");
        Check(compiler.GetFailedCount() == 0, "文件中转用例：无失败");

        // 读不存在的文件：投递被接受（在途），终态以失败结算，名额不泄漏
        hgl::OSString missing_path = dir;
        missing_path += OS_TEXT("/nope.recipe.toml");

        Check(compiler.SubmitRecipeFile(missing_path),
              "缺失文件用例：投递被接受");
        Check(compiler.GetPendingCount() == 1, "缺失文件用例：在途 1");

        settled = false;
        for (int i = 0; i < 5000 && !settled; ++i)
        {
            lfm.Pump();
            compiler.Pump(&registry, nullptr, &texture_manager);

            if (compiler.IsIdle() && lfm.GetPendingJobCount() == 0)
            {
                lfm.Pump();
                compiler.Pump(&registry, nullptr, &texture_manager);
                settled = compiler.IsIdle();
            }

            hgl::SleepSecond(0.001);
        }

        Check(settled, "缺失文件用例：追平");
        Check(compiler.GetFailedCount() == 1, "缺失文件用例：计入一次失败");
        Check(compiler.GetRecipeCommittedCount() == 1, "缺失文件用例：提交数不变");

        // 没有注入 LoadedFileManager 之前，文件投递必须当场拒绝（本用例开头未注入时
        // 由 TestAttachDetach 覆盖：未挂池即失败）
        texture_manager.Release();
        Check(compiler.Detach(), "文件中转用例：摘除");
        lfm.Stop();
        pool.Stop();
    }
}

int main()
{
    hgl::logger::InitLogger(OS_TEXT("TestMaterialAsyncCompiler"));

    TestAttachDetach();
    TestRecipeChildBeforeParent();
    TestRecipeParseFailure();
    TestDataBlockLanes();
    TestPumpWithoutRegistry();
    TestFileRelayEndToEnd();

    if (g_fail > 0 || g_pass == 0)
    {
        GLogError(u8"=== TestMaterialAsyncCompiler: 通过 %d / %d 项，%d 项失败 ===",
                  g_pass, g_pass + g_fail, g_fail);
        return 1;
    }

    GLogInfo(u8"=== TestMaterialAsyncCompiler: 全部 %d 项测试通过 ===", g_pass);
    return 0;
}
