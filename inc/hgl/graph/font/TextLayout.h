#ifndef HGL_GRAPH_TEXT_LAYOUT_INCLUDE
#define HGL_GRAPH_TEXT_LAYOUT_INCLUDE

#include<hgl/type/StringList.h>
#include<hgl/graph/font/FontSource.h>
#include<hgl/graph/RenderableCreater.h>
#include<hgl/graph/TileData.h>
#include<hgl/type/MemBlock.h>
namespace hgl
{
    namespace graph
    {
        class TileFont;
        class TextRenderable;

        /**
         * 字符属性，可精确到字也可精确到段落或是全文
         */
        struct CharLayoutAttr
        {
            bool    bold        =false; ///<加粗
            bool    italic      =false; ///<右斜
            bool    underline   =false; ///<下划线

            Color4f CharColor;          ///<字符颜色
            Color4f BackgroundColor;    ///<背影颜色
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
        struct TextLayoutAttributes
        {
            FontSource *    font_source             =nullptr;                                       ///<字符源
            CharLayoutAttr *char_layout_attr        =nullptr;                                       ///<缺省字符排版属性

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
        
        using TEXT_COORD_TYPE=int;                      //字符必须坐标对齐显示才能不模糊，所以这里坐标系全部使用整型坐标

        class TextLayout
        {
        protected:

            FontSource *font_source;
            TextLayoutAttributes tla;

        protected:

            TextDirection direction;

            bool endless;
            float splite_line_max_limit;

            int draw_chars_count;                       ///<要绘制字符列表

            Set<u32char> alone_chars;                   ///<不重复字符统计缓冲区
            TileUVFloatMap alone_chars_uv;              ///<所有要绘制字符的uv

            struct CharDrawAttr
            {
                const CLA *cla;
                TileUVFloat uv;
            };
            
            ObjectList<CharDrawAttr> draw_chars_list; ///<所有字符属性列表

            template<typename T> bool preprocess(TileFont *,const T *,const int);

        protected:        

            bool h_splite_to_lines(float view_limit);
            bool v_splite_to_lines(float view_limit);
            
            int sl_h_l2r();
            int sl_h_r2l();
            int sl_v_r2l();
            int sl_v_l2r();

            template<typename T> int SimpleLayout(TextRenderable *,TileFont *,const String<T> &);                   ///<简易排版

//            template<typename T> int SimpleLayout(TileFont *,const StringList<String<T>> &);                      ///<简易排版

        protected:  

            TEXT_COORD_TYPE x,y;
            TEXT_COORD_TYPE char_height;
            TEXT_COORD_TYPE space_size;
            TEXT_COORD_TYPE full_space_size;
            TEXT_COORD_TYPE tab_size;
            TEXT_COORD_TYPE char_gap;
            TEXT_COORD_TYPE line_gap;
            TEXT_COORD_TYPE line_height;
            TEXT_COORD_TYPE paragraph_gap;

        protected:
        
            TextRenderable *text_render_obj;
            MemBlock<float> vertex;
            MemBlock<float> tex_coord;

        public:

            TextLayout()
            {
                direction.text_direction=0;
                draw_chars_count=0;

                text_render_obj =nullptr;
            }

            virtual ~TextLayout()=default;

            void SetTLA             (const TextLayoutAttributes *_tla)  {if(_tla)memcpy(&tla,_tla,sizeof(TextLayoutAttributes));}
            void SetFont            (FontSource *fs)                    {if(fs)font_source=fs;}
            void SetTextDirection   (const uint8 &td)                   {tla.text_direction=td;}
            void SetAlign           (const TextAlign &ta)               {tla.align=ta;}
            void SetMaxWidth        (const float mw)                    {tla.max_width=mw;}
            void SetMaxHeight       (const float mh)                    {tla.max_height=mh;}

            virtual bool    Init        ();                                                         ///<初始化排版

            int     SimpleLayout (TextRenderable *,TileFont *,const UTF16String &);                 ///<简易排版
            int     SimpleLayout (TextRenderable *,TileFont *,const UTF32String &);                 ///<简易排版

//            int     SimpleLayout (TileFont *,const UTF16StringList &);                            ///<简易排版
//            int     SimpleLayout (TileFont *,const UTF32StringList &);                            ///<简易排版
        };//class TextLayout
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_TEXT_LAYOUT_INCLUDE
