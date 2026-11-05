#include<hgl/graph/font/TextLayout.h>
#include<hgl/graph/font/TextCharSSBO.h>
#include<hgl/graph/tile/TileData.h>
#include<hgl/type/IndexedList.h>
#include<hgl/type/ConstStringSet.h>
#include<vector>
#include<unordered_map>

namespace hgl::graph::layout
{
    // 单字符排版属性（命名空间级：sl_* 排版函数与禁断决策共用）
    struct CharDrawAttr
    {
        const CLA *cla;
        TileUVFloat uv;
    };

    using CharDrawAttrIt=IndexedList<CharDrawAttr>::Iterator;

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

        IndexedList<CharDrawAttr> draw_chars_list;  ///<所有字符属性列表

        bool StatChars();  ///<统计所有字符

    protected:

        U32CharSet *atlas_chars_sets=nullptr;     ///<调用方持有的字符合集（排版结果回写，供字库图集淘汰）

    protected:  // GPU path data

        std::vector<layout::TextCharInfo>    char_info_table;        ///< unique char info table (for SSBO binding 14)
        UnorderedMap<uint64,uint16_t>       char_to_id;             ///< (variant<<32|unicode) → char_id mapping；variant 区分同一字符的不同排版变体（如竖排居中标点修正版）
        std::vector<layout::CharStyle>       char_styles;            ///< style table (for SSBO binding 15)

        uint16_t GetOrRegisterCharId(u32char ch,const CharMetricsInfo &metrics,const TileUVFloat &uv,uint16_t variant = 0);

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

    public:

        /**
        * 命中测试数据(水平排版专用)
        *
        * 每个源字符(含空格等不可见字符)记录其在最近一次排版中的笔位与推进量，
        * 供 GUI 做点击定位与光标绘制。竖排(Vertical)不填充本表。
        */
        struct HitChar
        {
            int32_t pen_x       = 0;    ///<笔位 X(与 CharInstance::pen_x 同一坐标空间)
            int32_t pen_y       = 0;    ///<行顶 Y
            int32_t advance     = 0;    ///<横向推进量(像素)
            int32_t src_index   = 0;    ///<源字符串字符下标(UTF-16)
            int32_t line_index  = 0;    ///<所属行号
        };//struct HitChar

    protected:

        ValueArray<HitChar> hit_chars;

        int32_t hit_origin_x    = 0;        ///<最近一次排版的起始 X
        int32_t hit_origin_y    = 0;        ///<最近一次排版的起始 Y
        int32_t hit_line_step   = 0;        ///<行步进(像素)
        int32_t hit_char_height = 0;        ///<字符高度(像素，光标高度)
        int32_t hit_line_count  = 0;        ///<行数
        int32_t hit_text_length = 0;        ///<最近一次排版的源文本长度

    public:

        /**
        * 坐标反查字符下标(水平排版)
        * @param local_pos 与 start_position 同坐标空间的像素坐标
        * @return 源字符下标；命中行尾返回该行末字符下标+1；无有效排版返回 -1
        */
        int GetIndexAtPosition(const TEXT_COORD_VEC &local_pos) const;

        /**
        * 字符下标反查光标位置(水平排版)
        * @param src_index 源字符下标(可取 [0, 文本长度] 区间，末尾表示光标停在文末)
        * @param out_pos   输出光标笔位(与 start_position 同坐标空间)
        * @param out_height 输出光标高度(像素)
        * @return 是否有有效排版数据
        */
        bool GetCursorPosition(const int src_index, TEXT_COORD_VEC &out_pos, int &out_height) const;

        /// 文末光标位置
        bool GetEndCursorPosition(TEXT_COORD_VEC &out_pos, int &out_height) const;

        /// 最近一次排版的文本长度(供光标下标上限使用)
        int GetHitTextLength() const { return hit_text_length; }

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

            hit_chars.Clear();
            hit_origin_x    =0;
            hit_origin_y    =0;
            hit_line_step   =0;
            hit_char_height =0;
            hit_line_count  =0;
            hit_text_length =0;
        }
    };//class TextLayout
}//namespace hgl::graph::layout

