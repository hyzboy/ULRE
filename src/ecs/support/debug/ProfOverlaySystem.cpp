#include<hgl/ecs/support/debug/ProfOverlaySystem.h>

#include<hgl/color/Color4ub.h>
#include<hgl/ecs/components/TextComponent.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/core/render/RenderGraph.h>
#include<hgl/ecs/support/storage/ComponentTypeTable.h>
#include<hgl/ecs/support/storage/FrameProfiler.h>
#include<hgl/graph/font/FontSource.h>
#include<hgl/graph/font/MonoBitmapFontSource.h>
#include<hgl/graph/font/TextCharSSBO.h>
#include<hgl/time/Time.h>

#include<cstdio>

namespace hgl::ecs
{
    namespace
    {
        /// ExecutionPhase 0..15 的短名（面板 11 列宽）
        const char * const kPhaseNames[16] =
        {
            "TickInput",  "TickPhysics", "TickTransform", "TickCamera", "TickPostCamera",
            "SwapNextImg","PreBegin",    "Collect",       "Batch",      "BufCommit",
            "BufUpload",  "FrameSync",   "DrawSubmit",    "Debug",      "Stat",
            "Submit",
        };

        constexpr int kTopModules = 12;   ///< MODULES 段显示前 N 个系统
        constexpr int kMaxTextLen = 240;  ///< 单行字符上限（ASCII）

        U16String MakeU16(const char * ascii, int len)
        {
            if (len < 0)   len = 0;
            if (len > kMaxTextLen) len = kMaxTextLen;

            u16char buf[kMaxTextLen + 1];

            for (int i = 0; i < len; ++i)
                buf[i] = static_cast<u16char>(static_cast<uint8_t>(ascii[i]));

            return U16String(buf, len);
        }

        /// snprintf 返回值夹到安全偏移（防超缓冲）
        inline int ClampOff(int off)
        {
            if (off < 0) return 0;
            return off > 200 ? 200 : off;
        }
    }//anonymous

    ProfOverlaySystem::ProfOverlaySystem()
        : System("ProfOverlay")
    {
        // 必须用 tick 相位：普通 System 的 Update 只由 Tick(tick_system_order) 或
        // RenderGraph pass 范围派发；渲染相位 RenderDebug 不在任何 pass 范围内 ⇒ 永不执行。
        SetExecutionPhase(ExecutionPhase::TickPostCamera);
    }

    ProfOverlaySystem::~ProfOverlaySystem()
    {
        // 不销毁实体/字体：实体生命周期归世界，FontSource 按世界存活惯例随进程释放
        // （与 TextDrawMonoBitmap 示例一致；析构期上下文已不可用，无法安全回收）。
    }

    bool ProfOverlaySystem::Setup()
    {
        if (!context)
        {
            failed = true;
            return false;
        }

        // 绝不在此注册系统（会动正被迭代的 render_system_order）⇒ 只依赖宿主已装好的 Text 组。
        if (!context->IsSystemGroupInstalled(SystemGroupID::Text))
        {
            static bool s_warned = false;
            if (!s_warned)
            {
                s_warned = true;
                LogWarning(u8"profui: Text system group not installed, overlay waits for it");
            }
            return false;   // 不置 failed ⇒ 下个 Update 再试（只是一次 bool 检查）
        }

        data_source = new graph::MonoBitmapFontSource(8, 8);

        if (!data_source->Init(OS_TEXT("res/font/FM-T-437.F08")))
        {
            delete data_source;
            data_source = nullptr;
            failed = true;
            LogError(u8"profui: bitmap font load failed: res/font/FM-T-437.F08");
            return false;
        }

        font_source = new graph::FontSource(data_source);   // 只接管引用计数，数据源须同寿命

        lines.resize(kMaxLines);

        for (int i = 0; i < kMaxLines; ++i)
        {
            Line & l = lines[i];

            l.entity = context->CreateEntity();
            if (!l.entity)
            {
                failed = true;
                LogError(u8"profui: entity pool creation failed at line %d", i);
                return false;
            }

            l.comp = l.entity->AddComponent<TextComponent>();
            if (!l.comp)
            {
                failed = true;
                LogError(u8"profui: TextComponent attach failed at line %d", i);
                return false;
            }

            l.comp->SetFontSource(font_source);
            l.comp->SetStartPosition(graph::layout::TEXT_COORD_VEC{ 6, 6 + i * 10 });
        }

        return true;
    }

    void ProfOverlaySystem::Update(float /*deltaTime*/)
    {
        // 通道运行期不可变（ULRE_DEBUG.toml 只在启动读取一次）⇒ 函数本地 static 缓存。
        static const bool s_channel = ULRE_DEBUG_ON(ProfUI);

        if (!s_channel)
            return;

        if (!active)
        {
            if (failed)
                return;

            if (!Setup())
                return;

            FrameProfiler::SetEnabled(true);   // 覆盖层存在即强制计时

            window_t0    = GetUptimeUs();
            window_calls = 0;
            active       = true;
            return;
        }

        // Tick 中每真实帧调用一次 Update ⇒ window_calls 即真实帧数。
        if (++window_calls < kWindowFrames)
            return;

        const uint64_t wall = GetUptimeUs() - window_t0;

        Rebuild(wall);

        FrameProfiler::Reset();   // 连同 SystemProfiler / 计数槽一起清零

        window_t0    = GetUptimeUs();
        window_calls = 0;
    }

    void ProfOverlaySystem::PutLine(int idx, uint32_t color, const char * ascii, int len)
    {
        if (idx < 0 || idx >= kMaxLines || idx >= static_cast<int>(lines.size()))
            return;

        Line & l = lines[idx];
        if (!l.comp)
            return;

        if (len < 0) len = 0;

        AnsiString cur(ascii, len);
        if (cur == l.text)
            return;   // 未变化 ⇒ 不 Touch Content，避免每帧无谓重排

        l.comp->SetText(MakeU16(ascii, len));

        graph::layout::CharStyle cs;
        cs.text_color = color;
        cs.scale      = 1.0f;   // 原生 8x8，不放大；行距 10px
        l.comp->SetCharStyle(cs);

        l.text = cur;
    }

    void ProfOverlaySystem::Rebuild(uint64_t wall_us)
    {
        if (wall_us == 0)
            wall_us = 1;

        const double inv     = 1.0 / static_cast<double>(kWindowFrames);
        const double fps     = static_cast<double>(kWindowFrames) * 1e6 / static_cast<double>(wall_us);
        const double frame_ms = static_cast<double>(wall_us) * 1e-3 * inv;
        const double attr_ms  = static_cast<double>(FrameProfiler::SumUs()) * 1e-3 * inv;
        const double unattr_ms = frame_ms > attr_ms ? frame_ms - attr_ms : 0.0;

        const Color4ub c_header(255, 220,  60, 255);   // 黄：总览行
        const Color4ub c_title (255, 180, 100, 255);   // 橙：段标题
        const Color4ub c_white (255, 255, 255, 255);
        const Color4ub c_gray  (160, 160, 160, 255);
        const Color4ub c_cyan  ( 80, 220, 255, 255);
        const Color4ub c_green (120, 255, 120, 255);
        const Color4ub c_red   (255,  80,  80, 255);

        char buf[256];
        int  n   = 0;
        int  len = snprintf(buf, sizeof(buf),
                            "ULRE PROF   FPS %7.1f   FRAME %7.2f   ATTR %7.2f   UNATTR %7.2f  ms",
                            fps, frame_ms, attr_ms, unattr_ms);
        PutLine(n++, c_header.packUnorm4x8(), buf, len);

        // ── STAGES：16 个互不重叠的顶层槽（17..23 属 SUB 明细），每行 2 对定宽列 ──
        len = snprintf(buf, sizeof(buf), "== STAGES ms/frame | %%wall ==");
        PutLine(n++, c_title.packUnorm4x8(), buf, len);

        int tops[32];
        int ntop = 0;
        for (int i = 0; i < static_cast<int>(FrameProfileSlot::Count); ++i)
        {
            if (i >= static_cast<int>(FrameProfileSlot::CollectPreScan) &&
                i <= static_cast<int>(FrameProfileSlot::BatchFinalize))
                continue;   // SUB 段单独显示，避免与父槽混排
            tops[ntop++] = i;
        }

        for (int i = 0; i < ntop; i += 2)
        {
            const FrameProfileSlot a = static_cast<FrameProfileSlot>(tops[i]);

            const uint64_t ua   = FrameProfiler::GetUs(a);
            const double   a_ms = static_cast<double>(ua) * 1e-3 * inv;
            const double   a_pct= static_cast<double>(ua) * 100.0 / static_cast<double>(wall_us);

            if (i + 1 < ntop)
            {
                const FrameProfileSlot b = static_cast<FrameProfileSlot>(tops[i + 1]);

                const uint64_t ub   = FrameProfiler::GetUs(b);
                const double   b_ms = static_cast<double>(ub) * 1e-3 * inv;
                const double   b_pct= static_cast<double>(ub) * 100.0 / static_cast<double>(wall_us);

                len = snprintf(buf, sizeof(buf), "%-18.18s %7.2f %6.1f   %-18.18s %7.2f %6.1f",
                               GetFrameProfileSlotName(a), a_ms, a_pct,
                               GetFrameProfileSlotName(b), b_ms, b_pct);

                PutLine(n++, (a_pct > 30.0 || b_pct > 30.0) ? c_red.packUnorm4x8()
                                                            : c_white.packUnorm4x8(),
                        buf, len);
            }
            else
            {
                len = snprintf(buf, sizeof(buf), "%-18.18s %7.2f %6.1f",
                               GetFrameProfileSlotName(a), a_ms, a_pct);

                PutLine(n++, (a_pct > 30.0) ? c_red.packUnorm4x8() : c_white.packUnorm4x8(),
                        buf, len);
            }
        }

        // ── SUB：Collect/Batch 的内部拆分明细（与父槽重叠，仅供定位）──────────
        len = snprintf(buf, sizeof(buf), "== SUB (of Collect/Batch, overlap parents) ==");
        PutLine(n++, c_title.packUnorm4x8(), buf, len);

        for (int i = static_cast<int>(FrameProfileSlot::CollectPreScan);
                i <= static_cast<int>(FrameProfileSlot::BatchFinalize); i += 3)
        {
            int off = 0;
            buf[0] = 0;

            for (int k = i; k < i + 3 && k <= static_cast<int>(FrameProfileSlot::BatchFinalize); ++k)
                off += snprintf(buf + off, sizeof(buf) - off, "%-20.20s%7.2f  ",
                                GetFrameProfileSlotName(static_cast<FrameProfileSlot>(k)),
                                static_cast<double>(FrameProfiler::GetUs(static_cast<FrameProfileSlot>(k))) * 1e-3 * inv);

            off = ClampOff(off);
            PutLine(n++, c_gray.packUnorm4x8(), buf, off);
        }

        // ── MODULES：按窗口耗时降序的前 N 个系统 ──────────────────────────────
        std::vector<SystemProfileEntry> entries;
        SystemProfiler::Snapshot(entries);

        len = snprintf(buf, sizeof(buf), "== MODULES top%d ==", kTopModules);
        PutLine(n++, c_title.packUnorm4x8(), buf, len);

        len = snprintf(buf, sizeof(buf), "%-24.24s %-11.11s %8s %6s %8s",
                       "NAME", "PHASE", "ms", "%wall", "calls");
        PutLine(n++, c_title.packUnorm4x8(), buf, len);

        const int total = static_cast<int>(entries.size());
        std::vector<char> taken(total, 0);

        for (int rank = 0; rank < kTopModules && rank < total; ++rank)
        {
            int      best = -1;
            uint64_t best_us = 0;

            for (int i = 0; i < total; ++i)
            {
                if (!taken[i] && entries[i].us > best_us)
                {
                    best_us = entries[i].us;
                    best = i;
                }
            }

            if (best < 0)
                break;

            taken[best] = 1;

            const SystemProfileEntry & e = entries[best];
            const uint32_t phase = e.phase < 16 ? e.phase : 15;

            len = snprintf(buf, sizeof(buf), "%-24.24s %-11.11s %8.2f %6.1f %8u",
                           e.name.c_str(), kPhaseNames[phase],
                           static_cast<double>(e.us) * 1e-3,
                           static_cast<double>(e.us) * 100.0 / static_cast<double>(wall_us),
                           static_cast<unsigned>(e.calls));
            PutLine(n++, c_cyan.packUnorm4x8(), buf, len);
        }

        // ── COUNTS：每帧平均工作量 ────────────────────────────────────────────
        len = snprintf(buf, sizeof(buf), "== COUNTS avg/frame ==");
        PutLine(n++, c_title.packUnorm4x8(), buf, len);

        {
            int off = 0;
            buf[0] = 0;

            for (int i = 0; i <= static_cast<int>(FrameProfileCount::CollectCalls); ++i)
            {
                const FrameProfileCount c = static_cast<FrameProfileCount>(i);

                off += snprintf(buf + off, sizeof(buf) - off, "%-18.18s %8.0f ",
                                GetFrameProfileCountName(c),
                                static_cast<double>(FrameProfiler::GetCount(c)) * inv);
            }

            off = ClampOff(off);
            PutLine(n++, c_white.packUnorm4x8(), buf, off);
        }

        // ── SCENE：参与自适应图构建的渲染组 ───────────────────────────────────
        const SceneStats stats = GatherSceneStats(context);

        len = snprintf(buf, sizeof(buf), "== SCENE ==");
        PutLine(n++, c_title.packUnorm4x8(), buf, len);

        len = snprintf(buf, sizeof(buf), "groups %3u   hash %016llx",
                       static_cast<unsigned>(stats.active_render_groups.size()),
                       static_cast<unsigned long long>(stats.GetHash()));
        PutLine(n++, c_green.packUnorm4x8(), buf, len);

        {
            int off = snprintf(buf, sizeof(buf), "  ");
            for (const std::string & g : stats.active_render_groups)
                off += snprintf(buf + off, sizeof(buf) - off, "%-18.18s", g.c_str());

            off = ClampOff(off);
            PutLine(n++, c_green.packUnorm4x8(), buf, off);
        }

        // 其余行池清空（隐藏）
        for (; n < kMaxLines; ++n)
            PutLine(n, 0, "", 0);
    }
}//namespace hgl::ecs
