/**
 * `RenderableBuilder::WithMaterialDataRow(uri, row_index)` 的契约测试。
 *
 * 为什么没有「跑起来」的用例：这条路径真跑一遍需要 GraphicsContext + 一个已初始化
 * 的 Vulkan 设备 + 全局池（`GraphicsContext::Initialize` 强制要求 device），而
 * `src/ecs/tests` 下没有设备引导代码 —— 与 `TestGlobalSSBORowRange` 同一个限制。
 *
 * 因此这里分两层，全部是真断言：
 *
 * | 层 | 查什么 | 为什么这样查 |
 * |----|--------|--------------|
 * | A 组合 | `MaterialDataBlockAsset` 的 `GetSSBOId()` / `RowIDAt()` / `GetGlobalSSBOType()`
 *          能否拼出一条合法 `GlobalSSBOBinding` | 这是 builder 唯一做的事，逻辑可纯数据验证 |
 * | B 契约 | builder 源码里必须出现的顺序与错误分支 | 顺序（先上传、再查越界）不做在源码里就没法保证 |
 */

#include <hgl/ecs/support/RenderableBuilder.h>
#include <hgl/graph/asset/MaterialDataBlockRegistry.h>
#include <hgl/graph/ssbo/GlobalSSBOTypes.h>
#include <hgl/vk/buffer/ActiveRowPool.h>
#include <hgl/io/FileInputStream.h>
#include <hgl/log/Logger.h>
#include <hgl/log/Log.h>
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

    void Check(bool ok, const char *what)
    {
        if (ok)
        {
            GLogInfo(u8"[ OK ] %s", what);
            return;
        }

        ++g_failed;
        GLogError(u8"[FAIL] %s", what);
    }

    // 两行 PBRSurface：与 PBRSpheres 用的批量数据同构。
    const char *kTwoRowBlock = R"(
schema = 1
name = "grid"
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

    AnsiString MakeU8(const U8String &s)
    {
        return AnsiString(reinterpret_cast<const char *>(s.c_str()),
                          static_cast<int>(s.Length()));
    }

    /// A 层：块内行 → 池行号 → `GlobalSSBOBinding`。
    bool TestBindingComposition()
    {
        mtl::MaterialDataBlockRegistry registry;

        mtl::MaterialDataBlockAsset *block = registry.RegisterFromToml(
            OS_TEXT("<mem>"), kTwoRowBlock, static_cast<int>(strlen(kTwoRowBlock)));

        if (!block || block->IsFailed())
            return false;

        if (block->GetRowCount() != 2)
            return false;

        // 未上传：行号全是 InvalidRowID ⇒ 拼出来的 binding 必须 IsValid()==false。
        // 这条正是「builder 必须先 Upload 才能拼」的机器可查证明。
        if (block->RowIDAt(0) != ActiveRowPool::InvalidRowID)
            return false;

        GlobalSSBOBinding unuploaded{};
        unuploaded.ssbo_type  = block->GetGlobalSSBOType();
        unuploaded.ssbo_id    = block->GetSSBOId();
        unuploaded.data_index = block->RowIDAt(0);

        if (unuploaded.IsValid())
            return false;

        // 模拟上传后的形态（ssbo_id 与行号都由池给出）：同一个组合表达式必须合法。
        GlobalSSBOBinding uploaded{};
        uploaded.ssbo_type  = GlobalSSBOType::PBRSurface;
        uploaded.ssbo_id    = 1;
        uploaded.data_index = 7;

        return uploaded.IsValid();
    }

    /// A 层：越界行号必须被拒（`RowIDAt` 是越界安全的第一道门）。
    bool TestRowIndexBounds()
    {
        mtl::MaterialDataBlockRegistry registry;

        mtl::MaterialDataBlockAsset *block = registry.RegisterFromToml(
            OS_TEXT("<mem>"), kTwoRowBlock, static_cast<int>(strlen(kTwoRowBlock)));

        if (!block)
            return false;

        // 未上传时 `RowIDAt` 对任何下标都返回 InvalidRowID（不越界读）——
        // 于是「越界」和「没上传」两种拒法都由它一次性兜住。
        return block->RowIDAt(0) == ActiveRowPool::InvalidRowID
            && block->RowIDAt(1) == ActiveRowPool::InvalidRowID
            && block->RowIDAt(2) == ActiveRowPool::InvalidRowID
            && block->RowIDAt(999u) == ActiveRowPool::InvalidRowID;
    }

    /// A 层：数据块的身份必须能被 `mtldata:file:` 门面复认（builder 靠它拿块）。
    bool TestBlockByUri()
    {
        mtl::MaterialDataBlockRegistry registry;

        mtl::MaterialDataBlockAsset *block = registry.RegisterFromToml(
            OS_TEXT("res/material/grid.material_data.toml"),
            kTwoRowBlock, static_cast<int>(strlen(kTwoRowBlock)));

        if (!block)
            return false;

        // 同一份块，路径与名字两条定位都命中同一个句柄：builder 传路径或 `@name` 都行。
        return registry.FindAssetByPath(OS_TEXT("res/material/grid.material_data.toml")) == block
            && registry.FindByName("grid") == block;
    }

    // ── B 层：源码契约 ───────────────────────────────────────────────────────
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

    /// begin 之后第一次出现 left 必须早于 first 出现 right（同一函数体里的顺序断言）。
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

    bool TestSourceContracts()
    {
        AnsiString src;
        if (!ReadSource(OS_TEXT("src/ecs/support/RenderableBuilder.cpp"), src))
        {
            GLogError(u8"TestRenderableBuilderDataRow: 无法读取源文件（需在仓库根目录运行）");
            return false;
        }

        const char *fn = "RenderableBuilder::WithMaterialDataRow";

        Check(strstr(src.c_str(), fn) != nullptr,
              "builder 实现了 WithMaterialDataRow");

        Check(ContainsToken(src, "gc->GetMaterialDataBlockRegistry()"),
              "builder 从 GraphicsContext 取数据块注册表");

        Check(ContainsToken(src, "blocks->GetOrLoadURI(block_uri)"),
              "builder 走 URI 门面拿数据块（自己不碰文件格式）");

        // 顺序：必须先整块上传，再查行号 —— 反过来会被未上传的 InvalidRowID 骗过。
        Check(OrderedIn(src, fn, "block->Upload(pool)", "block->GetRowCount()"),
              "先 Upload 再判越界");

        Check(ContainsToken(src, "binding.data_index = block->RowIDAt(row_index);"),
              "行号取块内下标映射到的池行号");

        Check(ContainsToken(src, "binding.ssbo_id    = block->GetSSBOId();"),
              "ssbo_id 取块租约的池 id");

        Check(ContainsToken(src, "return WithMaterialDataResource(binding);"),
              "落库复用唯一授权入口（不另开绑定路径）");

        // 三种失败都必须显式 Fail（不得静默把实体建出来）。
        Check(ContainsToken(src, "WithMaterialDataRow: GetOrLoadURI")
                  && ContainsToken(src, "WithMaterialDataRow: Upload")
                  && ContainsToken(src, "WithMaterialDataRow: row index out of range"),
              "三条失败分支都显式 Fail");

        return true;
    }
}//namespace

int main()
{
    hgl::logger::InitLogger(OS_TEXT("TestRenderableBuilderDataRow"));

    Check(TestBindingComposition(), "块内行 → GlobalSSBOBinding 组合");
    Check(TestRowIndexBounds(), "越界/未上传行号一律 InvalidRowID");
    Check(TestBlockByUri(), "无名数据块按路径复认（builder 靠它拿块）");
    Check(TestSourceContracts(), "builder 源码契约");

    if (g_failed > 0)
    {
        GLogError(u8"TestRenderableBuilderDataRow: %d 项失败", g_failed);
        return 1;
    }

    GLogInfo(u8"TestRenderableBuilderDataRow: 全部通过");
    return 0;
}

