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
    class RenderFormat;
    class TileFont;
    class TextGeometry;
    class MaterialManager;
    class PrimitiveManager;

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

        PrimitiveManager *  primitive_manager;
        MaterialManager *   mtl_manager;

        Sampler *           sampler;

        GraphicsPipeline *  pipeline;

        TileFont *          tile_font;
        layout::TextLayout *tl_engine;

        layout::ParagraphStyle para_style;                  ///<段落风格
        layout::TextDrawStyle text_draw_style;              ///<文本绘制风格

    private:    //fixed style 资源

        layout::CharStyle   fixed_style;                    ///<固定字符风格

        Material *          mtl_fs;                         ///<固定风格材质
        MaterialInstance *  mi_fs;                          ///<固定风格材质实例

    private:

        OrderedSet<TextGeometry *> text_geometry_set;        ///<所有的文字绘制几何体

    private:

        bool SimpleLayout(TextGeometry *tr,const U16StringView &str);              ///<简单文本排版

    private:

        friend class AppFramework;

        TextRender(GraphicsContext *,TileFont *);
        TextRender(AppFramework *,TileFont *);

        bool InitTextLayoutEngine();
        bool InitMaterial(RenderFormat *);
        bool Init(RenderFormat *,Sampler *);

    public:

        ~TextRender();

    public:

        static TextRender *CreateWithGraphicsContext(GraphicsContext *gc,RenderFormat *rp,FontSource *fs,int limit=1024,const VkExtent2D *extent=nullptr);

        TextGeometry *Begin(const TextGeometryType &tpt=TextGeometryType::FixedStyle,int limit=2048);                   ///<创建一个文本绘制几何体

        void SetFixedStyle(const layout::CharStyle &);                                                                  ///<设定固定风格模式所用风格
        void SetParagraphStyle(const layout::ParagraphStyle *);                                                         ///<设定段落风格

        bool Layout(const layout::TEXT_COORD_VEC &start_pos,const U16StringView&);                                      ///<排版一段文本

        void End();                                                                                                     ///<结束排版

    public:

        TextGeometry *CreateGeometry(const TextGeometryType &tpt,const U16StringView&str);                              ///<创建一个文本几何体，并进行简单排版

        Primitive *CreatePrimitive(TextGeometry *text_geometry);                                                        ///<创建一个网格对象用于渲染指定的文本几何体

        void Release(TextGeometry *);                                                                                   ///<释放一个文本几何体
    };//class TextRender
}//namespace hgl::graph
