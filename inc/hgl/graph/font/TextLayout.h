#pragma once

#include<hgl/graph/font/FontSource.h>
#include<hgl/color/Color4f.h>

namespace hgl::graph
{
    class TileFont;
    class TextPrimitive;

    /**
    * 字符绘制风格
    */
    struct CharDrawStyle
    {
        float   weight      =1.0f;  ///<粗细
        float   italic      =0.0f;  ///<倾斜角度(<0左斜,>0右斜)

        float   underline   =0.0f; ///<下划线粗细
        float   strikeout   =0.0f; ///<删除线粗细

        Color4f CharColor;          ///<字符颜色
        Color4f BackgroundColor;    ///<背景颜色
    };//struct CharDrawStyle
       
    /**
    * 文本排列方向
    */
    enum class TextDirection : uint8
    {
        LeftToRight,    //<横排从左到右
        RightToLeft,    //<横排从右到左
        Vertical,       //<竖排从上到下，从右到左

        //有竖排从下到上的吗？从左到右？
    };//enum class TextDirection

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
    * 段落排版属性
    */
    struct TextLayoutAttribute
    {
        TextDirection   text_direction          =TextDirection::LeftToRight;                    ///<文本排列方向
        TextAlign       align                   =TextAlign::Left;                               ///<段落对齐
        float           char_gap                =0.0f;                                          ///<字间距
        float           line_gap                =0.1f;                                          ///<行间距(相对于字符高度)

        float           max_width               =0.0f;                                          ///<最大宽度(<=0代表无限制)
        float           max_height              =0.0f;                                          ///<最大高度(<=0代表无限制)

        bool            disable_border_symbols  =true;                                          ///<禁用边界符号(如行首禁用逗号)
//      bool            disable_break_word      =true;                                          ///<禁用断字(如英文单词不允许断开)
//      bool            auto_symbols_convert    =true;                                          ///<自动符号转换(如tm/(r)/(c)等)

        float           space_size              =0.5f;                                          ///<半角空格尺寸(对应字符高度的系数)
        float           full_space_size         =1.0f;                                          ///<全角空格尺寸(对应字符高度的系数)
        float           tab_size                =4.0f;                                          ///<Tab符号尺寸(对应字符高度的系数)

        bool            compress_punctuation    =false;                                         ///<压缩标点符号
    };//struct TextLayoutAttribute

    using TLA=TextLayoutAttribute;
        
    using TEXT_COORD_TYPE=int;                      //字符必须坐标对齐显示才能不模糊，所以这里坐标系全部使用整型坐标

    struct TextDrawStyle:public ComparatorData<TextDrawStyle>
    {
        CharDrawStyle char_draw_style;
        TextLayoutAttribute layout_attr;

        TEXT_COORD_TYPE char_height;
        TEXT_COORD_TYPE space_size;
        TEXT_COORD_TYPE full_space_size;
        TEXT_COORD_TYPE tab_size;
        TEXT_COORD_TYPE char_gap;
        TEXT_COORD_TYPE line_gap;
        TEXT_COORD_TYPE line_height;
    };
}//namespace hgl::graph
