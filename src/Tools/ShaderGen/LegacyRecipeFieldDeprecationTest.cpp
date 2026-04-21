/// LegacyRecipeFieldDeprecationTest.cpp
///
/// Phase 8 — 旧字段弃用检测回归测试
///
/// 验证 CheckRecipeLegacyFieldUsage() 的三类场景：
///   PASS-A: intent_features != 0  → 无警告（已迁移）
///   PASS-B: 所有旧字段为默认值    → 无警告（不构成"有意选择"）
///   PASS-C: dim == D2             → 无警告（2D 配方不存在这些字段）
///   WARN-D: camera 被设为 false   → 触发警告
///   WARN-E: sky 被设为 true       → 触发警告
///   WARN-F: lighting 非 Lambert   → 触发警告

#include <hgl/graph/module/MaterialAssetLoader.h>
#include <hgl/mtl/MaterialRecipe.h>
#include <hgl/mtl/MaterialFeature.h>

#include <cstdio>
#include <string>

using namespace hgl::graph;
using namespace hgl::graph::mtl;

namespace
{
    // 构造一个"全默认"3D 配方（camera=true, sky=false, lighting=Lambert, intent_features=0）
    mtl::MaterialRecipe MakeDefault3D()
    {
        mtl::MaterialRecipe rec;
        rec.preset         = mtl::MaterialPreset::Standard;
        rec.dim            = mtl::MaterialRecipe::Dim::D3;
        rec.camera         = true;
        rec.sky            = false;
        rec.lighting       = mtl::LightingModel::Lambert;
        rec.intent_features = 0;
        return rec;
    }

    int g_pass = 0;
    int g_fail = 0;

    void Check(const char *label, bool actual, bool expected)
    {
        const bool ok = (actual == expected);
        std::fprintf(stdout, "  [%s] CheckRecipeLegacyFieldUsage = %s  (expected %s) → %s\n",
            label,
            actual   ? "true"  : "false",
            expected ? "true"  : "false",
            ok ? "OK" : "FAIL");
        if (ok) ++g_pass; else ++g_fail;
    }
}

int main()
{
    std::fprintf(stdout, "[LegacyRecipeFieldDeprecationTest] Phase 8 — 旧字段弃用检测\n\n");

    // ── PASS-A: intent_features 已设，旧字段应被忽略（相当于已迁移）──────────
    {
        auto rec = MakeDefault3D();
        rec.sky            = true;                          // 旧字段有非默认值
        rec.intent_features = ToFeatureMask(MaterialFeature::NeedsSky);  // 但已用新 API
        Check("PASS-A intent_features!=0 overrides legacy",
              CheckRecipeLegacyFieldUsage(rec), false);
    }

    // ── PASS-B: 全默认值，无需警告 ────────────────────────────────────────────
    {
        auto rec = MakeDefault3D();   // camera=true, sky=false, lighting=Lambert
        Check("PASS-B all defaults, no warning",
              CheckRecipeLegacyFieldUsage(rec), false);
    }

    // ── PASS-C: 2D 配方，字段无意义，不警告 ──────────────────────────────────
    {
        auto rec = MakeDefault3D();
        rec.dim  = mtl::MaterialRecipe::Dim::D2;
        rec.sky  = true;             // 非默认，但是 2D 配方
        Check("PASS-C dim==D2 skips check",
              CheckRecipeLegacyFieldUsage(rec), false);
    }

    // ── WARN-D: camera 被明确关闭（非默认 true）──────────────────────────────
    {
        auto rec = MakeDefault3D();
        rec.camera = false;
        std::fprintf(stdout, "  >>> expect deprecation warning below:\n");
        Check("WARN-D camera=false triggers warning",
              CheckRecipeLegacyFieldUsage(rec), true);
    }

    // ── WARN-E: sky 被明确启用 ────────────────────────────────────────────────
    {
        auto rec = MakeDefault3D();
        rec.sky = true;
        std::fprintf(stdout, "  >>> expect deprecation warning below:\n");
        Check("WARN-E sky=true triggers warning",
              CheckRecipeLegacyFieldUsage(rec), true);
    }

    // ── WARN-F: lighting 非 Lambert ───────────────────────────────────────────
    {
        auto rec = MakeDefault3D();
        rec.lighting = mtl::LightingModel::BlinnPhong;
        std::fprintf(stdout, "  >>> expect deprecation warning below:\n");
        Check("WARN-F lighting!=Lambert triggers warning",
              CheckRecipeLegacyFieldUsage(rec), true);
    }

    std::fprintf(stdout, "\n[LegacyRecipeFieldDeprecationTest] Results: %d PASS / %d FAIL\n",
        g_pass, g_fail);

    return (g_fail == 0) ? 0 : 1;
}
