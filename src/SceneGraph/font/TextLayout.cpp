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

		    constexpr int       BeginSymbolsCount   =(sizeof(BeginSymbols)   /sizeof(u16char))-1;
		    constexpr int 		EndSymbolsCount	    =(sizeof(EndSymbols)     /sizeof(u16char))-1;
            constexpr int 		CurrencySymbolsCount=(sizeof(CurrencySymbols)/sizeof(u16char))-1;
            constexpr int       VRotateSymbolsCount =(sizeof(VRotateSymbols) /sizeof(u16char))-1;

            /**
             * 4舍5入处理
             */
            inline TEXT_COORD_TYPE out4in5(const double value)
            {
                return TEXT_COORD_TYPE(floor(value+0.5));
            }
        }//namespace

        bool TextLayout::Init()
        {
            if(!rc
                ||!tla.font_source
                ||!tla.char_attributes)
                return(false);

            direction.text_direction=tla.text_direction;

            if(direction.vertical)
            {
                endless                 =(tla.max_width<=0.0f);
                splite_line_max_limit   = tla.max_width;
            }
            else
            {
                endless                 =(tla.max_height<=0.0f);
                splite_line_max_limit   = tla.max_height;
            }

            const float origin_char_height=tla.font_source->GetCharHeight();

            x=y=0;
            char_height     =out4in5(origin_char_height);
            space_size      =out4in5(origin_char_height*tla.space_size);
            full_space_size =out4in5(origin_char_height*tla.full_space_size);
            tab_size        =out4in5(origin_char_height*tla.tab_size);
            char_gap        =out4in5(origin_char_height*tla.char_gap);
            line_gap        =out4in5(origin_char_height*tla.line_gap);
            line_height     =out4in5(origin_char_height+line_gap);
            paragraph_gap   =out4in5(origin_char_height*tla.paragraph_gap);

            return(true);
        }

        /**
         * 预处理所有的字符，获取所有字符的宽高，以及是否标点符号等信息
         */
        template<typename T>
        int TextLayout::preprocess(const BaseString<T> &origin_string)
        {
            const int count=hgl_min(max_chars,origin_string.Length());

            if(count<=0)
                return 0;

            chars_attributes.SetCount(count);
            draw_chars_count=0;

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
                    cla->visible=true;      cla->size=out4in5(tla.font_source->GetCharAdvWidth(*cp));

                    cla->begin_disable  =hgl::strchr(BeginSymbols,  *cp,BeginSymbolsCount       );
                    cla->end_disable    =hgl::strchr(EndSymbols,    *cp,EndSymbolsCount         );

                    if(!cla->end_disable)       //货币符号同样行尾禁用
                    cla->end_disable    =hgl::strchr(CurrencySymbols,*cp,CurrencySymbolsCount   );

                    cla->vrotate        =hgl::strchr(VRotateSymbols,*cp,VRotateSymbolsCount     );

                    cla->is_cjk         =isCJK(*cp);
                    cla->is_emoji       =isEmoji(*cp);

                    cla->is_punctuation =isPunctuation(*cp);
                        
                    if(!tla->font_source->GetCharMetrics(cla->adv_info,*cp))
                        hgl_zero(cla->adv_info);
                    else
                    if(cla->adv_info.w>0&&cla->adv_info.h>0)
                        ++draw_chars_count;
                }

                ++cp;
                ++cla;
            }
                     
            return count;
        }
                
        /**
         * 简易预处理所有的字符，获取所有字符的宽高，以及是否标点符号等信息
         */
        template<typename T>
        int TextLayout::plane_preprocess(const BaseString<T> &origin_string)
        {
            const int count=hgl_min(max_chars,origin_string.Length());

            if(count<=0)
                return 0;

            chars_attributes.SetCount(count);
            draw_chars_count=0;

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
                    cla->visible=true;      cla->size=out4in5(tla.font_source->GetCharAdvWidth(*cp));

                    cla->vrotate        =hgl::strchr(VRotateSymbols,*cp,VRotateSymbolsCount     );
                        
                    if(!tla->font_source->GetCharMetrics(cla->adv_info,*cp))
                        hgl_zero(cla->adv_info);
                    else
                    if(cla->adv_info.w>0&&cla->adv_info.h>0)
                        ++draw_chars_count;
                }

                ++cp;
                ++cla;
            }
                     
            return count;
        }

        /**
         * 将字符串断成多行，处理标点禁用，以及英文单词禁拆分
         */
        bool TextLayout::h_splite_to_lines(float view_limit)
        {
            const int count=chars_attributes.GetCount();
            const CLA *cla=chars_attributes.GetData();

            int cur_size=0;

            for(int i=0;i<count;i++)
            {
            }

            return(true);
        }

        bool TextLayout::v_splite_to_lines(float view_limit)
        {

            return(true);
        }

        //int TextLayout::Layout(const int mc,const BaseString<T> &str)
        //{
        //    if(mc<=0
        //        ||!str
        //        ||!(*str))
        //        return(-1);

        //    max_chars=mc;
        //    origin_string=str;

        //    if(preprocess()<=0)
        //        return(-3);
        //                
        //    if(!rc->Init(draw_chars_count))
        //        return(-4);

        //    vertex      =rc->CreateVADA<VB4f>(VAN::Vertex);
        //    tex_coord   =rc->CreateVADA<VB4f>(VAN::TexCoord);

        //    if(!vertex||!tex_coord)
        //        return(-5);

        //    if(direction.vertical)
        //    {
        //        if(!v_splite_to_lines(tla->max_height))
        //            return(-4);
        //    }
        //    else
        //    {
        //        if(!h_splite_to_lines(tla->max_width))
        //            return(-4);
        //    }

        //    return 0;
        //}

        int TextLayout::pl_h_l2r()
        {
            const int count=chars_attributes.GetCount();
            const CLA *cla=chars_attributes.GetData();

            int cur_size=0;
            int left=0,top=0;

            for(int i=0;i<count;i++)
            {
                vertex->Write(
            }
        }

        int TextLayout::pl_h_r2l(){return 0;}
        int TextLayout::pl_v_r2l(){return 0;}
        int TextLayout::pl_v_l2r(){return 0;}

        /**
         * 平面文本排版<br>
         * 不处理自动换行，仅支持\r\n换行。无任何特殊处理
         */
        template<typename T>
        int TextLayout::PlaneLayout(TileFont *tf,const int mc,const BaseString<T> &str)
        {
            if(mc<=0||str.IsEmpty()
                return(-1);

            max_chars=hgl_min(mc,str.Length());

            if(plane_preprocess<T>(str)<=0)
                return(-3);

            if(!rc->Init(draw_chars_count))
                return(-4);

            vertex      =rc->CreateVADA<VB4f>(VAN::Vertex);
            tex_coord   =rc->CreateVADA<VB4f>(VAN::TexCoord);

            if(!vertex||!tex_coord)
                return(-5);

            if(direction.vertical)
            {
                if(direction.right_to_left)
                    return pl_v_r2l();
                else
                    return pl_v_l2r();
            }
            else
            {
                if(direction.right_to_left)
                    return pl_h_r2l();
                else
                    return pl_h_l2r();
            }

            return 0;
        }
    }//namespace graph
}//namespace hgl
