#include <hgl/ecs/support/RenderStrategyTable.h>
#include <hgl/ecs/support/RenderStrategyParity.h>
#include <hgl/log/Log.h>
#include <hgl/log/Logger.h>

using namespace hgl;
using namespace hgl::ecs;

namespace
{
    constexpr uint32_t kGeoSlot      = ComponentTypeBit(ComponentType::Geometry);
    constexpr uint32_t kMatRtSlot    = ComponentTypeBit(ComponentType::MaterialRuntime);
    constexpr uint32_t kMatDataSlot  = ComponentTypeBit(ComponentType::MaterialData);

    /// 全部事实为正（主视图）：随后按需翻某个事实来构造用例
    StrategyFacts AllGoodFacts()
    {
        StrategyFacts f;
        f.component_visible   = true;
        f.entity_visible      = true;
        f.has_owner           = true;
        f.renderable          = true;
        f.has_material_source = true;
        f.cast_shadow         = true;
        f.receive_shadow      = true;
        f.shadow_pass         = false;
        f.in_shadow_range     = true;
        return f;
    }

    bool Collect(const uint32_t mask,const StrategyFacts &facts)
    {
        return HasRenderNeed(EvaluateRenderNeed(mask,facts.ToMask()),RenderNeed::CollectForCurrentPass);
    }
}

// 编译期自检：空组件集合 ⇒ 不产生任何"需要槽位"的需求
static_assert(EvaluateRenderNeed(0u,(1u << STRATEGY_FACT_COUNT) - 1u) == 0u,
              "没有组件槽位就不该有任何渲染需求（规则都要求槽位）");
static_assert(ComponentTypeBit(ComponentType::MaterialRuntime) == ComponentTypeBit(ComponentType::MaterialRuntime), "");

/**
 * A1 策略判定表（v2 §9.2 P2）
 *
 *   · 表是**声明式**的：输入 = 组件槽位掩码 + 事实；输出 = 需求位；
 *   · 逐条镜像现有判据（`RenderPrimitiveCollectSystem.cpp:1344-1383`、`:1362-1379`、
 *     `PrimitiveBatchPipeline.cpp:1016`）——对拍装置见 `RenderStrategyParity.h`；
 *   · 规则表的自洽性由头文件里的 static_assert 保证（新增需求/事实自动纳入）。
 */
int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    hgl::logger::InitLogger(OS_TEXT("TestRenderStrategyTable"));

    GLogInfo(u8"=== Testing 渲染策略判定表（A1）===");

    // ─────────────────────────────────────────────────────────────
    // Test 1: 主视图正常路径 —— 有几何、一切正常 ⇒ 进收集 + 参与阴影能力位
    // ─────────────────────────────────────────────────────────────
    {
        const StrategyFacts facts = AllGoodFacts();
        const uint32_t needs = EvaluateRenderNeed(kGeoSlot,facts.ToMask());

        if (!HasRenderNeed(needs,RenderNeed::CollectForCurrentPass))
        {
            GLogError(u8"Test 1 Failed: 正常实体应进当前 pass 收集");
            return 10;
        }

        if (!HasRenderNeed(needs,RenderNeed::ShadowCaster) || !HasRenderNeed(needs,RenderNeed::ShadowReceiver))
        {
            GLogError(u8"Test 1 Failed: 应具备阴影 caster/receiver 能力位");
            return 10;
        }

        if (!HasRenderNeed(needs,RenderNeed::EarlyZ))
        {
            GLogError(u8"Test 1 Failed: 有几何即可进 EarlyZ");
            return 10;
        }

        if (HasRenderNeed(needs,RenderNeed::NeedsMaterialRuntime))
        {
            GLogError(u8"Test 1 Failed: 没有 MaterialRuntime 槽位却要求材质运行期行");
            return 10;
        }

        GLogInfo(u8"Test 1 Passed: 主视图正常路径的需求位正确。");
    }

    // ─────────────────────────────────────────────────────────────
    // Test 2: 阴影 pass 的条件要求（镜像 :1362-1379）
    // ─────────────────────────────────────────────────────────────
    {
        StrategyFacts facts = AllGoodFacts();
        facts.shadow_pass = true;

        if (!Collect(kGeoSlot,facts))
        {
            GLogError(u8"Test 2 Failed: 阴影 pass 下允许投射且在距离内 ⇒ 应进收集");
            return 11;
        }

        StrategyFacts no_cast = facts;
        no_cast.cast_shadow = false;

        if (Collect(kGeoSlot,no_cast))
        {
            GLogError(u8"Test 2 Failed: 阴影 pass 下不允许投射 ⇒ 不得进收集（镜像 :1364）");
            return 11;
        }

        StrategyFacts out_of_range = facts;
        out_of_range.in_shadow_range = false;

        if (Collect(kGeoSlot,out_of_range))
        {
            GLogError(u8"Test 2 Failed: 阴影 pass 下超出距离 ⇒ 不得进收集（镜像 :1376）");
            return 11;
        }

        // 同一实体在主视图下**不受**这两条约束
        StrategyFacts main_pass = no_cast;
        main_pass.shadow_pass = false;

        if (!Collect(kGeoSlot,main_pass))
        {
            GLogError(u8"Test 2 Failed: 主视图不受阴影条件约束");
            return 11;
        }

        GLogInfo(u8"Test 2 Passed: 阴影 pass 的条件要求（cast_shadow / 距离）与现有判据一致，主视图不受影响。");
    }

    // ─────────────────────────────────────────────────────────────
    // Test 3: 可见性/资源/材质来源等事实逐条生效
    // ─────────────────────────────────────────────────────────────
    {
        const char *what = nullptr;

        StrategyFacts f1 = AllGoodFacts(); f1.component_visible = false;
        if (Collect(kGeoSlot,f1)) { what = "组件级不可见"; }

        StrategyFacts f2 = AllGoodFacts(); f2.entity_visible = false;
        if (!what && Collect(kGeoSlot,f2)) { what = "实体级不可见（含祖先继承）"; }

        StrategyFacts f3 = AllGoodFacts(); f3.renderable = false;
        if (!what && Collect(kGeoSlot,f3)) { what = "无渲染资源"; }

        StrategyFacts f4 = AllGoodFacts(); f4.has_material_source = false;
        if (!what && Collect(kGeoSlot,f4)) { what = "无材质来源（镜像 :1381）"; }

        StrategyFacts f5 = AllGoodFacts(); f5.has_owner = false;
        if (!what && Collect(kGeoSlot,f5)) { what = "无 owner"; }

        if (what)
        {
            GLogError(u8"Test 3 Failed: %s 时仍被判为进收集", what);
            return 12;
        }

        // 无几何槽位 ⇒ 不进任何 pass（规则都要求 Geometry 槽位）
        if (Collect(0u,AllGoodFacts()))
        {
            GLogError(u8"Test 3 Failed: 没有几何槽位却进了收集");
            return 12;
        }

        GLogInfo(u8"Test 3 Passed: 可见性/资源/材质来源/owner 逐条生效；无槽位即无需求。");
    }

    // ─────────────────────────────────────────────────────────────
    // Test 4: 组件集合蕴含（implies）与"需要材质运行期行"
    // ─────────────────────────────────────────────────────────────
    {
        if (GetImpliedComponentMask(ComponentType::MaterialRuntime) != (kMatRtSlot | kMatDataSlot))
        {
            GLogError(u8"Test 4 Failed: MaterialRuntime 应蕴含 MaterialData");
            return 13;
        }

        const uint32_t needs = EvaluateRenderNeed(kMatRtSlot,AllGoodFacts().ToMask());

        if (!HasRenderNeed(needs,RenderNeed::NeedsMaterialRuntime))
        {
            GLogError(u8"Test 4 Failed: 有 MaterialRuntime 槽位 ⇒ 应需要材质运行期行");
            return 13;
        }

        if (Collect(kMatRtSlot,AllGoodFacts()))
        {
            GLogError(u8"Test 4 Failed: 只有材质槽位、没有几何槽位 ⇒ 不该进收集");
            return 13;
        }

        GLogInfo(u8"Test 4 Passed: implies 展开 + NeedsMaterialRuntime 判定正确。");
    }

    // ─────────────────────────────────────────────────────────────
    // Test 5: 对拍装置自身可信（能报不一致、不误报一致）
    //   这是"对拍证据"的前提：如果装置永远不报错，零不一致就毫无意义。
    // ─────────────────────────────────────────────────────────────
    {
        const uint64_t before = GetRenderStrategyParityStats().collect_mismatch;

        ParityCheckCollect(true,false,"TestRenderStrategyTable#selftest");
        ParityCheckCollect(false,true,"TestRenderStrategyTable#selftest");

        if (GetRenderStrategyParityStats().collect_mismatch != before + 2)
        {
            GLogError(u8"Test 5 Failed: 对拍装置没有报出故意制造的不一致（计数 %llu → %llu）",
                      (unsigned long long)before,
                      (unsigned long long)GetRenderStrategyParityStats().collect_mismatch);
            return 14;
        }

        const uint64_t equal_before = GetRenderStrategyParityStats().collect_mismatch;

        ParityCheckCollect(true,true,"TestRenderStrategyTable#selftest");
        ParityCheckCollect(false,false,"TestRenderStrategyTable#selftest");

        if (GetRenderStrategyParityStats().collect_mismatch != equal_before)
        {
            GLogError(u8"Test 5 Failed: 相等的判定被误报为不一致");
            return 14;
        }

        GLogInfo(u8"Test 5 Passed: 对拍装置能报不一致、不误报一致（自检计数 %llu → %llu）。",
                 (unsigned long long)before,
                 (unsigned long long)GetRenderStrategyParityStats().collect_mismatch);
    }

    GLogInfo(u8"=== All RenderStrategyTable tests passed successfully! ===");
    return 0;
}
