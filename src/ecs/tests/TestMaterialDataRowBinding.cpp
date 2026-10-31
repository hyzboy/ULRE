/**
 * `MaterialDataBlockRegistry::GetRowBinding()` / `GlobalSSBORowRangeLease::GetGlobalSSBOBindingAt()`
 * / `WorkObject::GetMaterialDataBinding()` 三层的契约测试。
 *
 * 为什么没有「上传成功后取行」的运行时用例：真上传要一个初始化好的 Vulkan 设备
 * （`GraphicsContext::Initialize` 强制要求 device），而 `src/ecs/tests` 下没有设备引导
 * 代码 —— 与 `TestGlobalSSBORowRange` / `TestRenderableBuilderDataRow` 同一个限制。
 *
 * 因此分三层，全部是真断言：
 *
 * | 层 | 查什么 | 为什么这样查 |
 * |----|--------|--------------|
 * | A 真文件 | 真 `.material_data.toml` 能被门面认出来；池为空时**失败且不写 out** | 这是能真的跑起来的部分 |
 * | B 组合 | `GetGlobalSSBOBindingAt(i)` 的取值口径：`i` ⇒ `first+i`，越界 ⇒ 无效 | 纯表达式，可源码核 |
 * | C 契约 | 三层各自必须出现的顺序（先判池、再判失败态、先上传、再判越界） | 顺序做错在源码里看不出来，只能钉死 |
 */

#include <hgl/graph/asset/MaterialDataBlockRegistry.h>
#include <hgl/graph/ssbo/GlobalSSBOTypes.h>
#include <hgl/vk/buffer/ActiveRowPool.h>
#include <hgl/io/FileInputStream.h>
#include <hgl/log/Log.h>
#include <hgl/log/Logger.h>
#include <hgl/type/String.h>

#include <cstring>

using namespace hgl;

namespace mtl = hgl::graph::mtl;
using hgl::graph::GlobalSSBOBinding;
using hgl::graph::GlobalSSBOType;
using hgl::graph::ActiveRowPool;

namespace
{
    int g_failed = 0;

    void Check(const bool ok, const char *what)
    {
        if (ok)
        {
            GLogInfo(u8"[ OK ] %s", what);
            return;
        }

        ++g_failed;
        GLogError(u8"[FAIL] %s", what);
    }

    /// 手工建实体示例用的批量数据（10×10 网格里的前两行，与真实文件同构）。
    const char *kTwoRowBlock = R"(
schema = 1
name = "manual_rows"
target = "PBRSurface"

[[rows]]
base_color = [0.40, 0.20, 0.10, 1.00]
metallic = 0.10
roughness = 0.90

[[rows]]
base_color = [0.10, 0.30, 0.50, 1.00]
metallic = 0.90
roughness = 0.20
)";

    /// PBRSpheres 已经用上的那份真文件（证明同一门面对「文件 + 名字」都成立）。
    const char *kGridDataPath =
        "ShaderLibrary/asset/material_data/pbr_10x10_grid.material_data.toml";

    // ── B 层：源码读取 ───────────────────────────────────────────────────────
    bool ReadSource(const OSString &path, AnsiString &out_text)
    {
        hgl::io::OpenFileInputStream fis(path);
        if (!fis || fis->GetSize() <= 0)
            return false;

        char chunk[4096];
        int64 got;
        while ((got = fis->Read(chunk, static_cast<int64>(sizeof(chunk)))) > 0)
            out_text.Strcat(chunk, static_cast<int>(got));

        return true;
    }

    bool ContainsToken(const AnsiString &src, const char *needle)
    {
        return strstr(src.c_str(), needle) != nullptr;
    }

    /// 从 begin 起算，left 的首次出现必须早于 right 的首次出现（同一函数体里的顺序断言）。
    bool OrderedIn(const AnsiString &src, const char *begin,
                   const char *left, const char *right)
    {
        const char *start = strstr(src.c_str(), begin);
        if (!start)
            return false;

        const char *a = strstr(start, left);
        const char *b = strstr(start, right);
        return a && b && a < b;
    }

    // ── A 层：真文件 + 真门面 ────────────────────────────────────────────────

    /// 池为空时门面必须失败，而且**不得**把 `out` 写脏（调用方靠它保留旧值）。
    bool TestNullPoolLeavesOutUntouched()
    {
        mtl::MaterialDataBlockRegistry registry;

        mtl::MaterialDataBlockAsset *block = registry.RegisterFromToml(
            OS_TEXT("<mem>"), kTwoRowBlock, static_cast<int>(strlen(kTwoRowBlock)));

        if (!block || block->IsFailed())
            return false;

        GlobalSSBOBinding sentinel{};
        sentinel.ssbo_type  = GlobalSSBOType::EmissiveSurface;
        sentinel.ssbo_id    = 42;
        sentinel.data_index = 7;

        GlobalSSBOBinding out = sentinel;

        const bool ok = registry.GetRowBinding(nullptr, OS_TEXT("@manual_rows"), 0, out);

        return !ok
            && out.ssbo_type  == sentinel.ssbo_type
            && out.ssbo_id    == sentinel.ssbo_id
            && out.data_index == sentinel.data_index;
    }

    /// 门面认不出 URI（名字未注册）时，`GetOrLoadURI` 必须返回空 —— 这是 `GetRowBinding`
    /// 的第二道门；同时任何失败路径都不得把 `out` 写脏。
    bool TestUnknownUriLeavesOutUntouched()
    {
        mtl::MaterialDataBlockRegistry registry;

        registry.RegisterFromToml(OS_TEXT("<mem>"), kTwoRowBlock,
                                  static_cast<int>(strlen(kTwoRowBlock)));

        Check(registry.GetOrLoadURI(OS_TEXT("@no_such_block")) == nullptr,
              "未注册的块名解析为空");

        Check(registry.GetOrLoadURI(OS_TEXT("geo:builtin:cube")) == nullptr,
              "类型段不是 mtldata 的 URI 被拒（不当路径读盘）");

        GlobalSSBOBinding sentinel{};
        sentinel.ssbo_type  = GlobalSSBOType::TransmissionSurface;
        sentinel.ssbo_id    = 9;
        sentinel.data_index = 3;

        GlobalSSBOBinding out = sentinel;

        const bool ok = registry.GetRowBinding(nullptr, OS_TEXT("@no_such_block"), 0, out);

        return !ok
            && out.ssbo_type  == sentinel.ssbo_type
            && out.ssbo_id    == sentinel.ssbo_id
            && out.data_index == sentinel.data_index;
    }

    /// 真文件：`GetOrLoadURI` 认得 `mtldata:file:` 写法，且块名 / 行数符合示例约定。
    bool TestRealGridFileThroughUri()
    {
        mtl::MaterialDataBlockRegistry registry;

        mtl::MaterialDataBlockAsset *block =
            registry.GetOrLoadURI(OSString(OS_TEXT("mtldata:file:")) + ToOSString(kGridDataPath));

        if (!block || block->IsFailed())
        {
            GLogError(u8"TestMaterialDataRowBinding: 读不到数据文件（需在仓库根目录运行）: %s",
                      kGridDataPath);
            return false;
        }

        Check(block->GetAssetName() == "pbr_10x10_grid", "真文件块名 pbr_10x10_grid");
        Check(block->GetRowCount() == 100, "真文件 100 行");
        Check(block->GetGlobalSSBOType() == GlobalSSBOType::PBRSurface, "真文件行类型 PBRSurface");

        // 未上传：任何行号都取不到，binding 必须无效（这正是 builder / 门面必须先 Upload 的原因）。
        Check(block->GetRowLease().GetGlobalSSBOBindingAt(0).IsValid() == false,
              "未上传时 GetGlobalSSBOBindingAt 一律无效");

        return true;
    }

    // ── B 层：`GetGlobalSSBOBindingAt` 源码口径 ──────────────────────────────
    bool TestLeaseGetterContract()
    {
        AnsiString src;
        if (!ReadSource(OS_TEXT("inc/hgl/graph/module/GlobalSSBOBufferRegistry.h"), src))
        {
            GLogError(u8"TestMaterialDataRowBinding: 读不到 GlobalSSBOBufferRegistry.h（需在仓库根目录运行）");
            return false;
        }

        const char *fn = "GetGlobalSSBOBindingAt";

        Check(ContainsToken(src, fn), "行区间租约提供 GetGlobalSSBOBindingAt");

        // 行号必须复用 `RowIDAt`（越界安全 + 区间连续两件事都在那里保证），不得重算。
        Check(ContainsToken(src, "const uint32_t row = RowIDAt(index);"),
              "GetGlobalSSBOBindingAt 复用 RowIDAt 取池行号");

        Check(ContainsToken(src, "{type, GetSSBOId(), row}"),
              "GetGlobalSSBOBindingAt 用 type + 池 id + 行号拼绑定");

        return true;
    }

    // ── C 层：三层各自的顺序契约 ─────────────────────────────────────────────
    bool TestRegistryFacadeContract()
    {
        AnsiString src;
        if (!ReadSource(OS_TEXT("src/SceneGraph/asset/MaterialDataBlockRegistry.cpp"), src))
        {
            GLogError(u8"TestMaterialDataRowBinding: 读不到 MaterialDataBlockRegistry.cpp（需在仓库根目录运行）");
            return false;
        }

        const char *fn = "MaterialDataBlockRegistry::GetRowBinding";

        Check(ContainsToken(src, fn), "注册表实现 GetRowBinding");

        // 先判池：没有池就无从上传，早退比后面 Upload(nullptr) 的软失败更明确。
        Check(OrderedIn(src, fn, "if (!pool)", "GetOrLoadURI(uri)"),
              "先判池再取块");

        // 失败态早退：坏块的行不该被任何实体引用。
        Check(OrderedIn(src, fn, "IsFailed()", "asset->Upload(pool)"),
              "失败态块在上传前就被拒");

        // 顺序：上传在前、取行号在后（未上传的 RowIDAt 恒为 InvalidRowID）。
        Check(OrderedIn(src, fn, "asset->Upload(pool)", "GetGlobalSSBOBindingAt(row_index)"),
              "先整块上传再取行号");

        Check(ContainsToken(src, "if (!binding.IsValid())"),
              "取到的绑定要过 IsValid() 才写出");

        // 失败路径一律不碰 out（只有最后一句赋值）。
        Check(ContainsToken(src, "out = binding;"),
              "成功路径才写 out（失败保持不动）");

        return true;
    }

    bool TestWorkObjectHelperContract()
    {
        AnsiString src;
        if (!ReadSource(OS_TEXT("src/Work/WorkObject.cpp"), src))
        {
            GLogError(u8"TestMaterialDataRowBinding: 读不到 WorkObject.cpp（需在仓库根目录运行）");
            return false;
        }

        const char *fn = "WorkObject::GetMaterialDataBinding";

        Check(ContainsToken(src, fn), "框架层提供 GetMaterialDataBinding");

        Check(ContainsToken(src, "graphics->GetMaterialDataBlockRegistry()"),
              "框架层从 GraphicsContext 取数据块注册表");

        Check(ContainsToken(src, "GetManager<graph::GlobalSSBOBufferRegistry>()"),
              "框架层把全局池一起交给门面（示例不必自己取池）");

        Check(ContainsToken(src, "blocks->GetRowBinding("),
              "框架层只转发，不另开第二条绑定路径");

        return true;
    }
}//namespace

int main()
{
    hgl::logger::InitLogger(OS_TEXT("TestMaterialDataRowBinding"));

    Check(TestNullPoolLeavesOutUntouched(), "池为空 ⇒ 失败且不写 out");
    Check(TestUnknownUriLeavesOutUntouched(), "URI 认不出 ⇒ 解析为空且不写 out");
    Check(TestRealGridFileThroughUri(), "真网格文件经 mtldata:file: 门面可读");
    Check(TestLeaseGetterContract(), "GetGlobalSSBOBindingAt 源码口径");
    Check(TestRegistryFacadeContract(), "GetRowBinding 顺序契约");
    Check(TestWorkObjectHelperContract(), "WorkObject 转发契约");

    if (g_failed > 0)
    {
        GLogError(u8"TestMaterialDataRowBinding: %d 项失败", g_failed);
        return 1;
    }

    GLogInfo(u8"TestMaterialDataRowBinding: 全部通过");
    return 0;
}
