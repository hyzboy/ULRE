/**
 * RenderStrategyTable.h —— **渲染策略判定表**（v2 §9.2 P2）
 *
 * 用途：把"这个实体对渲染提出了什么要求"从**散落的 if 链**收敛成**一张声明式表**：
 *       输入 = 组件集合（A0 的位掩码）+ 组件/世界提供的事实；输出 = 需求位。
 *
 * 设计要点：
 *   · **唯一判据**（A7a 起）：收集 / 剔除 / 阴影投射 / 接收全部由**表查结果**决定，驱动侧不再
 *     有任何手写判据。语义 predicate（可渲染 / 有材质来源 / 可投射 / 可接收 / 实体级可见性）
 *     是表的**输入**而不是判据本身 —— 表不重新判定它们，因此不存在第二套真值。
 *   · 旧的手写 if 链降为 `RenderStrategyParity.h` 的**参考实现**（反向守卫）：Debug 下每帧
 *     同输入与表对拍、不一致即 `GLogError`；Release（NDEBUG）整块编空。"表驱动 = 旧链结果"
 *     因此是可被实测证伪的命题，而不是声明。
 *   · 表是**数据驱动**的：新增需求只加一行 `kRenderNeedRules`；新增事实只加一个位。
 *     规则引用未知事实位、或引用未知槽位，都会在下面的 static_assert 里被抓住（自动 scale）。
 *   · 表说的不是"用哪个具体 PassType"——那由材质 recipe / 变体键决定（见 v2 §9.3）。
 *
 * 语义镜像（A1 定下、A7a 保持；按语义命名而非行号引用，行号会漂）：
 *   · `CollectForCurrentPass` ⇔ `RenderPrimitiveCollectSystem.cpp` 收集循环的
 *     实体可见 / 可渲染 / owner（+ 阴影 pass 下的 `CanCastShadow` 与距离裁剪，
 *     作为 `ShadowPass` 触发时的条件要求）。**A7b：材质来源不是收集判据** ——
 *     「有几何但无材质来源」是**材质错误**，由 `FallbackMaterial` 需求位表达
 *     （走保底材质渲染出来 + 根颜色标注错误种类），不是剔除条件。
 *   · `FallbackMaterial` ⇔「有几何、无材质来源 ⇒ 取保底（错误）材质」
 *   · `ShadowCaster`   ⇔ 同一处的 `CanCastShadow`（能力位）
 *   · `ShadowReceiver` ⇔ `PrimitiveBatchPipeline.cpp` 的 `CanReceiveShadow`（能力位）
 */
#pragma once

#include<cstdint>
#include<hgl/ecs/support/ComponentTypeTable.h>

namespace hgl
{
    namespace ecs
    {
        /// 渲染需求位：**"提出了什么要求"**，不是"用哪个 PassType"
        enum class RenderNeed : uint32_t
        {
            None     = 0,

            /// 进**当前 pass** 的收集（主视图或阴影视图共用同一段收集代码）
            CollectForCurrentPass = 1u << 0,

            /// 阴影 pass 里可作为 caster 参与（能力位）
            ShadowCaster = 1u << 1,

            /// 接收阴影（材质侧标志；不是剔除条件）
            ShadowReceiver = 1u << 2,

            /// 可进 EarlyZ 预填（预留：仓里已有 `PassType::EarlyZSolid/Masked`）
            EarlyZ = 1u << 3,

            /// 需要材质运行期数据（由组件集合蕴含推出，见 implies）
            NeedsMaterialRuntime = 1u << 4,

            /// **材质错误需走回退（保底）材质**：有几何而**无**材质来源时置位。
            /// A7b：「无材质来源」不是剔除条件，而是必须用保底材质（+ 根颜色标注错误
            /// 种类）渲染出来的材质错误 —— 该需求位就是这条语义的**唯一判据**。
            FallbackMaterial = 1u << 5,
        };

        inline constexpr uint32_t RENDER_NEED_COUNT = 6;

        // ─────────────────────────────────────────────────────────────
        // 事实（求值输入）
        //   · 语义事实来自组件/世界；本表**不重新判定**它们（例如"可渲染"由组件自己回答），
        //     只把它们与组件集合组合成需求 —— 这样 stage A 才不会出现第二套真值。
        // ─────────────────────────────────────────────────────────────

        enum class StrategyFact : uint32_t
        {
            None = 0,

            /// 实体级可见（A5a：**唯一可见性真值** —— 组件级 `visible` 已删，
            /// 读法 = `ECSContext::IsEntityVisible`，含祖先继承，已算完）
            EntityVisible     = 1u << 0,
            HasOwner          = 1u << 1,   ///< 有 owner 实体
            Renderable        = 1u << 2,   ///< 具备可渲染资源（现 = `GeometryData::GetPrimitiveAsset() != nullptr`）
            /// 有材质来源（现 = `HasAnyMaterialSource`：数据层配方覆盖 或 asset 默认配方）。
            /// A7b：**不再是收集判据**，只作 `FallbackMaterial` 的输入 / 诊断上报。
            HasMaterialSource = 1u << 3,
            CastShadow        = 1u << 4,   ///< 允许投射（现 = `CanCastShadow`，含缺省约定）
            ReceiveShadow     = 1u << 5,   ///< 允许接收（现 = `CanReceiveShadow`，含缺省约定）
            ShadowPass        = 1u << 6,   ///< 当前 pass 是阴影 pass（世界态）
            InShadowRange     = 1u << 7,   ///< 在阴影距离裁剪内（世界态）

            Count             = 8,
        };

        inline constexpr uint32_t STRATEGY_FACT_COUNT = 8;

        /// 事实集合（调用侧填；`ToMask()` 后本表只做位运算）
        struct StrategyFacts
        {
            bool entity_visible      = true;
            bool has_owner           = true;
            bool renderable          = true;
            bool has_material_source = true;
            bool cast_shadow         = true;
            bool receive_shadow      = true;
            bool shadow_pass         = false;
            bool in_shadow_range     = true;

            constexpr uint32_t ToMask() const
            {
                uint32_t mask = 0;

                if (entity_visible)      mask |= static_cast<uint32_t>(StrategyFact::EntityVisible);
                if (has_owner)           mask |= static_cast<uint32_t>(StrategyFact::HasOwner);
                if (renderable)          mask |= static_cast<uint32_t>(StrategyFact::Renderable);
                if (has_material_source) mask |= static_cast<uint32_t>(StrategyFact::HasMaterialSource);
                if (cast_shadow)         mask |= static_cast<uint32_t>(StrategyFact::CastShadow);
                if (receive_shadow)      mask |= static_cast<uint32_t>(StrategyFact::ReceiveShadow);
                if (shadow_pass)         mask |= static_cast<uint32_t>(StrategyFact::ShadowPass);
                if (in_shadow_range)     mask |= static_cast<uint32_t>(StrategyFact::InShadowRange);

                return mask;
            }
        };

        // ─────────────────────────────────────────────────────────────
        // 规则表（数据驱动；新增需求只加一行）
        // ─────────────────────────────────────────────────────────────

        struct RenderNeedRule
        {
            RenderNeed need;
            uint32_t   required_slots;          ///< 必须具有的组件槽位（A0 掩码）
            uint32_t   required_facts;          ///< 必须为真的事实位
            uint32_t   forbidden_facts;         ///< 必须为假的事实位
            uint32_t   when_facts;              ///< 条件触发位（0 = 无条件）
            uint32_t   then_required_facts;     ///< 触发时额外必须为真的事实位
        };

        inline constexpr uint32_t kRenderNeedRules_Count = RENDER_NEED_COUNT;

        inline constexpr RenderNeedRule kRenderNeedRules[kRenderNeedRules_Count] =
        {
            // 进当前 pass 的收集：可见（**实体级唯一真值**）、有 owner、有可渲染资源；
            // **若当前是阴影 pass**，再要求允许投射且在距离裁剪内
            // （逐条镜像 `RenderPrimitiveCollectSystem.cpp` 的两个收集循环）
            //
            // A7b：**材质来源不再是收集判据**（用户定稿）——「有几何但无材质来源」是
            // 材质错误而非剔除条件，由紧随其后的 FallbackMaterial 规则表达。
            { RenderNeed::CollectForCurrentPass,
              ComponentTypeBit(ComponentType::Geometry),
              static_cast<uint32_t>(StrategyFact::EntityVisible)
            | static_cast<uint32_t>(StrategyFact::HasOwner)
            | static_cast<uint32_t>(StrategyFact::Renderable),
              0,
              static_cast<uint32_t>(StrategyFact::ShadowPass),
              static_cast<uint32_t>(StrategyFact::CastShadow)
            | static_cast<uint32_t>(StrategyFact::InShadowRange) },

            // 「材质缺失 ⇒ 取保底（错误）材质」：用 **forbidden_facts 表达“缺失”**，
            // 于是它仍是表可查的需求位而不是特例 if。种类与根颜色由 mtl 侧
            // `kMaterialErrorClassifyRules` / `kFallbackMaterialRules` 给出（数据驱动）。
            { RenderNeed::FallbackMaterial,
              ComponentTypeBit(ComponentType::Geometry),
              0,
              static_cast<uint32_t>(StrategyFact::HasMaterialSource),
              0, 0 },

            // 阴影 caster 是**能力位**：只要允许投射即可（是否真的进当前 pass 由上面那条决定）
            { RenderNeed::ShadowCaster,
              ComponentTypeBit(ComponentType::Geometry),
              static_cast<uint32_t>(StrategyFact::CastShadow),
              0, 0, 0 },

            // 接收阴影也是能力位
            { RenderNeed::ShadowReceiver,
              ComponentTypeBit(ComponentType::Geometry),
              static_cast<uint32_t>(StrategyFact::ReceiveShadow),
              0, 0, 0 },

            // EarlyZ：有几何即可（预留，暂不驱动任何路径）
            { RenderNeed::EarlyZ,
              ComponentTypeBit(ComponentType::Geometry),
              0, 0, 0, 0 },

            // 需要材质运行期行：由组件集合推出（A2–A5 落地后由 slot 自然决定）
            { RenderNeed::NeedsMaterialRuntime,
              ComponentTypeBit(ComponentType::MaterialRuntime),
              0, 0, 0, 0 },
        };

        // ─────────────────────────────────────────────────────────────
        // 求值
        // ─────────────────────────────────────────────────────────────

        /// 组件集合 + 事实 ⇒ 需求位掩码
        /// 先按 implies 展开组件集合（如 MaterialRuntime ⇒ MaterialData），再逐条套规则。
        constexpr uint32_t EvaluateRenderNeed(uint32_t component_mask, uint32_t fact_mask)
        {
            // implies 展开：把掩码里每个槽位的蕴含并进来
            uint32_t slots = component_mask;

            for (uint32_t round = 0; round <= COMPONENT_TYPE_COUNT; ++round)
            {
                const uint32_t before = slots;

                for (uint32_t i = 0; i < COMPONENT_TYPE_COUNT; ++i)
                    if (slots & (1u << i))
                        slots |= kComponentTypeImplies[i];

                if (slots == before)
                    break;
            }

            uint32_t needs = 0;

            for (const RenderNeedRule &rule : kRenderNeedRules)
            {
                if ((slots & rule.required_slots) != rule.required_slots)
                    continue;

                if ((fact_mask & rule.required_facts) != rule.required_facts)
                    continue;

                if ((fact_mask & rule.forbidden_facts) != 0)
                    continue;

                // 条件要求：触发位全满足时，额外要求也必须在
                if (rule.when_facts != 0 && (fact_mask & rule.when_facts) == rule.when_facts)
                    if ((fact_mask & rule.then_required_facts) != rule.then_required_facts)
                        continue;

                needs |= static_cast<uint32_t>(rule.need);
            }

            return needs;
        }

        inline constexpr bool HasRenderNeed(const uint32_t needs, const RenderNeed need)
        {
            return (needs & static_cast<uint32_t>(need)) != 0;
        }

        // ─────────────────────────────────────────────────────────────
        // 自检（自动 scale：新增需求/事实后这里自动纳入检查）
        //   ① 每个需求恰好一条规则，且不重复
        //   ② 规则只引用已知的事实位
        //   ③ 规则引用的槽位必须都在类型表里
        //   ④ "必须为真"与"必须为假"不得相交
        // ─────────────────────────────────────────────────────────────

        namespace detail
        {
            constexpr bool CheckRenderNeedRules()
            {
                constexpr uint32_t kKnownFacts = (1u << STRATEGY_FACT_COUNT) - 1u;
                constexpr uint32_t kKnownSlots = (1u << COMPONENT_TYPE_COUNT) - 1u;

                // ① 覆盖性：每条规则的需求互不相同，且都落在 RENDER_NEED_COUNT 位以内
                uint32_t seen = 0;

                for (const RenderNeedRule &rule : kRenderNeedRules)
                {
                    const uint32_t bit = static_cast<uint32_t>(rule.need);

                    if (bit == 0 || (bit & (bit - 1)) != 0)     // 必须是单个位
                        return false;

                    if (seen & bit)                             // 重复
                        return false;

                    seen |= bit;

                    if ((rule.required_facts & ~kKnownFacts) != 0)      // ②
                        return false;

                    if ((rule.forbidden_facts & ~kKnownFacts) != 0)     // ②
                        return false;

                    if ((rule.required_facts & rule.forbidden_facts) != 0)  // ④
                        return false;

                    if ((rule.when_facts & ~kKnownFacts) != 0)          // ② 条件位也必须是已知事实
                        return false;

                    if ((rule.then_required_facts & ~kKnownFacts) != 0) // ②
                        return false;

                    if ((rule.then_required_facts & rule.forbidden_facts) != 0) // ④ 条件要求不得与禁止项冲突
                        return false;

                    if ((rule.required_slots & ~kKnownSlots) != 0)      // ③
                        return false;
                }

                return true;
            }
        }

        static_assert(detail::CheckRenderNeedRules(),
                      "渲染策略规则表不自洽：需求位重复/非单位位、引用了未知事实位或未知槽位、或同一事实既要求真又要求假");

        /// 所有需求位都被规则覆盖（新增 RenderNeed 却忘加规则 ⇒ 编译失败；自动 scale：遍历 0..RENDER_NEED_COUNT-1）
        namespace detail
        {
            constexpr bool CheckRenderNeedCoverage()
            {
                uint32_t covered = 0;

                for (const RenderNeedRule &rule : kRenderNeedRules)
                    covered |= static_cast<uint32_t>(rule.need);

                for (uint32_t i = 0; i < RENDER_NEED_COUNT; ++i)
                    if ((covered & (1u << i)) == 0)
                        return false;

                return true;
            }
        }

        static_assert(detail::CheckRenderNeedCoverage(),
                      "有 RenderNeed 没有对应的规则（新增需求时请在 kRenderNeedRules 同步加一行）");
    }//namespace ecs
}//namespace hgl
