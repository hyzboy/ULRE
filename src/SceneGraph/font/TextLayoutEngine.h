#include<hgl/graph/font/TextLayout.h>
#include<hgl/graph/TileData.h>
#include<hgl/type/IndexedList.h>
#include<hgl/type/ConstStringSet.h>

namespace hgl::graph::layout
{
    class TextLayout
    {
    protected:

        TileFont *tile_font=nullptr;
        FontSource *font_source=nullptr;

    protected:

        int draw_chars_count=0;                     ///<最终要绘制字符列表(仅可见字符)

        U32CharSet chars_sets;                      ///<不重复字符统计缓冲区
        U32CharSet clear_chars_sets;                ///<待清除的字符合集
        TileUVFloatMap chars_uv;                    ///<所有要绘制字符的uv

        struct CharDrawAttr
        {
            const CLA *cla;
            TileUVFloat uv;
        };

        using CharDrawAttrIt=IndexedList<CharDrawAttr>::Iterator;

        IndexedList<CharDrawAttr> draw_chars_list;  ///<所有字符属性列表

        bool StatChars();  ///<统计所有字符

    protected:

        TextPrimitive *text_primitive=nullptr;

        DataArray<int16> vertex;
        DataArray<float> tex_coord;
        //DataArray<uint8> char_style;

    protected:

        ConstU16StringSet draw_all_strings;                 ///<所有绘制字符串合集

        struct DrawStringItem
        {
            ConstStringView<u16char> str;

            TextDrawStyle style;

            CharDrawAttrIt it;

            int16 *vertex;
            float *tex_coord;
        };

        ArrayList<DrawStringItem> draw_string_list;   ///<所有绘制字符串列表

    protected:

        int sl_l2r  (const DrawStringItem &);
        int sl_r2l  (const DrawStringItem &);
        int sl_v    (const DrawStringItem &);

    public:

        TextLayout(TileFont *);
        virtual ~TextLayout()=default;

    public: //多次排版

        bool Begin(TextPrimitive *,int Estimate=1024);                  ///<开始排版

        bool AddString(const U16StringView&,const TextDrawStyle &);     ///<添加一个要排版的字符串

        int  End();                                                     ///<结束排版

        void Clear()
        {
            text_primitive=nullptr;
            vertex.Clear();
            tex_coord.Clear();
            
            draw_chars_count=0;
            chars_sets.Clear();
            chars_uv.Clear();
            draw_chars_list.Clear();
            draw_all_strings.Clear();
            draw_string_list.Clear();
        }
    };//class TextLayout
}//namespace hgl::graph::layout

