/**
 * RenderStrategyParity.h —— 渲染策略判定表的**对拍装置**（v2 §9.2 P2 / §9.4）
 *
 * stage A 的第一步是"表只读不驱动"：表先不算数，只是**每帧拿表结论与现有 if 链结论对拍**，
 * 不一致数必须为 0。这样把"策略收敛"这件事变成可观测、可回归的证据，而不是一次盲改。
 *
 * 成本：Release（NDEBUG）下全部编成空 inline（零成本）；Debug 下是几次比较 + 计数器。
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
        };

        RenderStrategyParityStats &GetRenderStrategyParityStats();

        /// 对拍：当前 pass 的收集判定（table = 表结论，existing = 现有 if 链结论）
        void ParityCheckCollect(bool table_verdict,bool existing_verdict,const char *context);

        /// 对拍：阴影 caster 能力（表 ShadowCaster vs CanCastShadow）
        void ParityCheckShadowCaster(bool table_verdict,bool existing_verdict,const char *context);

        /// 对拍：阴影接收能力（表 ShadowReceiver vs CanReceiveShadow）
        void ParityCheckShadowReceiver(bool table_verdict,bool existing_verdict,const char *context);

        /// 打印汇总（出现首个不一致时自动打印；此外每 N 次检查打印一次）
        void LogRenderStrategyParityStats(bool force);

#else

        struct RenderStrategyParityStats { };

        inline RenderStrategyParityStats &GetRenderStrategyParityStats() { static RenderStrategyParityStats s; return s; }

        inline void ParityCheckCollect(bool,bool,const char *) {}
        inline void ParityCheckShadowCaster(bool,bool,const char *) {}
        inline void ParityCheckShadowReceiver(bool,bool,const char *) {}
        inline void LogRenderStrategyParityStats(bool) {}

#endif
    }//namespace ecs
}//namespace hgl
