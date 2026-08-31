#pragma once

#include<hgl/ecs/core/System.h>
#include<hgl/vk/StructuredBufferAccessor.h>
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
        using UBOColorPalette = graph::StructuredBufferAccessor<graph::ColorPalette>;

        /**
         * ColorPaletteSystem
         *
         * 管理全局顶点调色板 UBO（Scene 集，Set 0, binding=3）。
         * 与 EnvironmentSystem 一样，作为跨材质全局 UBO 的拥有者：
         * 懒创建 UBO、dirty 追踪、写入全局 Scene 描述符集一次。
         */
        class ColorPaletteSystem : public System
        {
        private:

            graph::RenderContext *render_context = nullptr;
            UBOColorPalette *palette_ubo = nullptr;
            bool palette_ubo_managed = false;

            graph::ColorPalette palette_cpu_;   ///< CPU 侧调色板（RGBA8 打包），默认全白
            bool palette_dirty_ = false;

        public:

            ColorPaletteSystem(const std::string &name = "ColorPaletteSystem");
            ~ColorPaletteSystem() override;

            void SetRenderContext(graph::RenderContext *ctx) { render_context = ctx; }
            UBOColorPalette *GetPaletteUBO() const { return palette_ubo; }

            /// 设置调色板某一项颜色（立即提交，若资源已就绪）。
            void SetColor(int index, const hgl::Color4f &color);

            /// 重置整张调色板为白色。
            void ResetToWhite();

            void Initialize() override;
            void Update(float deltaTime) override;

        private:

            void EnsureResources();
            void Flush();
        };
    }//namespace ecs
}//namespace hgl
