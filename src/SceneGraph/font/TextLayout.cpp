#include<hgl/graph/font/TextLayout.h>
#include<hgl/type/Extent.h>
#include<hgl/type/UnicodeBlocks.h>

namespace hgl
{
    namespace graph
    {
        namespace
        {        
		    constexpr u16char   BeginSymbols    []=U16_TEXT("!),❟.:;?]}¨·ˇˉ―‖’❜”„❞…∶、。〃々❯〉》」』】〕〗！＂＇），．：；？］｀｜｝～»›");     //行首禁用符号
		    constexpr u16char 	EndSymbols      []=U16_TEXT("([{·❛‘“‟❝❮〈《「『【〔〖（．［｛«‹");                                           //行尾禁用符号
            constexpr u16char 	CurrencySymbols []=U16_TEXT("₳฿₿￠₡¢₢₵₫€￡£₤₣ƒ₲₭Ł₥₦₽₱＄$₮ℳ₶₩￦¥￥₴₸¤₰៛₪₯₠₧﷼㍐원৳₹₨৲௹");                //货币符号
            constexpr u16char   VRotateSymbols  []=U16_TEXT("()[]{}〈〉《》「」『』【】〔〕〖〗（）［］｛｝―‖…∶｜～");                        //竖排必须旋转的符号

		    constexpr int       BeginSymbolsCount   =sizeof(BeginSymbols)   /sizeof(u16char);
		    constexpr int 		EndSymbolsCount	    =sizeof(EndSymbols)     /sizeof(u16char);
            constexpr int 		CurrencySymbolsCount=sizeof(CurrencySymbols)/sizeof(u16char);
            constexpr int       VRotateSymbolsCount =sizeof(VRotateSymbols) /sizeof(u16char);

            using TEXT_COORD_TYPE=int;                      //字符必须坐标对齐显示才能不模糊，所以这里坐标系全部使用整型坐标

            template<typename T>
            struct CharLayoutAttributes
            {
                T ch;                   ///<字符
                
                int size;               ///<字符排版尺寸(一般为宽)

                bool visible;           ///<是否可显示字符

                bool is_cjk;            ///<是否是中日韩文字
                bool is_emoji;          ///<是否是表情符号

                bool is_punctuation;    ///<是否是标点符号

                bool begin_disable;     ///<是否行首禁用符号
                bool end_disable;       ///<是否行尾禁用符号
                bool vrotate;           ///<竖排时是否需要旋转
            };//struct CharLayoutAttributes

            /**
             * 4舍5入处理
             */
            inline TEXT_COORD_TYPE out4in5(const double value)
            {
                return TEXT_COORD_TYPE(floor(value+0.5));
            }

            template<typename T> class TextLayoutEngine
            {
            protected:

                RenderableCreater *rc;
                const TextLayoutAttributes *tla;

                TextDirection direction;

                bool endless;
                float splite_line_max_limit;

                int max_chars;

                BaseString<T> origin_string;

                using CLA=CharLayoutAttributes<T>;

                List<CLA> chars_attributes;

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

            public:

                virtual ~TextLayoutEngine()=default;
                
                bool Init(RenderableCreater *_rc,const TextLayoutAttributes *_tla)
                {                    
                    if(!_rc
                     ||!_tla
                     ||!tla->font_source
                     ||!tla->char_attributes)
                        return false;

                    rc=_rc;
                    tla=_tla;

                    direction.text_direction=tla->text_direction;

                    if(direction.vertical)
                    {
                        endless                 =(tla->max_width<=0.0f);
                        splite_line_max_limit   =tla->max_width;
                    }
                    else
                    {
                        endless                 =(tla->max_height<=0.0f);
                        splite_line_max_limit   =tla->max_height;
                    }

                    const float origin_char_height=tla->font_source->GetCharHeight();

                    x=y=0;
                    char_height     =out4in5(origin_char_height);
                    space_size      =out4in5(origin_char_height*tla->space_size);
                    full_space_size =out4in5(origin_char_height*tla->full_space_size);
                    tab_size        =out4in5(origin_char_height*tla->tab_size);
                    char_gap        =out4in5(origin_char_height*tla->char_gap);
                    line_gap        =out4in5(origin_char_height*tla->line_gap);
                    line_height     =out4in5(origin_char_height+line_gap);
                    paragraph_gap   =out4in5(origin_char_height*tla->paragraph_gap);

                    return(true);
                }

            protected:

                /**
                 * 预处理所有的字符，获取所有字符的宽高，以及是否标点符号等信息
                 */
                int preprocess()
                {
                    const int count=hgl_min(max_chars,origin_string.Length());

                    if(count<=0)
                        return 0;

                    chars_attributes.SetCount(count);

                    CLA *cla=chars_attributes.GetData();
                    const T *cp=origin_string.c_str();

                    for(int i=0;i<count;i++)
                    {
                        cla->ch=*cp;

                        if(hgl::isspace(*cp))
                        {
                            cla->visible=false;

                            if(*cp=='\t')           cla->size=tab_size;         else
                            if(*cp==HGL_FULL_SPACE) cla->size=full_space_size;  else
                                                    cla->size=space_size;
                        }
                        else
                        {
                            cla->visible=true;      cla->size=out4in5(tla->font_source->GetCharAdvWidth(*cp));

                            cla->begin_disable  =hgl::strchr(BeginSymbols,  *cp,BeginSymbolsCount       );
                            cla->end_disable    =hgl::strchr(EndSymbols,    *cp,EndSymbolsCount         );

                            if(!cla->end_disable)       //货币符号同样行尾禁用
                            cla->end_disable    =hgl::strchr(CurrencySymbols,*cp,CurrencySymbolsCount   );

                            cla->vrotate        =hgl::strchr(VRotateSymbols,*cp,VRotateSymbolsCount     );

                            cla->is_cjk         =isCJK(*cp);
                            cla->is_emoji       =isEmoji(*cp);

                            cla->is_punctuation =isPunctuation(*cp);
                        }

                        ++cp;
                        ++cla;
                    }
                     
                    return count;
                }

                /**
                 * 将字符串断成多行，处理标点禁用，以及英文单词禁拆分
                 */
                bool h_splite_to_lines(float view_limit)
                {
                    const int count=chars_attributes.GetCount();
                    const CLA *cla=chars_attributes.GetData();

                    int cur_size=0;

                    for(int i=0;i<count;i++)
                    {
                    }

                    return(true);
                }

                bool v_splite_to_lines(float view_limit)
                {

                    return(true);
                }

            public:

                int layout(const int mc,const BaseString<T> &str)
                {
                    if(mc<=0
                     ||!str
                     ||!(*str))
                        return(-1);

                    max_chars=mc;
                    origin_string=str;

                    if(preprocess()<=0)
                        return(-3);

                    if(direction.vertical)
                    {
                        if(!v_splite_to_lines(tla->max_height))
                            return(-4);
                    }
                    else
                    {
                        if(!h_splite_to_lines(tla->max_width))
                            return(-4);
                    }

                    return 0;
                }
            };//template<typename T> class TextLayoutEngine
        }//namespace

        int TextLayout(RenderableCreater *rc,const TextLayoutAttributes *tla,const int max_chars,const UTF16String &str)
        {
            TextLayoutEngine<u16char> engine;

            if(!engine.Init(rc,tla))
                return(-1);

            return engine.layout(max_chars,str);
        }
    }//namespace graph
}//namespace hgl
