#pragma once

#include<hgl/graph/font/FontSource.h>
#include<hgl/color/Color4ub.h>

namespace hgl::graph
{
    class TileFont;
    class TextPrimitive;

    namespace layout
    {
        /**
        * 字符风格
        */
        struct CharStyle
        {
            Color4ub CharColor;          ///<字符颜色
            //Color4ub BackgroundColor;    ///<背景颜色

            //float   weight      =1.0f;  ///<粗细
            //float   italic      =0.0f;  ///<倾斜角度(<0左斜,>0右斜)

            //float   underline   =0.0f; ///<下划线粗细
            //float   strikeout   =0.0f; ///<删除线粗细
        };//struct CharStyle

        using CharDrawStyleID=uint8;

        constexpr const size_t CharDrawStyleBytes=sizeof(CharStyle);
       
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
        * 段落风格
        */
        struct ParagraphStyle
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
        };//struct ParagraphStyle
        
        using TEXT_COORD_TYPE=int;                      //字符必须坐标对齐显示才能不模糊，所以这里坐标系全部使用整型坐标

        struct TextDrawStyle:public ComparatorData<TextDrawStyle>
        {
            ParagraphStyle para_style;

            // CharDrawStyle与TextLayoutAttribute的区别在于：

            // CharStyle    是针对单个字符的绘制风格，其所有的属性都将储存于UBO，在Shader中使用。
            //                  所以每个CharDrawStyle其实都对应了一个TextRender的材质实例(MaterialInstance)。
            //                  也因此CharDrawStyle的数量是有限制的，因为所有的材质实例属性加起来不能超过一个UBO大小。
            //                  但其实这个值也还很大的，100来个还是可以支撑的。

            // ParagraphStyle 是针对整段文本的排版属性，其所有的值都在CPU阶段进行计算。所以其数量其实是无限制的。

            TEXT_COORD_TYPE char_height;
            TEXT_COORD_TYPE space_size;
            TEXT_COORD_TYPE full_space_size;
            TEXT_COORD_TYPE tab_size;
            TEXT_COORD_TYPE char_gap;
            TEXT_COORD_TYPE line_gap;
            TEXT_COORD_TYPE line_height;
        };
    }//namespace layout
}//namespace hgl::graph
