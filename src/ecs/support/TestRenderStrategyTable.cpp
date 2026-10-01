#include <hgl/ecs/support/RenderStrategyTable.h>
#include <hgl/ecs/support/RenderStrategyParity.h>
#include <hgl/mtl/MaterialRecipe.h>
#include <hgl/mtl/MaterialDefinitionRegistry.h>
#include <hgl/log/Log.h>
#include <hgl/log/Logger.h>
#include <cstring>

using namespace hgl;
using namespace hgl::ecs;
using namespace hgl::graph::mtl;

namespace
{
    constexpr uint32_t kGeoSlot      = ComponentTypeBit(ComponentType::Geometry);
    constexpr uint32_t kMatRtSlot    = ComponentTypeBit(ComponentType::MaterialRuntime);
    constexpr uint32_t kMatDataSlot  = ComponentTypeBit(ComponentType::MaterialData);

    /// 全部事实为正（主视图）：随后按需翻某个事实来构造用例
    StrategyFacts AllGoodFacts()
    {
        StrategyFacts f;
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
 *   · 逐条镜像收集 / 阴影能力判据（`RenderPrimitiveCollectSystem.cpp` 的两个收集循环、
 *     `PrimitiveBatchPipeline.cpp` 的接收侧）——A7a 起这些判据由本表驱动，旧链降为
 *     对拍参考实现（见 `RenderStrategyParity.h` 的反向守卫）；
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
    // Test 2: 阴影 pass 的条件要求（镜像收集循环的阴影分支：CanCastShadow + 距离裁剪）
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
    // Test 3: 可见性（实体级唯一真值）/资源/owner 事实逐条生效
    //         + A7b：**无材质来源不再是剔除条件**（改为回退材质需求位）
    // ─────────────────────────────────────────────────────────────
    {
        const char *what = nullptr;

        StrategyFacts f2 = AllGoodFacts(); f2.entity_visible = false;
        if (Collect(kGeoSlot,f2)) { what = "实体级不可见（含祖先继承）"; }

        StrategyFacts f3 = AllGoodFacts(); f3.renderable = false;
        if (!what && Collect(kGeoSlot,f3)) { what = "无渲染资源"; }

        StrategyFacts f5 = AllGoodFacts(); f5.has_owner = false;
        if (!what && Collect(kGeoSlot,f5)) { what = "无 owner"; }

        if (what)
        {
            GLogError(u8"Test 3 Failed: %s 时仍被判为进收集", what);
            return 12;
        }

        // ── A7b 新语义：有几何但**无材质来源** ────────────────────────────────
        // 用户定稿：它不是剔除条件，而是**材质错误** ⇒ 照常进收集 + 取回退（错误）材质。
        StrategyFacts no_source = AllGoodFacts();
        no_source.has_material_source = false;
        const uint32_t no_source_needs = EvaluateRenderNeed(kGeoSlot,no_source.ToMask());

        if (!HasRenderNeed(no_source_needs,RenderNeed::CollectForCurrentPass))
        {
            GLogError(u8"Test 3 Failed: 无材质来源被当成剔除条件（新语义：必须走回退材质渲染出来）");
            return 12;
        }

        if (!HasRenderNeed(no_source_needs,RenderNeed::FallbackMaterial))
        {
            GLogError(u8"Test 3 Failed: 无材质来源却没有置回退材质需求位（错误种类/根颜色无处落）");
            return 12;
        }

        // 反例：有材质来源 ⇒ 不得置回退位（否则正常材质也会被标成错误色）
        if (HasRenderNeed(EvaluateRenderNeed(kGeoSlot,AllGoodFacts().ToMask()),
                          RenderNeed::FallbackMaterial))
        {
            GLogError(u8"Test 3 Failed: 有材质来源也置了回退材质需求位");
            return 12;
        }

        // 反例：没有几何槽位 ⇒ 不得置回退位（回退材质是给几何图元的）
        if (HasRenderNeed(EvaluateRenderNeed(0u,no_source.ToMask()),
                          RenderNeed::FallbackMaterial))
        {
            GLogError(u8"Test 3 Failed: 没有几何槽位也置了回退材质需求位");
            return 12;
        }

        // 无几何槽位 ⇒ 不进任何 pass（规则都要求 Geometry 槽位）
        if (Collect(0u,AllGoodFacts()))
        {
            GLogError(u8"Test 3 Failed: 没有几何槽位却进了收集");
            return 12;
        }

        GLogInfo(u8"Test 3 Passed: 可见性/资源/owner 逐条生效；无材质来源改走回退材质（仍进收集）；无槽位即无需求。");
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

        // A7b：回退材质对拍装置同样必须“能报不一致、不误报一致”——否则
        // “回退判定不一致 0 行”这条实测证据毫无意义。
        const uint64_t fallback_before = GetRenderStrategyParityStats().fallback_mismatch;

        ParityCheckFallbackMaterial(true,false,"TestRenderStrategyTable#selftest");
        if (GetRenderStrategyParityStats().fallback_mismatch != fallback_before + 1)
        {
            GLogError(u8"Test 5 Failed: 回退材质对拍装置没有报出故意制造的不一致");
            return 14;
        }

        ParityCheckFallbackMaterial(false,false,"TestRenderStrategyTable#selftest");
        ParityCheckFallbackMaterial(true,true,"TestRenderStrategyTable#selftest");
        if (GetRenderStrategyParityStats().fallback_mismatch != fallback_before + 1)
        {
            GLogError(u8"Test 5 Failed: 回退材质对拍装置把相等的判定误报为不一致");
            return 14;
        }

        GLogInfo(u8"Test 5 Passed: 对拍装置能报不一致、不误报一致（collect 自检计数 %llu → %llu，"
                 u8"fallback 不一致计数 %llu）。",
                 (unsigned long long)before,
                 (unsigned long long)GetRenderStrategyParityStats().collect_mismatch,
                 (unsigned long long)GetRenderStrategyParityStats().fallback_mismatch);
    }

    // ─────────────────────────────────────────────────────────────
    // Test 6: A7b 回退（错误）材质规则表
    //   · 定义 ID 与根颜色都来自 kFallbackMaterialRules（数据驱动、有界枚举）；
    //   · 每个错误种类的根颜色**两两不同** —— 否则“按颜色标注错误种类”不成立；
    //   · 分类表（事实 ⇒ 种类）与规则表一致：无材质来源 ⇒ MissingMaterialSource，
    //     有材质来源 ⇒ None（非材质错误，不得被标成错误色）。
    // ─────────────────────────────────────────────────────────────
    {
        // 无参回退 = 非错误回退，仍是既有保底材质（与门 F.fallback-dimension-neutral 同一判据）
        if (std::strcmp(GetFallbackMaterialDefinitionID(),"builtin/pure_color") != 0)
        {
            GLogError(u8"Test 6 Failed: 无参回退材质定义 ID 不再是 builtin/pure_color");
            return 15;
        }

        for (uint32_t i = 0; i < MATERIAL_ERROR_KIND_COUNT; ++i)
        {
            const MaterialErrorKind kind = static_cast<MaterialErrorKind>(i);
            const FallbackMaterialRule &rule = GetFallbackMaterialRule(kind);

            if (static_cast<uint32_t>(rule.kind) != i)
            {
                GLogError(u8"Test 6 Failed: 规则表第 %u 行与枚举不同序", i);
                return 15;
            }

            if (!rule.definition_id || rule.definition_id[0] == '\0')
            {
                GLogError(u8"Test 6 Failed: 种类 %u 没有回退材质定义 ID", i);
                return 15;
            }

            if (!rule.marker_name || rule.marker_name[0] == '\0')
            {
                GLogError(u8"Test 6 Failed: 种类 %u 没有稳定名", i);
                return 15;
            }

            if (std::strcmp(GetFallbackMaterialDefinitionID(kind),rule.definition_id) != 0)
            {
                GLogError(u8"Test 6 Failed: 种类 %u 的两个取用口给出的定义 ID 不一致", i);
                return 15;
            }

            for (uint32_t c = 0; c < 4; ++c)
            {
                if (!(rule.marker_color[c] >= 0.0f && rule.marker_color[c] <= 1.0f))
                {
                    GLogError(u8"Test 6 Failed: 种类 %u 的根颜色通道 %u 越界", i, c);
                    return 15;
                }
            }
        }

        // 错误种类（非 None）的根颜色两两不同
        for (uint32_t i = 1; i < MATERIAL_ERROR_KIND_COUNT; ++i)
        {
            for (uint32_t j = i + 1; j < MATERIAL_ERROR_KIND_COUNT; ++j)
            {
                const FallbackMaterialRule &a = GetFallbackMaterialRule(
                    static_cast<MaterialErrorKind>(i));
                const FallbackMaterialRule &b = GetFallbackMaterialRule(
                    static_cast<MaterialErrorKind>(j));

                const bool same =
                    a.marker_color[0] == b.marker_color[0]
                 && a.marker_color[1] == b.marker_color[1]
                 && a.marker_color[2] == b.marker_color[2]
                 && a.marker_color[3] == b.marker_color[3];

                if (same)
                {
                    GLogError(u8"Test 6 Failed: 错误种类 %u 与 %u 的根颜色相同（无法按颜色区分种类）",
                              i, j);
                    return 15;
                }
            }
        }

        // 分类表（数据驱动）：无材质来源 ⇒ MissingMaterialSource；有来源 ⇒ None
        if (ClassifyMaterialErrorKind(false) != MaterialErrorKind::MissingMaterialSource)
        {
            GLogError(u8"Test 6 Failed: 无材质来源没有被分类为 MissingMaterialSource");
            return 15;
        }

        if (ClassifyMaterialErrorKind(true) != MaterialErrorKind::None)
        {
            GLogError(u8"Test 6 Failed: 有材质来源被误分类为材质错误");
            return 15;
        }

        // 分类表落在规则表的定义域内（分类结果必须有规则可查）
        const FallbackMaterialRule &rule =
            GetFallbackMaterialRule(MaterialErrorKind::MissingMaterialSource);
        const FallbackMaterialRule &none_rule =
            GetFallbackMaterialRule(MaterialErrorKind::None);

        if (std::strcmp(rule.marker_name,"missing_material_source") != 0
         || std::strcmp(none_rule.definition_id,"builtin/pure_color") != 0)
        {
            GLogError(u8"Test 6 Failed: 规则表稳定名/非错误回退定义 ID 与契约不符");
            return 15;
        }

        GLogInfo(u8"Test 6 Passed: 回退材质规则表 %u 行（定义 ID + 根颜色两两不同 + 稳定名），"
                 u8"分类表：无来源 ⇒ MissingMaterialSource、有来源 ⇒ None。",
                 static_cast<uint32_t>(MATERIAL_ERROR_KIND_COUNT));
    }

    GLogInfo(u8"=== All RenderStrategyTable tests passed successfully! ===");
    return 0;
}
