#pragma once

#include<hgl/graph/VK.h>
#include<hgl/color/Color4f.h>
#include<hgl/type/SortedSet.h>

namespace hgl::graph
{
    class FontDataSource;
    class FontSourceMulti;
    class TileFont;
    class TextLayout;
    class TextPrimitive;
    class RenderFramework;

    class TextRender
    {
        VulkanDevice *      device;

        RenderResource *    db;

        Material *          material;
        MaterialInstance *  material_instance;

        Sampler *           sampler;

        Pipeline *          pipeline;

        FontSourceMulti *   font_source;

        TileFont *          tile_font;
        TextLayout *        tl_engine;
    
        Color4f             color;

        SortedSet<TextPrimitive *> tr_sets;

    private:

        friend class RenderFramework;

        TextRender(VulkanDevice *,TileFont *,FontSourceMulti *);

        bool InitTextLayoutEngine();
        bool InitMaterial(RenderPass *);
        bool Init(RenderPass *rp);

    public:

        ~TextRender();

    public:

        TextPrimitive *CreatePrimitive(int limit=2048);
        TextPrimitive *CreatePrimitive(const U16String &str);

        bool Layout(TextPrimitive *tr,const U16String &str);

        Mesh *CreateMesh(TextPrimitive *text_primitive);

        void Release(TextPrimitive *);
    };//class TextRender
}//namespace hgl::graph
