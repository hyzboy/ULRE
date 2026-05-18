// SFMAuditSnapshotTests — Phase 6 SFM vs row.resources 对比审计工具
//
// 调用 GetBuiltinMaterialPresetAuditSnapshot() 获取全量快照，
// 解析 sfm_vs_row_resources 列，打印所有 MISMATCH 行供人工审查，
// 并在存在任何 MISMATCH 时以非零退出码退出（作为 CI 回归守卫）。

#include <hgl/mtl/MaterialLibrary.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <sstream>

// ---------------------------------------------------------------------------
// 简易 CSV 行分割（以 | 为分隔符）
// ---------------------------------------------------------------------------
static std::vector<std::string> SplitPipe(const std::string &line)
{
    std::vector<std::string> cols;
    std::istringstream ss(line);
    std::string col;
    while (std::getline(ss, col, '|'))
        cols.push_back(col);
    return cols;
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main()
{
    const std::string snapshot = hgl::graph::mtl::GetBuiltinMaterialPresetAuditSnapshot();

    // --- 1. 转储完整快照 --------------------------------------------------
    std::printf("=== Full Builtin Preset Audit Snapshot ===\n%s\n", snapshot.c_str());

    // --- 2. 解析列索引 ----------------------------------------------------
    // 找到 header 行（以 "preset|" 开头）
    std::istringstream all(snapshot);
    std::string line;
    int header_idx_sfm_inferred       = -1;
    int header_idx_sfm_vs_row         = -1;
    int header_idx_row_name           = -1;
    int header_idx_preset             = -1;

    std::vector<std::string> mismatch_lines;
    std::vector<std::string> fail_lines;
    bool in_data = false;

    while (std::getline(all, line))
    {
        if (line.empty() || line[0] == '#')
            continue;

        if (!in_data)
        {
            // 首次非注释非空行是 header
            const auto cols = SplitPipe(line);
            for (int i = 0; i < static_cast<int>(cols.size()); ++i)
            {
                if (cols[i] == "preset")               header_idx_preset       = i;
                if (cols[i] == "row_name")             header_idx_row_name     = i;
                if (cols[i] == "sfm_inferred")         header_idx_sfm_inferred = i;
                if (cols[i] == "sfm_vs_row_resources") header_idx_sfm_vs_row   = i;
            }
            in_data = true;
            continue;
        }

        // 数据行
        const auto cols = SplitPipe(line);
        const int ncols = static_cast<int>(cols.size());

        const std::string preset   = (header_idx_preset   >= 0 && header_idx_preset   < ncols) ? cols[header_idx_preset]   : "?";
        const std::string row_name = (header_idx_row_name >= 0 && header_idx_row_name < ncols) ? cols[header_idx_row_name] : "?";
        const std::string sfm_inf  = (header_idx_sfm_inferred >= 0 && header_idx_sfm_inferred < ncols) ? cols[header_idx_sfm_inferred] : "";
        const std::string sfm_vs   = (header_idx_sfm_vs_row   >= 0 && header_idx_sfm_vs_row   < ncols) ? cols[header_idx_sfm_vs_row]   : "";

        const bool is_assembly_fail = sfm_inf.find("FAIL") != std::string::npos;

        if (sfm_vs.rfind("MISMATCH", 0) == 0 && !is_assembly_fail)
            mismatch_lines.push_back("  [MISMATCH] preset=" + preset + " row=" + row_name + " " + sfm_vs + " (sfm_inferred=" + sfm_inf + ")");

        if (is_assembly_fail)
            fail_lines.push_back("  [ASSEMBLY_FAIL] preset=" + preset + " row=" + row_name + " sfm_inferred=" + sfm_inf);
    }

    // --- 3. 汇总报告 -------------------------------------------------------
    std::printf("\n=== SFM Audit Summary ===\n");
    std::printf("Assembly failures : %zu\n", fail_lines.size());
    std::printf("MISMATCH rows     : %zu\n", mismatch_lines.size());

    if (!fail_lines.empty())
    {
        std::printf("\n-- Assembly Failures --\n");
        for (const auto &s : fail_lines)
            std::printf("%s\n", s.c_str());
    }

    if (!mismatch_lines.empty())
    {
        std::printf("\n-- MISMATCH Detail --\n");
        for (const auto &s : mismatch_lines)
            std::printf("%s\n", s.c_str());
    }

    if (fail_lines.empty() && mismatch_lines.empty())
        std::printf("\nAll rows OK — SFM inferred fields match row.resources declarations.\n");

    // ASSEMBLY_FAIL 属于已知的非标准路径（2D / custom template），不触发 CI 失败。
    // 只有真实 MISMATCH（SFM 推导结果与 row.resources 不一致）才使测试失败。
    return mismatch_lines.empty() ? 0 : 1;
}
