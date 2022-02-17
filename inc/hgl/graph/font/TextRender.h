#ifndef HGL_GRAPH_TEXT_RENDER_INCLUDE
#define HGL_GRAPH_TEXT_RENDER_INCLUDE

#include<hgl/graph/VK.h>
#include<hgl/type/Color4f.h>

namespace hgl
{
    namespace graph
    {
        class FontSource;
        class TileFont;
        class TextLayout;
        class TextRenderable;

        class TextRender
        {
            GPUDevice *         device              =nullptr;
            RenderResource *    db                  =nullptr;

            Material *          material            =nullptr;
            MaterialInstance *  material_instance   =nullptr;

            Sampler *           sampler             =nullptr;

            Pipeline *          pipeline            =nullptr;

            FontSource *        font_source         =nullptr;

            TileFont *          tile_font           =nullptr;
            TextLayout *        tl_engine           =nullptr;
    
            Color4f             color;
            GPUBuffer *         ubo_color           =nullptr;

        private:

            friend TextRender *CreateTextRender(GPUDevice *,FontSource *,RenderPass *,GPUBuffer *);
            TextRender(GPUDevice *dev,FontSource *);

            bool InitTileFont();
            bool InitTextLayoutEngine();
            bool InitUBO();
            bool InitMaterial(RenderPass *,GPUBuffer *);

        public:

            ~TextRender();

            bool Init(RenderPass *rp,GPUBuffer *ubo_camera_info);

        public:

            TextRenderable *CreateRenderable();
            RenderableInstance *CreateRenderableInstance(TextRenderable *text_render_obj,const UTF16String &str);
        };//class TextRender

        /**
         * 创建一个CJK字体源
         * @param cf CJK字体名称
         * @param lf 其它字体名称
         * @param size 字体象素高度
         */
        FontSource *CreateCJKFontSource(const os_char *cf,const os_char *lf,const uint32_t size);

        /**
         * 创建一个字体源
         * @param name 字体名称
         * @param size 字体象素高度
         */
        FontSource *CreateFontSource(const os_char *name,const uint32_t size);

        /**
         * 创建一个文本渲染器.
         */
        TextRender *CreateTextRender(GPUDevice *,FontSource *,RenderPass *,GPUBuffer *);
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_TEXT_RENDER_INCLUDE
