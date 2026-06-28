#pragma once

#include <cstdint>

namespace hgl::ecs
{
    struct MaterialResolveR21DryRunStats
    {
        uint64_t tasks_seen = 0;
        uint64_t candidate_program_hits = 0;
        uint64_t candidate_payload_hits = 0;
        uint64_t candidate_binding_hits = 0;

        uint64_t miss_program = 0;
        uint64_t miss_payload = 0;
        uint64_t miss_binding = 0;

        uint64_t program_match_with_legacy = 0;
        uint64_t payload_match_with_legacy = 0;
        uint64_t binding_match_with_legacy = 0;

        uint64_t short_circuit_checks = 0;
        uint64_t dry_run_short_circuit_eligible = 0;
        uint64_t would_short_circuit_execute = 0;
        uint64_t short_circuit_executed = 0;
        uint64_t short_circuit_apply_failures = 0;
        uint64_t short_circuit_auto_disable_events = 0;
        uint64_t short_circuit_auto_reenable_events = 0;
        uint64_t short_circuit_blocked_by_program = 0;
        uint64_t short_circuit_blocked_by_payload = 0;
        uint64_t short_circuit_blocked_by_binding = 0;

        uint64_t short_circuit_blocked_by_guard_dirty = 0;
        uint64_t short_circuit_blocked_by_guard_domain = 0;
        uint64_t short_circuit_blocked_by_guard_vil = 0;
        uint64_t short_circuit_blocked_by_guard_primitive = 0;
    };

    struct MaterialResolveR2CStats
    {
        uint64_t short_circuit_executed_total = 0;
        uint64_t short_circuit_apply_failures_total = 0;
        uint64_t short_circuit_auto_disable_events_total = 0;
        uint64_t short_circuit_auto_reenable_events_total = 0;
        uint64_t short_circuit_manual_disable_observed_total = 0;
        uint64_t short_circuit_warning_events_total = 0;
        uint32_t consecutive_short_circuit_failures = 0;
        bool execute_short_circuit_enabled = false;
        bool short_circuit_disabled_by_auto_guardrail = false;
    };

    class MaterialResolveDiagnostics
    {
    private:

        // Feature/control switches
        bool decoupled_cache_enabled = false;
        bool decoupled_cache_dryrun_short_circuit_check_enabled = true;
        bool decoupled_cache_dryrun_whitelist_enabled = true;
        bool decoupled_cache_execute_short_circuit_enabled = false;
        bool decoupled_cache_execute_short_circuit_auto_disable_on_failures = true;
        uint32_t decoupled_cache_execute_short_circuit_failure_threshold = 32;
        bool decoupled_cache_execute_short_circuit_auto_reenable_after_cooldown = true;
        uint64_t decoupled_cache_execute_short_circuit_cooldown_ms = 1000;

        bool short_circuit_warning_enabled = true;
        uint32_t short_circuit_warning_failure_rate_percent_threshold = 50;
        uint32_t short_circuit_warning_min_samples = 8;
        uint64_t short_circuit_warning_cooldown_ms = 3000;

        // Runtime state
        uint32_t consecutive_short_circuit_failures = 0;
        uint64_t short_circuit_last_auto_disable_ms = 0;
        bool short_circuit_disabled_by_auto_guardrail = false;
        bool short_circuit_manual_disable_logged = false;
        uint64_t last_short_circuit_warning_ms = 0;

        uint64_t last_cache_stats_log_ms = 0;
        uint64_t cache_stats_log_interval_ms = 1000;

        MaterialResolveR21DryRunStats frame_stats{};
        MaterialResolveR2CStats cumulative_stats{};

    public:

        void SetDecoupledCacheEnabled(bool enabled) { decoupled_cache_enabled = enabled; }
        bool IsDecoupledCacheEnabled() const { return decoupled_cache_enabled; }

        void SetDecoupledCacheDryRunShortCircuitCheckEnabled(bool enabled) { decoupled_cache_dryrun_short_circuit_check_enabled = enabled; }
        bool IsDecoupledCacheDryRunShortCircuitCheckEnabled() const { return decoupled_cache_dryrun_short_circuit_check_enabled; }

        void SetDecoupledCacheDryRunWhitelistEnabled(bool enabled) { decoupled_cache_dryrun_whitelist_enabled = enabled; }
        bool IsDecoupledCacheDryRunWhitelistEnabled() const { return decoupled_cache_dryrun_whitelist_enabled; }

        void SetDecoupledCacheExecuteShortCircuitEnabled(bool enabled)
        {
            decoupled_cache_execute_short_circuit_enabled = enabled;
            consecutive_short_circuit_failures = 0;
            short_circuit_disabled_by_auto_guardrail = false;
            short_circuit_manual_disable_logged = false;
        }
        bool IsDecoupledCacheExecuteShortCircuitEnabled() const { return decoupled_cache_execute_short_circuit_enabled; }

        void SetDecoupledCacheExecuteShortCircuitAutoDisableOnFailuresEnabled(bool enabled)
        {
            decoupled_cache_execute_short_circuit_auto_disable_on_failures = enabled;
        }
        bool IsDecoupledCacheExecuteShortCircuitAutoDisableOnFailuresEnabled() const
        {
            return decoupled_cache_execute_short_circuit_auto_disable_on_failures;
        }

        void SetDecoupledCacheExecuteShortCircuitFailureThreshold(uint32_t threshold)
        {
            decoupled_cache_execute_short_circuit_failure_threshold = threshold > 0 ? threshold : 1;
        }
        uint32_t GetDecoupledCacheExecuteShortCircuitFailureThreshold() const
        {
            return decoupled_cache_execute_short_circuit_failure_threshold;
        }

        void SetDecoupledCacheExecuteShortCircuitAutoReenableAfterCooldownEnabled(bool enabled)
        {
            decoupled_cache_execute_short_circuit_auto_reenable_after_cooldown = enabled;
        }
        bool IsDecoupledCacheExecuteShortCircuitAutoReenableAfterCooldownEnabled() const
        {
            return decoupled_cache_execute_short_circuit_auto_reenable_after_cooldown;
        }

        void SetDecoupledCacheExecuteShortCircuitCooldownMs(uint64_t cooldown_ms)
        {
            decoupled_cache_execute_short_circuit_cooldown_ms = cooldown_ms > 0 ? cooldown_ms : 1;
        }
        uint64_t GetDecoupledCacheExecuteShortCircuitCooldownMs() const
        {
            return decoupled_cache_execute_short_circuit_cooldown_ms;
        }

        void SetDecoupledCacheExecuteShortCircuitWarningEnabled(bool enabled)
        {
            short_circuit_warning_enabled = enabled;
        }
        bool IsDecoupledCacheExecuteShortCircuitWarningEnabled() const { return short_circuit_warning_enabled; }

        void SetDecoupledCacheExecuteShortCircuitWarningFailureRatePercentThreshold(uint32_t threshold_percent)
        {
            short_circuit_warning_failure_rate_percent_threshold = threshold_percent > 100 ? 100 : threshold_percent;
        }
        uint32_t GetDecoupledCacheExecuteShortCircuitWarningFailureRatePercentThreshold() const
        {
            return short_circuit_warning_failure_rate_percent_threshold;
        }

        void SetDecoupledCacheExecuteShortCircuitWarningMinSamples(uint32_t min_samples)
        {
            short_circuit_warning_min_samples = min_samples > 0 ? min_samples : 1;
        }
        uint32_t GetDecoupledCacheExecuteShortCircuitWarningMinSamples() const
        {
            return short_circuit_warning_min_samples;
        }

        void SetDecoupledCacheExecuteShortCircuitWarningCooldownMs(uint64_t cooldown_ms)
        {
            short_circuit_warning_cooldown_ms = cooldown_ms > 0 ? cooldown_ms : 1;
        }
        uint64_t GetDecoupledCacheExecuteShortCircuitWarningCooldownMs() const
        {
            return short_circuit_warning_cooldown_ms;
        }

        uint32_t GetConsecutiveShortCircuitFailures() const { return consecutive_short_circuit_failures; }
        void ResetConsecutiveShortCircuitFailures() { consecutive_short_circuit_failures = 0; }
        uint32_t IncrementConsecutiveShortCircuitFailures() { return ++consecutive_short_circuit_failures; }

        void SetShortCircuitLastAutoDisableMs(uint64_t value) { short_circuit_last_auto_disable_ms = value; }
        uint64_t GetShortCircuitLastAutoDisableMs() const { return short_circuit_last_auto_disable_ms; }

        void SetShortCircuitDisabledByAutoGuardrail(bool value) { short_circuit_disabled_by_auto_guardrail = value; }
        bool IsShortCircuitDisabledByAutoGuardrail() const { return short_circuit_disabled_by_auto_guardrail; }

        void SetShortCircuitManualDisableLogged(bool value) { short_circuit_manual_disable_logged = value; }
        bool IsShortCircuitManualDisableLogged() const { return short_circuit_manual_disable_logged; }

        void SetLastShortCircuitWarningMs(uint64_t value) { last_short_circuit_warning_ms = value; }
        uint64_t GetLastShortCircuitWarningMs() const { return last_short_circuit_warning_ms; }

        uint64_t GetLastCacheStatsLogMs() const { return last_cache_stats_log_ms; }
        void SetLastCacheStatsLogMs(uint64_t value) { last_cache_stats_log_ms = value; }

        uint64_t GetCacheStatsLogIntervalMs() const { return cache_stats_log_interval_ms; }
        void SetCacheStatsLogIntervalMs(uint64_t value) { cache_stats_log_interval_ms = value > 0 ? value : 1; }

        MaterialResolveR21DryRunStats &GetFrameStats() { return frame_stats; }
        const MaterialResolveR21DryRunStats &GetFrameStats() const { return frame_stats; }
        void ResetFrameStats() { frame_stats = {}; }
        void ResetCumulativeStats() { cumulative_stats = {}; }
        void ResetRuntimeState()
        {
            consecutive_short_circuit_failures = 0;
            short_circuit_last_auto_disable_ms = 0;
            short_circuit_disabled_by_auto_guardrail = false;
            short_circuit_manual_disable_logged = false;
            last_short_circuit_warning_ms = 0;
            last_cache_stats_log_ms = 0;
        }
        void Reset()
        {
            ResetFrameStats();
            ResetCumulativeStats();
            ResetRuntimeState();
        }

        void RecordShortCircuitExecuted()
        {
            ++frame_stats.short_circuit_executed;
            ++cumulative_stats.short_circuit_executed_total;
            consecutive_short_circuit_failures = 0;
        }

        void RecordShortCircuitApplyFailure()
        {
            ++frame_stats.short_circuit_apply_failures;
            ++cumulative_stats.short_circuit_apply_failures_total;
            ++consecutive_short_circuit_failures;
        }

        void RecordShortCircuitAutoDisable()
        {
            ++frame_stats.short_circuit_auto_disable_events;
            ++cumulative_stats.short_circuit_auto_disable_events_total;
        }

        void RecordShortCircuitAutoReenable()
        {
            ++frame_stats.short_circuit_auto_reenable_events;
            ++cumulative_stats.short_circuit_auto_reenable_events_total;
        }

        void RecordShortCircuitManualDisableObserved()
        {
            ++cumulative_stats.short_circuit_manual_disable_observed_total;
        }

        void RecordShortCircuitWarningEvent()
        {
            ++cumulative_stats.short_circuit_warning_events_total;
        }

        bool TryAutoReenableShortCircuit(uint64_t now_ms)
        {
            if (!decoupled_cache_enabled
             || decoupled_cache_execute_short_circuit_enabled
             || !decoupled_cache_execute_short_circuit_auto_reenable_after_cooldown
             || !short_circuit_disabled_by_auto_guardrail)
                return false;

            if (now_ms < short_circuit_last_auto_disable_ms)
                return false;

            if (now_ms - short_circuit_last_auto_disable_ms < decoupled_cache_execute_short_circuit_cooldown_ms)
                return false;

            SetDecoupledCacheExecuteShortCircuitEnabled(true);
            SetShortCircuitDisabledByAutoGuardrail(false);
            RecordShortCircuitAutoReenable();
            return true;
        }

        bool TryObserveManualDisableBypass()
        {
            if (!decoupled_cache_enabled
             || decoupled_cache_execute_short_circuit_enabled
             || short_circuit_disabled_by_auto_guardrail
             || short_circuit_manual_disable_logged)
                return false;

            short_circuit_manual_disable_logged = true;
            RecordShortCircuitManualDisableObserved();
            return true;
        }

        void OnShortCircuitApplySuccess()
        {
            RecordShortCircuitExecuted();
        }

        bool OnShortCircuitApplyFailure(uint64_t now_ms)
        {
            RecordShortCircuitApplyFailure();

            if (!decoupled_cache_execute_short_circuit_auto_disable_on_failures
             || !decoupled_cache_execute_short_circuit_enabled
             || consecutive_short_circuit_failures < decoupled_cache_execute_short_circuit_failure_threshold)
                return false;

            SetDecoupledCacheExecuteShortCircuitEnabled(false);
            SetShortCircuitDisabledByAutoGuardrail(true);
            SetShortCircuitLastAutoDisableMs(now_ms);
            RecordShortCircuitAutoDisable();
            return true;
        }

        bool ShouldEmitPeriodicStatsLog(uint64_t now_ms)
        {
            if (now_ms - last_cache_stats_log_ms < cache_stats_log_interval_ms)
                return false;

            last_cache_stats_log_ms = now_ms;
            return true;
        }

        bool TryConsumeFailureRateWarning(uint64_t now_ms,
                                          uint64_t short_circuit_executed,
                                          uint64_t short_circuit_failed,
                                          uint64_t &out_failure_rate_percent,
                                          uint64_t &out_samples)
        {
            out_samples = short_circuit_executed + short_circuit_failed;
            out_failure_rate_percent = 0;

            if (!short_circuit_warning_enabled || out_samples == 0 || out_samples < short_circuit_warning_min_samples)
                return false;

            out_failure_rate_percent = (short_circuit_failed * 100ull) / out_samples;
            if (out_failure_rate_percent < short_circuit_warning_failure_rate_percent_threshold)
                return false;

            if (now_ms < last_short_circuit_warning_ms)
                return false;

            if (now_ms - last_short_circuit_warning_ms < short_circuit_warning_cooldown_ms)
                return false;

            last_short_circuit_warning_ms = now_ms;
            RecordShortCircuitWarningEvent();
            return true;
        }

        void GetR2CStats(MaterialResolveR2CStats &out) const
        {
            out = cumulative_stats;
            out.consecutive_short_circuit_failures = consecutive_short_circuit_failures;
            out.execute_short_circuit_enabled = decoupled_cache_execute_short_circuit_enabled;
            out.short_circuit_disabled_by_auto_guardrail = short_circuit_disabled_by_auto_guardrail;
        }
    };
}
