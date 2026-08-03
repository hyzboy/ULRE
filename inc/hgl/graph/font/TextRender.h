#pragma once

#include<hgl/vk/VK.h>
#include<hgl/graph/font/TextLayout.h>
#include<hgl/type/OrderedSet.h>
#include<hgl/type/String.h>

namespace hgl
{
    class AppFramework;
}

namespace hgl::graph
{
    class FontDataSource;
    class FontSource;
    class GraphicsContext;
    class RenderPass;
    class TileFont;
    class TextGeometry;
    class ShaderProgramManager;
    class DescriptorBindingSet;
    class DeviceBuffer;

    namespace layout
    {
        class TextLayout;
    }//namespace layout

    enum class TextGeometryType:uint8
    {
        /**
        * 固定风格，所有的字符使用同一种风格绘制
        */
        FixedStyle=0,

        /**
        * 每个字符可以不同风格，最大不能超过256种。
        * 在绘制前，会通过一个格式为R8UI的VertexAttribute传递每个字符的风格ID，所以最多不能超过256种风格。
        */
        StylePerChar,
    };

    class TextRender
    {
        VulkanDevice *      device;
        GraphicsContext *   graphics_context;

        ShaderProgramManager *   mtl_manager;

        Sampler *           sampler;

        Pipeline *          pipeline;

        TileFont *          tile_font;
        layout::TextLayout *tl_engine;

        layout::ParagraphStyle para_style;                  ///<段落风格
        layout::TextDrawStyle text_draw_style;              ///<文本绘制风格

    private:    //fixed style 资源

        layout::CharStyle   fixed_style;                    ///<固定字符风格

        ShaderProgram *          mtl_fs;                         ///<固定风格材质
        const VIL *         binding_vil;                    ///<VIL for the fixed-style pipeline
        DescriptorBindingSet *binding_set;                  ///<descriptor binding for font rendering
        DeviceBuffer *      mi_ssbo;                        ///<SSBO holding CharStyle data

    private:

        OrderedSet<TextGeometry *> text_geometry_set;        ///<所有的文字绘制几何体

    private:

        bool SimpleLayout(TextGeometry *tr,const U16StringView &str);              ///<简单文本排版

    private:

        friend class AppFramework;

        TextRender(GraphicsContext *,TileFont *);
        TextRender(AppFramework *,TileFont *);

        bool InitTextLayoutEngine();
        bool InitMaterial(RenderPass *);
        bool Init(RenderPass *,Sampler *);

    public:

        ~TextRender();

    public:

        static TextRender *CreateWithGraphicsContext(GraphicsContext *gc,RenderPass *rp,FontSource *fs,int limit=1024,const VkExtent2D *extent=nullptr);

        TextGeometry *Begin(const TextGeometryType &tpt=TextGeometryType::FixedStyle,int limit=2048);                   ///<创建一个文本绘制几何体

        void SetFixedStyle(const layout::CharStyle &);                                                                  ///<设定固定风格模式所用风格
        void SetParagraphStyle(const layout::ParagraphStyle *);                                                         ///<设定段落风格

        bool Layout(const layout::TEXT_COORD_VEC &start_pos,const U16StringView&);                                      ///<排版一段文本

        void End();                                                                                                     ///<结束排版

    public:

        TextGeometry *CreateGeometry(const TextGeometryType &tpt,const U16StringView&str);                              ///<创建一个文本几何体，并进行简单排版

        void Release(TextGeometry *);                                                                                   ///<释放一个文本几何体
    };//class TextRender
}//namespace hgl::graph
