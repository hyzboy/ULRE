/**
 * TestGlobalSSBORowRange —— 全局 SSBO **连续行区间**与**数据块整段上传**。
 *
 * ── 为什么要这么切 ──────────────────────────────────────────────────────────
 *
 * 行池的真机分配（`ActiveRowPool::Create`）需要一张 VkBuffer，而 `src/ecs/tests`
 * 下**没有图形设备**（同 `TestCSMIncrementalPass` 的说明）。所以这里分三层验证：
 *
 * · A 层（纯数据，可完全执行）：授权行 → 物理行 → `MDB1` 字节 → **零拷贝视图**，
 *   证明「文件行区可以直通 GPU」这一前提成立，并钉住行宽口径；
 * · B 层（无设备时真实可执行）：租约的无效状态全程 fail-closed、Release 幂等、
 *   移动后源不再归还；
 * · C 层（源码契约）：真机才能观察的分配策略（永不复用 Idle / 编号连续校验 /
 *   整段回滚 / 单次 MarkDirty / 行距 fail-fast）用源码契约钉住，防止日后被改回去。
 */

#include <hgl/graph/module/GlobalSSBOBufferRegistry.h>
#include <hgl/graph/ssbo/MaterialDataRows.h>
#include <hgl/mtl/MaterialDataBlock.h>
#include <hgl/mtl/MaterialDataBlockFile.h>
#include <hgl/io/FileInputStream.h>
#include <hgl/log/Log.h>
#include <hgl/log/Logger.h>

#include <cstring>

namespace
{
    using namespace hgl;
    using namespace hgl::graph;

    int g_failed = 0;

    void Check(const bool ok, const char *what)
    {
        if (!ok)
        {
            ++g_failed;
            GLogError(u8"TestGlobalSSBORowRange 失败: %s", what);
        }
    }

    /// 8 行 PBR 块：行列交叉的有色/金属/粗糙组合，便于按行定位错位。
    const char *kBlock8Rows =
        "schema = 1\n"
        "name = \"pbr_range\"\n"
        "target = \"PBRSurface\"\n"
        "\n"
        "[[rows]]\n"
        "base_color = [0.0625, 0.125, 0.25, 1.0]\n"
        "metallic = 0.0\n"
        "roughness = 0.05\n"
        "\n"
        "[[rows]]\n"
        "base_color = [0.125, 0.25, 0.5, 1.0]\n"
        "metallic = 0.125\n"
        "roughness = 0.2\n"
        "\n"
        "[[rows]]\n"
        "base_color = [0.25, 0.5, 0.75, 1.0]\n"
        "metallic = 0.25\n"
        "roughness = 0.4\n"
        "\n"
        "[[rows]]\n"
        "base_color = [0.375, 0.5, 0.625, 1.0]\n"
        "metallic = 0.375\n"
        "roughness = 0.6\n"
        "\n"
        "[[rows]]\n"
        "base_color = [0.5, 0.5, 0.5, 1.0]\n"
        "metallic = 0.5\n"
        "roughness = 0.8\n"
        "\n"
        "[[rows]]\n"
        "base_color = [0.625, 0.5, 0.375, 1.0]\n"
        "metallic = 0.625\n"
        "roughness = 0.9\n"
        "\n"
        "[[rows]]\n"
        "base_color = [0.75, 0.5, 0.25, 1.0]\n"
        "metallic = 0.75\n"
        "roughness = 0.95\n"
        "\n"
        "[[rows]]\n"
        "base_color = [1.0, 0.75, 0.5, 1.0]\n"
        "metallic = 1.0\n"
        "roughness = 1.0\n";

    bool BuildBlock(mtl::MaterialDataBlock &out_block, MaterialDataBlockSpec &out_spec)
    {
        mtl::MaterialDataBlockFileData data;

        if (mtl::ParseMaterialDataBlockFile(kBlock8Rows,
                                          static_cast<int>(strlen(kBlock8Rows)),
                                          data, &out_spec)
            != mtl::MaterialDataBlockParseResult::OK)
            return false;

        out_block = std::move(data.block);
        return out_block.IsValid();
    }

    // ── A 层：块 → 视图（零解析、零拷贝） ────────────────────────────────────
    bool TestBlockViewFromToml()
    {
        mtl::MaterialDataBlock block;
        MaterialDataBlockSpec spec;

        if (!BuildBlock(block, spec))
            return false;

        // 行宽口径：文件摊平出来的行距必须恰好等于 GPU 物理行宽。
        if (block.GetRowCount() != 8
            || block.row_bytes != sizeof(ssbo::PBRSurfaceRow)
            || block.row_bytes != GetGlobalSSBOTypeStructStride(GlobalSSBOType::PBRSurface))
            return false;

        const mtl::MaterialDataBlockView view = mtl::MakeBlockView(block);

        if (!view.IsValid()
            || view.type != GlobalSSBOType::PBRSurface
            || view.row_bytes != block.row_bytes
            || view.row_count != block.GetRowCount()
            || view.rows != block.GetData()
            || view.GetByteSize() != block.GetByteSize())
            return false;

        // 逐行回读：块内第 i 行的 32 字节就是第 i 规约行摊平的结果。
        const auto *rows = reinterpret_cast<const ssbo::PBRSurfaceRow *>(view.rows);
        for (uint32 i = 0; i < view.row_count; ++i)
        {
            const auto &src = spec.rows[static_cast<int>(i)];

            if (rows[i].base_color.x != src.base_color.x
                || rows[i].base_color.y != src.base_color.y
                || rows[i].base_color.z != src.base_color.z
                || rows[i].base_color.w != src.base_color.w
                || rows[i].metallic != src.metallic
                || rows[i].roughness != src.roughness)
                return false;
        }

        // 空块 ⇒ 非法视图（没有行区就不可上船）。
        const mtl::MaterialDataBlockView empty = mtl::MakeBlockView(mtl::MaterialDataBlock{});
        return !empty.IsValid() && empty.rows == nullptr;
    }

    // ── A 层：MDB1 字节 → 视图（零拷贝直通 GPU 的依据） ──────────────────────
    bool TestBinaryViewIsZeroCopy()
    {
        mtl::MaterialDataBlock block;
        MaterialDataBlockSpec spec;

        if (!BuildBlock(block, spec))
            return false;

        ValueArray<uint8> binary;
        if (!mtl::WriteMaterialDataBlockBinary(block, binary))
            return false;

        if (binary.GetCount() != static_cast<int>(mtl::kMaterialDataBlockBinaryHeaderSize)
                                     + static_cast<int>(block.GetByteSize()))
            return false;

        mtl::MaterialDataBlockView view;
        AnsiString error;

        if (!mtl::ViewMaterialDataBlockBinary(binary.GetData(),
                                            static_cast<uint64>(binary.GetCount()),
                                            view, error))
            return false;

        // 零解析：视图行区就是缓冲 + 16，且与拥有型 payload 逐字节相同 ——
        // 上传时只需一次 memcpy，没有任何逐字段搬运。
        if (view.rows != binary.GetData() + mtl::kMaterialDataBlockBinaryHeaderSize)
            return false;

        if (!view.IsValid()
            || view.type != block.type
            || view.row_bytes != block.row_bytes
            || view.row_count != block.GetRowCount()
            || view.GetByteSize() != block.GetByteSize())
            return false;

        return memcmp(view.rows, block.GetData(), block.GetByteSize()) == 0;
    }

    // ── A 层：坏头必须挡在视图层（不让错行距走到池边上） ────────────────────
    bool TestBinaryViewRejectsBadRowBytes()
    {
        mtl::MaterialDataBlock block;
        MaterialDataBlockSpec spec;

        if (!BuildBlock(block, spec))
            return false;

        ValueArray<uint8> binary;
        if (!mtl::WriteMaterialDataBlockBinary(block, binary))
            return false;

        // 行距改成 16（Emissive 宽度）但行数不变 —— 铺开就会跨行踩坏相邻数据。
        const uint32 bad_row_bytes = sizeof(ssbo::EmissiveSurfaceRow);
        memcpy(binary.GetData() + 8, &bad_row_bytes, sizeof(bad_row_bytes));

        mtl::MaterialDataBlockView view;
        AnsiString error;

        if (mtl::ViewMaterialDataBlockBinary(binary.GetData(),
                                            static_cast<uint64>(binary.GetCount()),
                                            view, error))
            return false;

        if (error.IsEmpty() || view.IsValid())
            return false;

        // 行区被截断（头声明 8 行但只给 4 行字节）同样必须拒绝。
        ValueArray<uint8> truncated;
        if (!mtl::WriteMaterialDataBlockBinary(block, truncated))
            return false;

        const uint64 keep = static_cast<uint64>(truncated.GetCount())
                          - static_cast<uint64>(block.row_bytes) * 4;

        return !mtl::ViewMaterialDataBlockBinary(truncated.GetData(), keep, view, error);
    }

    // ── B 层：无效租约全程 fail-closed、Release 幂等 ─────────────────────────
    //
    // 池的真机分配策略需要一个真实 VkDevice 才能观察（GraphicsContext::Initialize()
    // 在 device 为空时直接失败，注册表构造口也是 private/friend GraphModuleManager），
    // 所以这里钉住的是调用方一定会碰到的「无效租约」语义：任何取用都返回 Invalid 哨兵，
    // 反复 Release 不产生副作用。区间连续/分批/回滚这些只能靠 C 层源码契约钉。
    bool TestInvalidLeaseIsFailClosed()
    {
        GlobalSSBORowRangeLease lease;

        if (lease.IsValid() || static_cast<bool>(lease))
            return false;

        if (lease.GetFirstRow() != ActiveRowPool::InvalidRowID
            || lease.GetLastRow() != ActiveRowPool::InvalidRowID
            || lease.GetRowCount() != 0)
            return false;

        // 越界/无效一律 Invalid 哨兵，绝不返回 0（0 可能是真实行号）。
        for (uint32 i = 0; i < 4; ++i)
        {
            if (lease.RowIDAt(i) != ActiveRowPool::InvalidRowID)
                return false;
        }

        if (lease.GetSSBOId() != 0)
            return false;

        // Release 幂等：默认/已归还的租约反复 Release 都不出事。
        lease.Release();
        lease.Release();

        if (lease.IsValid() || lease.GetFirstRow() != ActiveRowPool::InvalidRowID
            || lease.GetRowCount() != 0)
            return false;

        // 移动语义：源变无效、目标接管；移走后再 Release 源不能二次归还。
        GlobalSSBORowRangeLease moved(std::move(lease));
        if (moved.IsValid() || lease.IsValid())
            return false;

        lease.Release();

        if (lease.GetFirstRow() != ActiveRowPool::InvalidRowID || moved.GetRowCount() != 0)
            return false;

        // 自移动不得自毁（实现里靠 this != &other 挡）。
        GlobalSSBORowRangeLease &self = moved;
        moved = std::move(self);

        return !moved.IsValid() && moved.GetRowCount() == 0;
    }

    // ── C 层：源码契约（真机才可观察的分配策略） ─────────────────────────────
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

    /// 在 [begin_needle, end_needle) 这段源码里找 needle（用于「某函数体内不得出现 X」）。
    bool SegmentContains(const AnsiString &src, const char *begin_needle,
                        const char *end_needle, const char *needle)
    {
        const char *begin = strstr(src.c_str(), begin_needle);
        if (!begin)
            return false;

        const char *end = strstr(begin, end_needle);
        if (!end)
            return false;

        const AnsiString segment(begin, static_cast<int>(end - begin));
        return strstr(segment.c_str(), needle) != nullptr;
    }

    bool ContainsToken(const AnsiString &src, const char *needle)
    {
        return strstr(src.c_str(), needle) != nullptr;
    }

    bool TestSourceContracts()
    {
        AnsiString pool_src;
        AnsiString registry_src;

        if (!ReadSource(OS_TEXT("src/Vulkan/buffer/ActiveRowPool.cpp"), pool_src)
            || !ReadSource(OS_TEXT("src/SceneGraph/module/GlobalSSBOBufferRegistry.cpp"), registry_src))
        {
            GLogError(u8"TestGlobalSSBORowRange: 无法读取源文件（需在仓库根目录运行）");
            return false;
        }

        // 「块内第 i 行 == first + i」的前提：连续性只能来自水位之后的全新编号。
        const bool has_watermark = ContainsToken(pool_src, "ids.GetHistoryMaxId()");
        const bool has_rollback_partial = ContainsToken(pool_src, "ReleaseRun(first, done);");
        const bool has_rollback_batch = ContainsToken(pool_src, "ReleaseRun(first, done + batch);");
        const bool has_order_check = ContainsToken(pool_src,
            "created[i] != static_cast<int>(first + done + i)");
        const bool has_single_markdirty = ContainsToken(pool_src,
            "MarkDirty(VkDeviceSize(first) * row_bytes, VkDeviceSize(count) * row_bytes)");
        const bool reuses_idle = SegmentContains(pool_src,
            "ActiveRowPool::AcquireRun", "ActiveRowPool::ReleaseRun", "GetIdle()");

        const bool registry_row_check = ContainsToken(registry_src,
            "view.row_bytes != pool->GetRowBytes()");
        const bool registry_release_on_fail = ContainsToken(registry_src, "lease.Release();");

        const struct
        {
            bool        ok;
            const char *why;
        } kChecks[] =
        {
            { has_watermark, "AcquireRun 的连续性必须来自水位（GetHistoryMaxId）" },
            { has_rollback_partial, "CreateActive 失败必须整段回滚已拿到的行" },
            { has_rollback_batch, "编号漂移必须回滚包含本批在内的整段区间" },
            { has_order_check, "逐批校验编号连续：块内第 i 行必须恒为 first + i" },
            { has_single_markdirty, "CommitRun 必须一次标脏整段（而不是逐行提交）" },
            { registry_row_check, "数据块上传必须在入口校验行距（错步长会静默写坏相邻内容行）" },
            { registry_release_on_fail, "写失败必须归还区间，不留半截数据" },
        };

        constexpr uint32 kCount = sizeof(kChecks) / sizeof(kChecks[0]);

        bool ok = true;
        for (uint32 i = 0; i < kCount; ++i)
        {
            if (!kChecks[i].ok)
            {
                ok = false;
                GLogError(u8"TestGlobalSSBORowRange 失败: %s", kChecks[i].why);
            }
        }

        if (reuses_idle)
        {
            ok = false;
            GLogError(u8"TestGlobalSSBORowRange 失败: AcquireRun 里重新出现 GetIdle()"
                       "（Idle 复用给出零散行号 ⇒ 块内下标映射不再成立）");
        }

        return ok;
    }
}//namespace

int main()
{
    hgl::logger::InitLogger(OS_TEXT("TestGlobalSSBORowRange"));

    Check(TestBlockViewFromToml(), "块→视图：行宽口径与逐行摊平");
    Check(TestBinaryViewIsZeroCopy(), "MDB1→视图：零拷贝直通");
    Check(TestBinaryViewRejectsBadRowBytes(), "坏行距/截断行区必须拒绝");
    Check(TestInvalidLeaseIsFailClosed(), "无效租约 fail-closed 且 Release 幂等");
    Check(TestSourceContracts(), "分配策略源码契约");

    if (g_failed > 0)
    {
        GLogError(u8"=== TestGlobalSSBORowRange: %d 项失败 ===", g_failed);
        return 1;
    }

    GLogInfo(u8"=== TestGlobalSSBORowRange: 全部测试通过 ===");
    return 0;
}
