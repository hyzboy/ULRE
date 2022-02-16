#ifndef HGL_GRAPH_TEXT_RENDER_INCLUDE
#define HGL_GRAPH_TEXT_RENDER_INCLUDE

#include<hgl/graph/VK.h>
#include<hgl/type/Color4f.h>

namespace hgl
{
    namespace graph
    {
        class FontSource;
        class FontSourceMulti;
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

            FontSource *        eng_fs              =nullptr;
            FontSource *        chs_fs              =nullptr;
            FontSourceMulti *   font_source         =nullptr;

            TileFont *          tile_font           =nullptr;
            TextLayout *        tl_engine           =nullptr;
    
            Color4f             color;
            GPUBuffer *         ubo_color           =nullptr;

        private:

            friend TextRender *CreateTextRender(GPUDevice *,RenderPass *,GPUBuffer *);
            TextRender(GPUDevice *dev);

        public:

            ~TextRender();

        private:    

            bool InitTileFont();
            bool InitTextLayoutEngine();
            bool InitUBO();
            bool InitMaterial(RenderPass *,GPUBuffer *);

        public:

            bool Init(RenderPass *rp,GPUBuffer *ubo_camera_info);

            TextRenderable *CreateRenderable();
            RenderableInstance *CreateRenderableInstance(TextRenderable *text_render_obj,const UTF16String &str);
        };//class TextRender

        TextRender *CreateTextRender(GPUDevice *,RenderPass *,GPUBuffer *);
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_TEXT_RENDER_INCLUDE
