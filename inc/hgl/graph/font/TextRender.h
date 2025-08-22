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
    class TextLayout;
    class TextPrimitive;
    class RenderFramework;

    class TextRender
    {
        VulkanDevice *      device;

        RenderResource *    db;

        Material *          material;

        Sampler *           sampler;

        Pipeline *          pipeline;

        TileFont *          tile_font;
        TextLayout *        tl_engine;

    private:

        struct CharStyleMaterial
        {
            CharDrawStyle char_style;
            MaterialInstance *mi;
        };

        CharStyleMaterial default_char_style_material;                          ///<默认字符风格材质

        Map<AnsiString,CharStyleMaterial> char_style_materials;                 ///<字符风格材质列表

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

        bool RegistryStyle(const AnsiString &,const CharDrawStyle &cds);

        bool Layout(TextPrimitive *tr,const U16String &str);

        Mesh *CreateMesh(TextPrimitive *text_primitive);

        void Release(TextPrimitive *);
    };//class TextRender
}//namespace hgl::graph
