#include<hgl/graph/font/TextLayout.h>
#include<hgl/graph/font/TextCharSSBO.h>
#include<hgl/graph/tile/TileData.h>
#include<hgl/type/IndexedList.h>
#include<hgl/type/ConstStringSet.h>
#include<vector>
#include<unordered_map>

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

        U32CharSet *atlas_chars_sets=nullptr;     ///<调用方持有的字符合集（排版结果回写，供字库图集淘汰）

    protected:  // GPU path data

        std::vector<layout::TextCharInfo>    char_info_table;        ///< unique char info table (for SSBO binding 14)
        UnorderedMap<u32char,uint16_t>       char_to_id;             ///< unicode → char_id mapping
        std::vector<layout::CharStyle>       char_styles;            ///< style table (for SSBO binding 15)

        uint16_t GetOrRegisterCharId(u32char ch,const CharMetricsInfo &metrics,const TileUVFloat &uv);

    protected:

        ConstU16StringSet draw_all_strings;                 ///<所有绘制字符串合集

        struct DrawStringItem
        {
            ConstStringView<u16char> str;

            TextDrawStyle style;

            CharDrawAttrIt it;

            bool operator==(const DrawStringItem& other) const
            {
                return str.GetString() == other.str.GetString()
                    && str.GetLength() == other.str.GetLength();
            }
        };

        ValueArray<DrawStringItem> draw_string_list;   ///<所有绘制字符串列表

    public:

        // GPU path: per-visible-character instance data (directly uses layout::CharInstance from TextCharSSBO.h)
        std::vector<layout::CharInstance> gpu_char_instances;

        const std::vector<layout::CharInstance>& GetGpuCharInstances() const { return gpu_char_instances; }
        const IndexedList<CharDrawAttr>& GetDrawCharsList() const { return draw_chars_list; }

        const std::vector<layout::TextCharInfo>&  GetCharInfoTable()  const { return char_info_table; }
        const std::vector<layout::CharStyle>&   GetCharStyles()     const { return char_styles; }
        uint16_t GetUniqueCharCount() const { return (uint16_t)char_info_table.size(); }

    private:
        int layout_width_ = 0;   ///< 排版后文本宽度（像素）
        int layout_height_ = 0;  ///< 排版后文本高度（像素）

    public:
        /// 获取排版后文本的宽度（像素）
        int GetLayoutWidth() const { return layout_width_; }
        /// 获取排版后文本的高度（像素）
        int GetLayoutHeight() const { return layout_height_; }

    protected:

        int sl_l2r  (const DrawStringItem &);
        int sl_r2l  (const DrawStringItem &);
        int sl_v    (const DrawStringItem &);

    public:

        TextLayout(TileFont *);
        virtual ~TextLayout()=default;

    public: //多次排版

        bool Begin(U32CharSet *,int Estimate=1024);                  ///<开始排版

        bool AddString(const U16StringView&,const TextDrawStyle &);     ///<添加一个要排版的字符串

        int  End();                                                     ///<结束排版

        void Clear()
        {
            atlas_chars_sets=nullptr;

            draw_chars_count=0;
            chars_sets.Clear();
            chars_uv.Clear();
            draw_chars_list.Clear();
            draw_all_strings.Clear();
            draw_string_list.Clear();
            gpu_char_instances.clear();

            char_info_table.clear();
            char_to_id.Clear();
            char_styles.clear();
        }
    };//class TextLayout
}//namespace hgl::graph::layout

