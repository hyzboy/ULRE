/**
 * RenderStrategyParity.h —— 渲染策略判定表的**对拍装置**（v2 §9.2 P2 / §9.4）
 *
 * A7a 起本装置的角色是**反向守卫**：决策已由 `RenderStrategyTable` 驱动，表**不再**是"只读
 * 不驱动"的旁证；旧的手写 if 链在此降为**参考实现**——每帧用同一批输入分别算表结论与旧链
 * 结论，不一致数必须为 0（不一致即 `GLogError`）。这样"表驱动 = 旧链结果"是可被实测证伪的
 * 命题：改动任一侧而另一侧不跟随，都会立刻报出来。
 *
 * 代价与边界：Release（NDEBUG）下全部编成空 inline（零成本）；Debug 下是几次比较 + 计数器。
 * 装置**不得空转**——两侧都必须真读各自的计算结果（调用侧只传结论，不传常量）。
 */
#pragma once

#include<hgl/ecs/support/RenderStrategyTable.h>

#ifdef NDEBUG
    #define ULRE_STRATEGY_PARITY_ENABLED 0
#else
    #define ULRE_STRATEGY_PARITY_ENABLED 1
#endif

namespace hgl
{
    namespace ecs
    {
#if ULRE_STRATEGY_PARITY_ENABLED

        /// 对拍统计（单线程渲染路径，不加锁）
        struct RenderStrategyParityStats
        {
            uint64_t collect_checks      = 0;
            uint64_t collect_mismatch    = 0;
            uint64_t caster_checks       = 0;
            uint64_t caster_mismatch     = 0;
            uint64_t receiver_checks     = 0;
            uint64_t receiver_mismatch   = 0;
            uint64_t fallback_checks     = 0;   ///< A7b：材质缺失 ⇒ 回退材质需求位
            uint64_t fallback_mismatch   = 0;
        };

        RenderStrategyParityStats &GetRenderStrategyParityStats();

        /// 对拍：当前 pass 的收集判定（table = 表结论，existing = 旧 if 链参考实现结论）
        void ParityCheckCollect(bool table_verdict,bool existing_verdict,const char *context);

        /// 对拍：阴影 caster 能力（表 ShadowCaster vs 参考实现 CanCastShadow）
        void ParityCheckShadowCaster(bool table_verdict,bool existing_verdict,const char *context);

        /// 对拍：阴影接收能力（表 ShadowReceiver vs 参考实现 CanReceiveShadow）
        void ParityCheckShadowReceiver(bool table_verdict,bool existing_verdict,const char *context);

        /// 对拍：**材质缺失 ⇒ 回退材质**（A7b）。
        /// 表侧 = `RenderNeed::FallbackMaterial` 需求位；参考实现 = 事实
        /// `!HasAnyMaterialSource(owner)` 直接求反 —— 两者必须逐帧同值。
        /// 这条守卫是 A7b 的**新语义**守卫：把“无材质来源”改回剔除条件、或把回退规则
        /// 写歪，都会立刻报不一致（不是恒真比较）。
        void ParityCheckFallbackMaterial(bool table_verdict,bool existing_verdict,const char *context);

        /// 打印汇总（出现首个不一致时自动打印；此外每 N 次检查打印一次）
        void LogRenderStrategyParityStats(bool force);

#else

        struct RenderStrategyParityStats { };

        inline RenderStrategyParityStats &GetRenderStrategyParityStats() { static RenderStrategyParityStats s; return s; }

        inline void ParityCheckCollect(bool,bool,const char *) {}
        inline void ParityCheckShadowCaster(bool,bool,const char *) {}
        inline void ParityCheckShadowReceiver(bool,bool,const char *) {}
        inline void ParityCheckFallbackMaterial(bool,bool,const char *) {}
        inline void LogRenderStrategyParityStats(bool) {}

#endif
    }//namespace ecs
}//namespace hgl
