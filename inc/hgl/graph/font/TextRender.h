#pragma once

#include<hgl/graph/VK.h>
#include<hgl/graph/font/TextLayout.h>
#include<hgl/type/Map.h>
#include<hgl/type/SortedSet.h>
#include<hgl/type/String.h>

namespace hgl::graph
{
    class FontDataSource;
    class FontSource;
    class TileFont;
    class TextPrimitive;
    class RenderFramework;

    namespace layout
    {
        class TextLayout;
    }//namespace layout

    enum class TextPrimitiveType:uint8
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

        RenderResource *    db;

        Material *          material;
        MaterialInstance *  material_instance;

        Sampler *           sampler;

        Pipeline *          pipeline;

        TileFont *          tile_font;
        layout::TextLayout *tl_engine;

    private:

        layout::CharStyle fixed_style;    ///<固定字符风格

    private:

        SortedSet<TextPrimitive *> tr_sets;         ///<所有的文字绘制几何体

    private:

        friend class RenderFramework;

        TextRender(VulkanDevice *,TileFont *);

        bool InitTextLayoutEngine();
        bool InitMaterial(RenderPass *);
        bool Init(RenderPass *rp);

    public:

        ~TextRender();

    public:

        TextPrimitive *CreatePrimitive(const TextPrimitiveType &tpt=TextPrimitiveType::FixedStyle,int limit=2048);
        TextPrimitive *CreatePrimitive(const TextPrimitiveType &tpt,const U16String &str);

        void SetFixedStyle(const layout::CharStyle &);

        void SetLayout(const layout::ParagraphStyle *tla);

        bool Layout(TextPrimitive *tr,const U16String &str);

        Mesh *CreateMesh(TextPrimitive *text_primitive);

        void Release(TextPrimitive *);
    };//class TextRender
}//namespace hgl::graph
