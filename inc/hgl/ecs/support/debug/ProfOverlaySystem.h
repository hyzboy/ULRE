#pragma once

#include<hgl/ecs/core/System.h>
#include<hgl/type/String.h>

#include<vector>

namespace hgl
{
    namespace graph
    {
        class FontSource;
        class MonoBitmapFontSource;
    }

    namespace ecs
    {
        class Entity;
        class TextComponent;

        /**
         * 屏显 DEBUG PROF 覆盖层（`ULRE_DEBUG.toml` 的 `channels.profui` 开启）。
         *
         * 工作方式：
         * - phase = TickPostCamera，走 `ECSContext::Tick` 的 tick 系统列。
         *   （不能用渲染相位：普通 System 的 Update 只有两个派发口 —— Tick 的
         *   tick_system_order（仅非渲染相位）与 RenderGraph pass 循环的
         *   RunRenderPhaseUpdates（仅各 pass 的 [startPhase,endPhase] 范围，而
         *   pass 按渲染元素类型分组建）。RenderDebug(13) 不属于任何 pass 的
         *   范围 ⇒ Update 永不执行；PrimitiveOverlayRenderSystem 能跑是因为它是
         *   RenderPipelineDrawSystem，由 draw call 驱动而非相位 Update。）
         *   文本写入发生在 Collect/Batch 之前 ⇒ 面板与所统计的帧同批出图。
         * - 首次启用时强制 `FrameProfiler::SetEnabled(true)` 并加载 8x8 点阵字体
         *   （res/font/FM-T-437.F08），建一个常驻文本行实体池（不销毁、不注销组件）。
         * - 每 `kWindowFrames` 次 Update 汇总一次窗口数据（槽耗时/计数 +
         *   `SystemProfiler` 逐系统耗时 + SceneStats），只在文本变化的行上重设内容。
         *
         * 约束（重要）：
         * - **不安装 Text 系统组**：在 Update/Initialize 里注册系统会向
         *   `render_system_order` 追加元素，而该表正被渲染循环迭代 ⇒ 悬垂迭代器。
         *   因此要求宿主应用已装好 Text 组，否则每帧重试并记一次警告。
         * - 窗口帧数按 **Tick 中的 Update 调用次数**计 = 真实帧数
         *   （一次 Tick 对应一次渲染帧）。
         * - 面板显示的是**上一个窗口**的数据（窗口末尾才有汇总值），滞后 ≤ 窗口长度，
         *   对 debug 叠加无碍。
         */
        class ProfOverlaySystem : public System
        {
        public:
            static constexpr int kMaxLines     = 48;   ///< 文本行池容量
            static constexpr int kWindowFrames = 60;   ///< 汇总窗口（Update 次数）

        private:
            struct Line
            {
                Entity        * entity = nullptr;
                TextComponent * comp   = nullptr;
                AnsiString      text;                  ///< 当前显示的 ASCII 文本（未变化则跳过重排）
            };

        private:
            std::vector<Line> lines;                   ///< 行池（Size = kMaxLines）
            graph::MonoBitmapFontSource * data_source = nullptr;
            graph::FontSource     * font_source  = nullptr;
            bool   active  = false;                    ///< 已完成懒初始化
            bool   failed  = false;                    ///< 初始化失败 ⇒ 永久停用
            uint64_t window_t0    = 0;                 ///< 窗口起点（GetUptimeUs）
            uint64_t window_calls = 0;                 ///< 窗口内已计 Update 次数

        public:
            ProfOverlaySystem();
            ~ProfOverlaySystem() override;

            void Update(float deltaTime) override;

        private:
            /// 懒初始化：检查 Text 组、加载字体、建行池。失败返回 false。
            bool Setup();

            /// 汇总一个窗口并刷新全部行（wall_us = 本窗口墙钟微秒）
            void Rebuild(uint64_t wall_us);

            /// 写第 idx 行：文本未变化则跳过；color 为 packUnorm4x8
            void PutLine(int idx, uint32_t color, const char * ascii, int len);
        };
    }//namespace ecs
}//namespace hgl
