#pragma once

#include<hgl/type/StringList.h>
#include<hgl/graph/font/FontSource.h>
#include<hgl/graph/PrimitiveCreater.h>
#include<hgl/graph/TileData.h>
#include<hgl/type/DataArray.h>
#include<hgl/type/IndexedList.h>

namespace hgl::graph
{
    class TileFont;
    class TextPrimitive;

    /**
    * 字符属性，可精确到字也可精确到段落或是全文
    */
    struct CharLayoutAttr
    {
        bool    bold        =false; ///<加粗
        bool    italic      =false; ///<右斜
        bool    underline   =false; ///<下划线
        bool    strikeout   =false; ///<删除线

        Color4f CharColor;          ///<字符颜色
        Color4f BackgroundColor;    ///<背景颜色
    };//struct CharLayoutAttr
       
    /**
    * 文本排列方向
    */
    union TextDirection
    {
        uint8 text_direction;

        struct
        {
            uint vertical:1;            ///<是否竖排
            uint right_to_left:1;       ///<是否从右往左
        };
    };//union TextDirection

    /**
    * 文本对齐
    */
    enum class TextAlign
    {
        Left=0,                     ///<左对齐
        Right,                      ///<右对齐
        Center,                     ///<居中
        Fill,                       ///<排满整行/列

        Top     =Left,              ///<上对齐
        Bottom  =Right              ///<下对齐
    };//enum class TextAlign

    /**
    * 文本排版属性
    */
    struct TextLayoutAttribute
    {
        CharLayoutAttr *char_layout_attr        =nullptr;                                       ///<缺省字符排版属性

        uint8           text_direction          =0;                                             ///<文本排列方向
        TextAlign       align                   =TextAlign::Left;                               ///<段落对齐
        float           char_gap                =0.0f;                                          ///<字间距
        float           line_gap                =0.1f;                                          ///<行间距(相对于字符高度)

        float           max_width               =0.0f;                                          ///<最大宽度(<=0代表无限制)
        float           max_height              =0.0f;                                          ///<最大高度(<=0代表无限制)

        bool            border_symbols_disable  =true;                                          ///<边界符号禁用(如行首禁用逗号)
//      bool            disable_break_alpha_word=true;                                          ///<禁用断字(如英文单词不允许断开)
//      bool            auto_symbols_convert    =true;                                          ///<自动符号转换(如tm/(r)/(c)等)

        float           space_size              =0.5f;                                          ///<半角空格尺寸(对应字符高度的系数)
        float           full_space_size         =1.0f;                                          ///<全角空格尺寸(对应字符高度的系数)
        float           tab_size                =4.0f;                                          ///<Tab符号尺寸(对应字符高度的系数)

        bool            compress_punctuation    =false;                                         ///<压缩标点符号
    };//struct TextLayoutAttribute

    using TLA=TextLayoutAttribute;
        
    using TEXT_COORD_TYPE=int;                      //字符必须坐标对齐显示才能不模糊，所以这里坐标系全部使用整型坐标

    class TextLayout
    {
    protected:

        FontSource *font_source=nullptr;
        TextLayoutAttribute tla{};

    protected:

        TextDirection direction{};

        float splite_line_max_limit=0;

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

        template<typename T> bool StatChars(TextPrimitive *,TileFont *,const T *,const int);                    ///<统计所有字符

    protected:        

        bool h_splite_to_lines(float view_limit);
        bool v_splite_to_lines(float view_limit);
            
        int sl_h_l2r();
        int sl_h_r2l();
        int sl_v_r2l();
        int sl_v_l2r();

        template<typename T> int SimpleLayout(TextPrimitive *,TileFont *,const String<T> &);                   ///<简易排版

    protected:  

        TEXT_COORD_TYPE char_height;
        TEXT_COORD_TYPE space_size;
        TEXT_COORD_TYPE full_space_size;
        TEXT_COORD_TYPE tab_size;
        TEXT_COORD_TYPE char_gap;
        TEXT_COORD_TYPE line_gap;
        TEXT_COORD_TYPE line_height;

    protected:
        
        TextPrimitive *text_primitive=nullptr;
        DataArray<int16> vertex;
        DataArray<float> tex_coord;

    public:

        TextLayout(FontSource *fs){font_source=fs;}
        virtual ~TextLayout()=default;

        void SetTLA             (const TLA *        _tla)   {if(_tla)hgl_cpy(&tla,_tla,1);}
        void SetTextDirection   (const uint8 &      td)     {tla.text_direction=td;}
        void SetAlign           (const TextAlign &  ta)     {tla.align=ta;}
        void SetMaxWidth        (const float        mw)     {tla.max_width=mw;}
        void SetMaxHeight       (const float        mh)     {tla.max_height=mh;}

        virtual bool    Init        ();                                                             ///<初始化排版

    public: //多次排版

        bool Begin(TextPrimitive *,TileFont *,int Estimate=1024);       ///<开始排版

        //bool PrepareVBO();

        void End();                                                     ///<结束排版

    public: //单次排版

                int     SimpleLayout(TextPrimitive *,TileFont *,const U16String &);                 ///<简易排版
                int     SimpleLayout(TextPrimitive *,TileFont *,const U32String &);                 ///<简易排版

//            int     SimpleLayout (TileFont *,const UTF16StringList &);                            ///<简易排版
//            int     SimpleLayout (TileFont *,const UTF32StringList &);                            ///<简易排版
    };//class TextLayout
}//namespace hgl::graph
