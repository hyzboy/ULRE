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
        
        using TEXT_COORD_TYPE=int;                      //字符必须坐标对齐显示才能不模糊，所以这里坐标系全部使用整型坐标

        class TextLayout
        {
        protected:        

            struct CharLayoutAttributes
            {
                u32char ch;             ///<字符
                
                int size;               ///<字符排版尺寸(一般为宽)

                bool visible;           ///<是否可显示字符

                bool is_cjk;            ///<是否是中日韩文字
                bool is_emoji;          ///<是否是表情符号

                bool is_punctuation;    ///<是否是标点符号

                bool begin_disable;     ///<是否行首禁用符号
                bool end_disable;       ///<是否行尾禁用符号
                bool vrotate;           ///<竖排时是否需要旋转

                FontAdvInfo adv_info;   ///<字符绘制信息
            };//struct CharLayoutAttributes

            using CLA=CharLayoutAttributes;

        protected:

            RenderableCreater *rc;
            TextLayoutAttributes tla;

        protected:

            TextDirection direction;

            bool endless;
            float splite_line_max_limit;

            int max_chars;                              ///<总字符数量
            int draw_chars_count;                       ///<可绘制字符数量

            List<CLA> chars_attributes;

        protected:
        
            template<typename T> int preprocess(const BaseString<T> &origin_string);
            template<typename T> int plane_preprocess(const BaseString<T> &origin_string);
            
            bool h_splite_to_lines(float view_limit);
            bool v_splite_to_lines(float view_limit);

            
            int pl_h_l2r();
            int pl_h_r2l();
            int pl_v_r2l();
            int pl_v_l2r();

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

            AutoDelete<VB4f>    vertex,
                                tex_coord;

        public:

            TextLayout()
            {
                rc=nullptr;
                
                direction.text_direction=0;
                max_chars               =0;
                draw_chars_count        =0;

                vertex      =nullptr;
                tex_coord   =nullptr;
            }

            virtual ~TextLayout()=default;

            bool Set(RenderableCreater *_rc)            {if(_rc)rc=_rc;}
            bool Set(const TextLayoutAttributes *_tla)  {if(_tla)memcpy(&tla,_tla,sizeof(TextLayoutAttributes));}
            bool Set(FontSource *fs)                    {if(fs)tla.font_source=fs;}
            bool SetTextDirection(const uint8 &td)      {tla.text_direction=td;}
            bool Set(const TextAlign &ta)               {tla.align=ta;}
            bool SetMaxWidth(const float mw)            {tla.max_width=mw;}
            bool SetMaxHeight(const float mh)           {tla.max_height=mh;}

            virtual bool    Init        ();                                                       ///<初始化排版

//            virtual int     Layout      (const int max_chars,const BaseString<T> &)=0;              ///<排版

            template<typename T>
            int     PlaneLayout (const int max_chars,const BaseString<T> &)=0;                ///<简易排版
        };//class TextLayout
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_TEXT_LAYOUT_INCLUDE
