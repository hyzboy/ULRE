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
            GPUDevice *         device;
            RenderResource *    db;

            Material *          material;
            MaterialInstance *  material_instance;

            Sampler *           sampler;

            Pipeline *          pipeline;

            FontSource *        font_source;

            TileFont *          tile_font;
            TextLayout *        tl_engine;
    
            Color4f             color;
            GPUBuffer *         ubo_color;

            SortedSets<TextRenderable *> tr_sets;

        private:

            friend TextRender *CreateTextRender(GPUDevice *,FontSource *,RenderPass *,GPUBuffer *,int limit=-1);
            TextRender(GPUDevice *dev,FontSource *);

            bool InitTileFont(int limit);
            bool InitTextLayoutEngine();
            bool InitUBO();
            bool InitMaterial(RenderPass *,GPUBuffer *);

        public:

            ~TextRender();

            bool Init(RenderPass *rp,GPUBuffer *ubo_camera_info,int limit);

        public:

            TextRenderable *CreateRenderable();
            TextRenderable *CreateRenderable(const UTF16String &str);

            bool Layout(TextRenderable *tr,const UTF16String &str);

            RenderableInstance *CreateRenderableInstance(TextRenderable *text_render_obj);

            void Release(TextRenderable *);
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
        FontSource *AcquireFontSource(const os_char *name,const uint32_t size);

        /**
         * 创建一个文本渲染器
         * @param limit 节数限制(-1表示自动)
         */
        TextRender *CreateTextRender(GPUDevice *,FontSource *,RenderPass *,GPUBuffer *,int limit);
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_TEXT_RENDER_INCLUDE
