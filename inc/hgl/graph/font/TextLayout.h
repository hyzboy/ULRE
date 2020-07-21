#ifndef HGL_GRAPH_TEXT_LAYOUT_INCLUDE
#define HGL_GRAPH_TEXT_LAYOUT_INCLUDE

#include<hgl/type/StringList.h>
#include<hgl/graph/font/FontSource.h>
#include<hgl/graph/RenderableCreater.h>
namespace hgl
{
    namespace graph
    {
        /**
         * 字符属性，可精确到字也可精确到段落或是全文
         */
        struct CharAttributes
        {
            bool    bold        =false; ///<加粗
            bool    italic      =false; ///<右斜
            bool    underline   =false; ///<下划线

            Color4f CharColor;          ///<字符颜色
            Color4f BackgroundColor;    ///<背影颜色
        };//struct CharAttributes
       
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
                uint bottom_to_top:1;       ///<是否从下到上
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
        struct TextLayoutAttributes
        {
            FontSource *    font_source             =nullptr;                                       ///<字符源
            CharAttributes *char_attributes         =nullptr;                                       ///<缺省字符属性

            uint8           text_direction          =0;                                             ///<文本排列方向
            TextAlign       align                   =TextAlign::Left;                               ///<段落对齐
            float           char_gap                =0.0f;                                          ///<字间距
            float           line_gap                =0.1f;                                          ///<行间距(相对于字符高度)
            float           paragraph_gap           =1.0f;                                          ///<段间距(相对于字符高度)

            float           max_width               =0.0f;                                          ///<最大宽度(<=0代表无限制)
            float           max_height              =0.0f;                                          ///<最大高度(<=0代表无限制)

            bool            border_symbols_disable  =true;                                          ///<边界符号禁用(如行首禁用逗号)
//            bool            auto_symbols_convert    =true;                                          ///<自动符号转换(如tm/(r)/(c)等)

            float           space_size              =0.5f;                                          ///<半角空格尺寸(对应字符高度的系数)
            float           full_space_size         =1.0f;                                          ///<全角空格尺寸(对应字符高度的系数)
            float           tab_size                =4.0f;                                          ///<Tab符号尺寸(对应字符高度的系数)

            bool            compress_punctuation    =false;                                         ///<压缩标点符号
        };//struct TextLayoutAttributes

        int TextLayout(RenderableCreater *,const TextLayoutAttributes *,const int max_chars,const UTF16String &);

        int PlaneTextLayout(RenderableCreater *,FontSource *font_source,const int max_chars,const UTF16String &);
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_TEXT_LAYOUT_INCLUDE
