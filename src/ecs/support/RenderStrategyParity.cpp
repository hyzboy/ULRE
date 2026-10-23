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
            uint32_t g_fallback_logged = 0;

            constexpr uint32_t kMaxDetailPerKind = 5;
            constexpr uint64_t kSummaryInterval = 300;      ///< 每 N 次检查打印一次汇总

            /// 是否已经出现过任何不一致。全 0 时汇总行没有任何诊断价值，直接不输出，
            /// 避免"一切正常"时每 N 次检查刷一条同样的日志。
            inline bool HasAnyMismatch()
            {
                return (g_stats.collect_mismatch + g_stats.caster_mismatch
                      + g_stats.receiver_mismatch + g_stats.fallback_mismatch) != 0;
            }

            /// 汇总节奏不再只挂在 collect 上：任一类检查跑到间隔整数倍都算一次采样点，
            /// 否则 caster/receiver/fallback 独立增长时会永远不汇总。
            inline bool ReachedSummaryInterval()
            {
                return (g_stats.collect_checks  > 0 && g_stats.collect_checks  % kSummaryInterval == 0)
                    || (g_stats.caster_checks    > 0 && g_stats.caster_checks    % kSummaryInterval == 0)
                    || (g_stats.receiver_checks  > 0 && g_stats.receiver_checks  % kSummaryInterval == 0)
                    || (g_stats.fallback_checks  > 0 && g_stats.fallback_checks  % kSummaryInterval == 0);
            }

            /// 首次由"无不一致"变"有不一致"时强制打印一次汇总（明细行最多 5 条，
            /// 光看明细拿不到分母，必须在那一刻给出完整快照）。
            inline void NoteMismatch()
            {
                if (HasAnyMismatch())
                {
                    const uint64_t total = g_stats.collect_mismatch + g_stats.caster_mismatch
                                         + g_stats.receiver_mismatch + g_stats.fallback_mismatch;

                    if (total == 1)
                        LogRenderStrategyParityStats(true);
                }
            }
        }

        RenderStrategyParityStats &GetRenderStrategyParityStats()
        {
            return g_stats;
        }

        void LogRenderStrategyParityStats(bool force)
        {
            if (!force)
            {
                // 一切正常时不输出：全 0 的汇总行纯属噪声
                if (!HasAnyMismatch())
                    return;

                if (!ReachedSummaryInterval())
                    return;
            }

            GLogInfo(u8"[策略判定对拍] collect=%llu/不一致=%llu  caster=%llu/不一致=%llu  receiver=%llu/不一致=%llu  fallback=%llu/不一致=%llu",
                     (unsigned long long)g_stats.collect_checks,
                     (unsigned long long)g_stats.collect_mismatch,
                     (unsigned long long)g_stats.caster_checks,
                     (unsigned long long)g_stats.caster_mismatch,
                     (unsigned long long)g_stats.receiver_checks,
                     (unsigned long long)g_stats.receiver_mismatch,
                     (unsigned long long)g_stats.fallback_checks,
                     (unsigned long long)g_stats.fallback_mismatch);
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

                    GLogError(u8"[策略判定对拍] 收集判定不一致：表=%d 旧链=%d（%s）",
                              table_verdict ? 1 : 0,
                              existing_verdict ? 1 : 0,
                              context ? context : "?");
                }

                NoteMismatch();
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

                    GLogError(u8"[策略判定对拍] 阴影 caster 判定不一致：表=%d 旧链=%d（%s）",
                              table_verdict ? 1 : 0,
                              existing_verdict ? 1 : 0,
                              context ? context : "?");
                }

                NoteMismatch();
            }
        }

        void ParityCheckFallbackMaterial(bool table_verdict,bool existing_verdict,const char *context)
        {
            ++g_stats.fallback_checks;

            if (table_verdict != existing_verdict)
            {
                ++g_stats.fallback_mismatch;

                if (g_fallback_logged < kMaxDetailPerKind)
                {
                    ++g_fallback_logged;

                    GLogError(u8"[策略判定对拍] 回退材质判定不一致：表=%d 参考=%d（%s）",
                              table_verdict ? 1 : 0,
                              existing_verdict ? 1 : 0,
                              context ? context : "?");
                }

                NoteMismatch();
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

                    GLogError(u8"[策略判定对拍] 阴影接收判定不一致：表=%d 旧链=%d（%s）",
                              table_verdict ? 1 : 0,
                              existing_verdict ? 1 : 0,
                              context ? context : "?");
                }

                NoteMismatch();
            }
        }
    }//namespace ecs
}//namespace hgl

#endif  // ULRE_STRATEGY_PARITY_ENABLED
