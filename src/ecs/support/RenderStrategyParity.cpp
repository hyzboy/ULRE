#include<hgl/ecs/support/RenderStrategyParity.h>

#if ULRE_STRATEGY_PARITY_ENABLED

#include<hgl/log/Log.h>

namespace hgl
{
    namespace ecs
    {
        namespace
        {
            RenderStrategyParityStats g_stats;

            /// 每种不一致只详细打印前若干次，避免刷屏（之后靠汇总计数）
            uint32_t g_collect_logged = 0;
            uint32_t g_caster_logged = 0;
            uint32_t g_receiver_logged = 0;

            constexpr uint32_t kMaxDetailPerKind = 5;
            constexpr uint64_t kSummaryInterval = 300;      ///< 每 N 次 collect 检查打印一次汇总
        }

        RenderStrategyParityStats &GetRenderStrategyParityStats()
        {
            return g_stats;
        }

        void LogRenderStrategyParityStats(bool force)
        {
            if (!force && g_stats.collect_checks % kSummaryInterval != 0)
                return;

            GLogInfo(u8"[策略判定对拍] collect=%llu/不一致=%llu  caster=%llu/不一致=%llu  receiver=%llu/不一致=%llu",
                     (unsigned long long)g_stats.collect_checks,
                     (unsigned long long)g_stats.collect_mismatch,
                     (unsigned long long)g_stats.caster_checks,
                     (unsigned long long)g_stats.caster_mismatch,
                     (unsigned long long)g_stats.receiver_checks,
                     (unsigned long long)g_stats.receiver_mismatch);
        }

        void ParityCheckCollect(bool table_verdict,bool existing_verdict,const char *context)
        {
            ++g_stats.collect_checks;

            if (table_verdict != existing_verdict)
            {
                ++g_stats.collect_mismatch;

                if (g_collect_logged < kMaxDetailPerKind)
                {
                    ++g_collect_logged;

                    GLogError(u8"[策略判定对拍] 收集判定不一致：表=%d 现有=%d（%s）",
                              table_verdict ? 1 : 0,
                              existing_verdict ? 1 : 0,
                              context ? context : "?");
                }
            }

            LogRenderStrategyParityStats(false);
        }

        void ParityCheckShadowCaster(bool table_verdict,bool existing_verdict,const char *context)
        {
            ++g_stats.caster_checks;

            if (table_verdict != existing_verdict)
            {
                ++g_stats.caster_mismatch;

                if (g_caster_logged < kMaxDetailPerKind)
                {
                    ++g_caster_logged;

                    GLogError(u8"[策略判定对拍] 阴影 caster 判定不一致：表=%d 现有=%d（%s）",
                              table_verdict ? 1 : 0,
                              existing_verdict ? 1 : 0,
                              context ? context : "?");
                }
            }
        }

        void ParityCheckShadowReceiver(bool table_verdict,bool existing_verdict,const char *context)
        {
            ++g_stats.receiver_checks;

            if (table_verdict != existing_verdict)
            {
                ++g_stats.receiver_mismatch;

                if (g_receiver_logged < kMaxDetailPerKind)
                {
                    ++g_receiver_logged;

                    GLogError(u8"[策略判定对拍] 阴影接收判定不一致：表=%d 现有=%d（%s）",
                              table_verdict ? 1 : 0,
                              existing_verdict ? 1 : 0,
                              context ? context : "?");
                }
            }
        }
    }//namespace ecs
}//namespace hgl

#endif  // ULRE_STRATEGY_PARITY_ENABLED
