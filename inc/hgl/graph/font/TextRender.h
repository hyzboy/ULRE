#pragma once

#include<hgl/graph/VK.h>
#include<hgl/color/Color4f.h>
#include<hgl/type/SortedSet.h>

namespace hgl::graph
{
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
        MaterialInstance *  material_instance;

        Sampler *           sampler;

        Pipeline *          pipeline;

        FontSource *        font_source;

        TileFont *          tile_font;
        TextLayout *        tl_engine;
    
        Color4f             color;

        SortedSet<TextPrimitive *> tr_sets;

    private:

        friend class RenderFramework;

        TextRender(VulkanDevice *,TileFont *,FontSource *);

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
}//namespace hgl::graph
