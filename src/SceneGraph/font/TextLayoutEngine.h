#include<hgl/graph/font/TextLayout.h>
#include<hgl/graph/TileData.h>
#include<hgl/type/IndexedList.h>
#include<hgl/type/ConstStringSet.h>

namespace hgl::graph::layout
{
    class TextLayout
    {
    protected:

        FontSource *font_source=nullptr;

    protected:

        int draw_chars_count=0;                     ///<要绘制字符列表

        U32CharSet chars_sets;                      ///<不重复字符统计缓冲区
        U32CharSet clear_chars_sets;                ///<待清除的字符合集
        TileUVFloatMap chars_uv;                    ///<所有要绘制字符的uv

        struct CharDrawAttr
        {
            const CLA *cla;
            TileUVFloat uv;
        };

        IndexedList<CharDrawAttr> draw_chars_list; ///<所有字符属性列表

        bool StatChars(TextPrimitive *,TileFont *,const u16char *,const int);                    ///<统计所有字符

    protected:

        int sl_l2r(const TextDrawStyle &);
        int sl_r2l(const TextDrawStyle &);
        int sl_v(const TextDrawStyle &);

    protected:

        TextPrimitive *text_primitive=nullptr;
        DataArray<int16> vertex;
        DataArray<float> tex_coord;
        DataArray<uint8> char_style;

    protected:

        

    public:

        TextLayout(FontSource *fs){font_source=fs;}
        virtual ~TextLayout()=default;

    public: //多次排版

        bool Begin(TextPrimitive *,TileFont *,int Estimate=1024);       ///<开始排版

        //bool PrepareVBO();

        void End();                                                     ///<结束排版

    public: //单次排版

        int     SimpleLayout(TextPrimitive *,TileFont *,const U16String &,const TextDrawStyle &);                 ///<简易排版
    };//class TextLayout
}//namespace hgl::graph::layout

