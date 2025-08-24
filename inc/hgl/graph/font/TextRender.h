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

    class TextRender
    {
        VulkanDevice *      device;

        RenderResource *    db;

        Material *          material;

        Sampler *           sampler;

        Pipeline *          pipeline;

        TileFont *          tile_font;
        layout::TextLayout *tl_engine;

    private:

        struct CharStyleMaterial
        {
            layout::CharStyle char_style;
            MaterialInstance *mi;
        };

        CharStyleMaterial default_char_style_material;                          ///<默认字符风格材质

        Map<AnsiString,CharStyleMaterial> char_style_materials;                 ///<字符风格材质列表

        MaterialInstance *cur_char_style_mi=nullptr;                            ///<当前字符风格材质

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

        TextPrimitive *CreatePrimitive(int limit=2048);
        TextPrimitive *CreatePrimitive(const U16String &str);

        bool RegistryStyle(const AnsiString &,const layout::CharStyle &cds);
        void SetStyle(const AnsiString &style_name);

        void SetLayout(const layout::ParagraphStyle *tla);

        bool Layout(TextPrimitive *tr,const U16String &str);

        Mesh *CreateMesh(TextPrimitive *text_primitive);

        void Release(TextPrimitive *);
    };//class TextRender
}//namespace hgl::graph
