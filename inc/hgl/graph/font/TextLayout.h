#ifndef HGL_GRAPH_TEXT_LAYOUT_INCLUDE
#define HGL_GRAPH_TEXT_LAYOUT_INCLUDE

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
         * 段落对齐
         */
        enum class ParagraphAlign
        {
            Left=0,                     ///<左对齐
            Right,                      ///<右对齐
            Center,                     ///<居中

            Top     =Left,              ///<上对齐
            Bottom  =Right              ///<下对齐
        };//enum class ParagraphAlign

        /**
         * 段落属性
         */
        struct ParagraphAttributes
        {
            ParagraphAlign  align                   =ParagraphAlign::Left;      ///<段落对齐
            float           char_gap                =0.0f;                      ///<字间距
            float           line_gap                =0.1f;                      ///<行间距(相对于字符高度)

            uint32          char_attributes         =0;                         ///<缺省字符属性

            bool            border_symbols_disable  =true;                      ///<边界符号禁用(如行首禁用逗号)
            bool            auto_symbols_convert    =true;                      ///<自动符号转换(如tm/(r)/(c)等)

            float           space_size              =0.5f;                      ///<空格符尺寸(对应字符高度的系数)
            float           tab_size                =2.0f;                      ///<Tab符号尺寸(对应字符高度的系数)

            bool            auto_spaces_size        =false;                     ///<自动空白字符尺寸(用于将每行/列文本长度拉至一样)
        };//struct ParagraphAttributes

        /**
         * 文本排列方向
         */
        enum class TextDirection
        {
            HorizontalLeftToRight=0,    ///<横排从左到右
            HorizontalRightToLeft,      ///<横排从右到左
            VerticalRightToLeft,        ///<坚排从上到下从右到左
        };//enum class Direction

        struct TextAttributes
        {
            TextDirection   direction               =TextDirection::HorizontalLeftToRight;      ///<文本排列方向            
            uint32_t        paragraph_attribute     =0;                                         ///<缺省段落属性
            float           paragraph_gap           =1.0f;                                      ///<段间距
        };//struct TextAttributes

        using CharAttributesList=Map<UTF16String,CharAttributes>;
        using ParagraphAttributesList=Map<UTF16String,ParagraphAttributes>;

        /**
         * 文本排版处理配置
         */
        struct TextLayoutAttributes
        {
            FontSource *            source              =nullptr;   ///字符源

            CharAttributesList      char_attributes;                ///<文本属性
            ParagraphAttributesList paragraph_attributes;           ///<段落属性
    
            float                   max_width           =0.0f;      ///<最大宽度(<=0代表无限制)
            float                   max_height          =0.0f;      ///<最大高度(<=0代表无限制)

            TextAttributes          attributes;                     ///<文本属性
        };//struct TextLayoutAttributes

        uint TextLayout(vulkan::Renderable *,const uint max_chars,const UTF16String &);
        uint TextLayout(vulkan::Renderable *,const uint max_chars,const UTF16StringList &);

        uint TextLayout(vulkan::Renderable *,const uint max_chars,const UTF32String &);
        uint TextLayout(vulkan::Renderable *,const uint max_chars,const UTF32StringList &);
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_TEXT_LAYOUT_INCLUDE
