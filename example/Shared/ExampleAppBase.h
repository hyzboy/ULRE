#pragma once

#include <hgl/framework/WorkObject.h>
#include <hgl/type/String.h>
#include <hgl/log/Log.h>
#include "FrameDigest.h"
#include <cstdlib>

namespace ulre_example
{
    /**
     * @brief 示例应用程序基础配置
     */
    struct ExampleConfig
    {
        const char *    env_prefix       = nullptr; ///< 环境变量前缀（如 "GC" -> GC_SELFCHECK, GC_FRAMES, GC_DUMP 等）
        const char *    dump_prefix      = nullptr; ///< 落盘文件前缀（如 "gcdump"，若为空则自动为 env_prefix + "dump" 的小写）
        const char *    selfcheck_tag    = nullptr; ///< 自检日志 tag（如 "GCSELF"，若为空则自动为 env_prefix + "SELF"）
        uint32_t        selfcheck_frame  = 8;       ///< 目标自检帧号（默认第 8 帧）
        uint32_t        dump_frame       = 8;       ///< 默认落盘帧号（默认第 8 帧，可通过 <PREFIX>_DUMP_FRAME 覆盖）
        FrameDigest     expected_digest{};          ///< 期望的帧摘要基线
    };

    /**
     * @brief 示例程序统一基类，继承自 WorkObject。
     *
     * 自动接管：
     * 1. 环境变量解析（<PREFIX>_SELFCHECK, <PREFIX>_FRAMES, <PREFIX>_DUMP, <PREFIX>_DUMP_FRAME）；
     * 2. 帧计数（frame_seen）、定帧退出与性能耗时统计；
     * 3. 自动安装 SelfCheck 摘要比对钩子与 Raw RT Dump 落盘钩子。
     */
    class ExampleAppBase : public hgl::WorkObject
    {
    protected:
        ExampleConfig   example_config;
        uint32_t        example_frame_seen        = 0;
        bool            example_selfcheck_enabled = false;
        uint32_t        example_perf_frames       = 0;
        hgl::AnsiString example_dump_tag;
        uint32_t        example_target_dump_frame = 8;

        hgl::AnsiString resolved_dump_prefix;
        hgl::AnsiString resolved_selfcheck_tag;

        hgl::uint64     example_perf_t0           = 0;
        double          example_perf_total_us     = 0.0;

    protected:
        /// 设置示例配置并解析对应环境变量
        void SetupExampleConfig(const ExampleConfig & cfg)
        {
            example_config = cfg;
            example_target_dump_frame = cfg.dump_frame;

            if (cfg.dump_prefix && cfg.dump_prefix[0] != '\0')
            {
                resolved_dump_prefix = cfg.dump_prefix;
            }
            else if (cfg.env_prefix && cfg.env_prefix[0] != '\0')
            {
                hgl::AnsiString p = cfg.env_prefix;
                p.LowerCase();
                resolved_dump_prefix = p + "dump";
            }
            else
            {
                resolved_dump_prefix = "dump";
            }

            if (cfg.selfcheck_tag && cfg.selfcheck_tag[0] != '\0')
            {
                resolved_selfcheck_tag = cfg.selfcheck_tag;
            }
            else if (cfg.env_prefix && cfg.env_prefix[0] != '\0')
            {
                resolved_selfcheck_tag = hgl::AnsiString(cfg.env_prefix) + "SELF";
            }
            else
            {
                resolved_selfcheck_tag = "SELF";
            }

            ParseExampleEnv();
        }

        /// 解析环境变量
        virtual void ParseExampleEnv()
        {
            if (!example_config.env_prefix || example_config.env_prefix[0] == '\0')
                return;

            const hgl::AnsiString prefix(example_config.env_prefix);

            // <PREFIX>_SELFCHECK
            const hgl::AnsiString selfcheck_var = prefix + "_SELFCHECK";
            example_selfcheck_enabled = (getenv(selfcheck_var.c_str()) != nullptr);

            // <PREFIX>_FRAMES
            const hgl::AnsiString frames_var = prefix + "_FRAMES";
            if (const char * v = getenv(frames_var.c_str()))
            {
                example_perf_frames = static_cast<uint32_t>(atoi(v));
            }

            // <PREFIX>_DUMP
            const hgl::AnsiString dump_var = prefix + "_DUMP";
            if (const char * tag = getenv(dump_var.c_str()))
            {
                example_dump_tag = tag;
            }

            // <PREFIX>_DUMP_FRAME
            const hgl::AnsiString df_var = prefix + "_DUMP_FRAME";
            if (const char * df = getenv(df_var.c_str()))
            {
                example_target_dump_frame = static_cast<uint32_t>(atoi(df));
            }
        }

        /// 派生类可重载以提供自定义渲染目标（默认从 ECSContext 取）
        virtual hgl::graph::IRenderTarget * GetActiveRenderTarget()
        {
            auto * ctx = GetECSContext();
            return ctx ? ctx->Environment().GetRenderTarget() : nullptr;
        }

        /// 派生类实现具体的初始化逻辑
        virtual bool OnInit() { return true; }

        /// 派生类实现具体的每帧更新逻辑（在 WorkObject::Tick 之前调用，适合更新相机/变换）
        virtual void OnPreTick(double delta_time) {}

        /// 派生类实现具体的每帧更新逻辑（在 WorkObject::Tick 之后调用，适合采样/验证）
        virtual void OnTick(double delta_time) {}

    public:
        ExampleAppBase() = default;
        explicit ExampleAppBase(const ExampleConfig & cfg)
        {
            SetupExampleConfig(cfg);
        }

        ~ExampleAppBase() override = default;

        bool Init() override
        {
            return OnInit();
        }

        void Tick(double delta_time) override
        {
            const bool perf_on = (example_perf_frames > 0);
            if (perf_on)
            {
                example_perf_t0 = hgl::GetUptimeUs();
            }

            OnPreTick(delta_time);

            WorkObject::Tick(delta_time);
            ++example_frame_seen;

            // 派生类更新逻辑
            OnTick(delta_time);

            auto * rt = GetActiveRenderTarget();

            // SelfCheck 钩子
            CheckAndInstallSelfCheckHook(
                rt,
                example_selfcheck_enabled && (example_frame_seen == example_config.selfcheck_frame),
                example_config.expected_digest,
                resolved_selfcheck_tag.c_str());

            // Dump 钩子
            CheckAndInstallDumpHookAtFrame(
                rt,
                example_dump_tag,
                example_frame_seen,
                resolved_dump_prefix.c_str(),
                example_target_dump_frame);

            // 定帧性能/冒烟退出
            if (perf_on)
            {
                example_perf_total_us += static_cast<double>(hgl::GetUptimeUs() - example_perf_t0);

                if (example_frame_seen >= example_perf_frames)
                {
                    const double avg_ms = (example_frame_seen > 0)
                        ? (example_perf_total_us / double(example_frame_seen) / 1000.0)
                        : 0.0;
                    GLogInfo(u8"[%s] done frames=%u total_ms=%.2f avg_ms=%.3f",
                             example_config.env_prefix ? example_config.env_prefix : "PERF",
                             example_frame_seen,
                             example_perf_total_us / 1000.0,
                             avg_ms);
                    std::exit(0);
                }
            }
        }

        uint32_t GetCurrentFrame() const { return example_frame_seen; }
        bool IsSelfCheckEnabled() const { return example_selfcheck_enabled; }
        bool IsDumpEnabled() const { return !example_dump_tag.IsEmpty(); }
        const hgl::AnsiString & GetDumpTag() const { return example_dump_tag; }
    };
}
