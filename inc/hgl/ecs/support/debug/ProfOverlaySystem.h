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
         * - phase = RenderDebug，位于绘制命令录制之后 ⇒ 文字随场景一起提交，永远最后叠显。
         * - 首次启用时强制 `FrameProfiler::SetEnabled(true)` 并加载 8x8 点阵字体
         *   （res/font/FM-T-437.F08），建一个常驻文本行实体池（不销毁、不注销组件）。
         * - 每 `kWindowFrames` 次 Update 汇总一次窗口数据（槽耗时/计数 +
         *   `SystemProfiler` 逐系统耗时 + SceneStats），只在文本变化的行上重设内容。
         *
         * 约束（重要）：
         * - **不安装 Text 系统组**：在 Update/Initialize 里注册系统会向
         *   `render_system_order` 追加元素，而该表正被渲染循环迭代 ⇒ 悬垂迭代器。
         *   因此要求宿主应用已装好 Text 组，否则记一次错误并永久停用。
         * - 窗口帧数按 **Update 调用次数**计：多 pass 世界（离屏/阴影 pass 也会触发
         *   RenderDebug）时一次真实帧会多次计数 ⇒ 帧率/均值按调用数归一，语义为
         *   "每次渲染 pass 一帧"。
         * - 文本在 RenderDebug 阶段写入，收集在下帧 ⇒ 面板天然滞后一帧，可接受。
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
