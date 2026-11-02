#pragma once

#include<hgl/ecs/core/System.h>
#include<hgl/vk/buffer/StructView.h>
#include<hgl/graph/ubo/ColorPalette.h>
#include<hgl/color/Color4f.h>

namespace hgl
{
    namespace graph
    {
        class RenderContext;
    }

    namespace ecs
    {
        using UBOColorPalette = graph::StructView<graph::ColorPalette>;

        /**
         * ColorPaletteSystem
         *
         * 管理全局顶点调色板（BDA：SSBO + 设备地址，无绑定无集）。
         * 懒创建 buffer、dirty 追踪、地址注册进全局地址表
         *（global_addresses.addr_color_palette）；内容原地更新，地址不变。
         */
        class ColorPaletteSystem : public System
        {
        private:

            graph::RenderContext *render_context = nullptr;
            UBOColorPalette *palette_ubo = nullptr;
            bool palette_ubo_managed = false;

            graph::ColorPalette palette_cpu_;   ///< CPU 侧调色板（RGBA8 打包），构造期以 COLOR 命名色表填充
            bool palette_dirty_ = false;

        public:

            ColorPaletteSystem(const std::string &name = "ColorPaletteSystem");
            ~ColorPaletteSystem() override;

            void SetRenderContext(graph::RenderContext *ctx) { render_context = ctx; }
            // 2026-10-10（审计 §1.1）：整个编辑 API（`GetPaletteUBO()` / `SetColor()` / `ResetToWhite()`）
            // 全仓零调用者 ⇒ 删除。`palette_ubo` 成员与 `Flush()` 仍由系统自身的初始化/更新路径在用。

            void Initialize() override;
            void Update(float deltaTime) override;

        private:

            void EnsureResources();
            void Flush();
        };
    }//namespace ecs
}//namespace hgl
