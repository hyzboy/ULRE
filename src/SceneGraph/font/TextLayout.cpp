#include<hgl/graph/font/TextLayout.h>
#include<hgl/type/Extent.h>
#include<hgl/type/UnicodeBlocks.h>

namespace hgl
{
    namespace graph
    {
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
            char_height     =ceil(origin_char_height);
            space_size      =ceil(origin_char_height*tla.space_size);
            full_space_size =ceil(origin_char_height*tla.full_space_size);
            tab_size        =ceil(origin_char_height*tla.tab_size);
            char_gap        =ceil(origin_char_height*tla.char_gap);
            line_gap        =ceil(origin_char_height*tla.line_gap);
            line_height     =ceil(origin_char_height+line_gap);
            paragraph_gap   =ceil(origin_char_height*tla.paragraph_gap);

            return(true);
        }

        template<typename T> 
        int TextLayout::stat_chars(const T *str,const int str_length)
        {
            if(!str||!*str||str_length<=0)
                return 0;

            alone_chars.ClearData();

            for(int i=0;i<str_length;i++)
            {
                alone_chars.Add(*str);
                ++str;
            }

            return alone_chars.GetCount();
        }

        /**
         * 预处理所有的字符，获取所有字符的宽高，以及是否标点符号等信息
         */
        template<typename T>
        int TextLayout::preprocess(const T *str,const int str_length)
        {
            const int count=hgl_min(max_chars,str_length);

            if(count<=0)
                return 0;

            cla_list.ClearData();
            cla_list.SetCount(count);

            CLA **cla=cla.GetData();
            const T *cp=str;

            for(int i=0;i<count;i++)
            {
                *cla=tla->font_source.GetCLA(*cp);

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
            const int count=cla_list.GetCount();
            const CLA **cla=cla_list.GetData();

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
            const int count=cla_list.GetCount();
            const CLA **cla=cla_list.GetData();

            int cur_size=0;
            int left=0,top=0;

            float *tp=vertex->Begin();

            for(int i=0;i<count;i++)
            {
                *tp=left;   ++tp;
                *tp=top;    ++tp;
                *tp=left+(*cla)->adv_info.w;++tp;
                *tp=top +(*cla)->adv_info.h;++tp;
            }

            vertex->End();

            return 0;
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
            if(mc<=0||str.IsEmpty())
                return(-1);

            max_chars=hgl_min(mc,str.Length());

            if(stat_chars<T>(str.c_str(),str.Length())<=0)
                return(-2);

            if(preprocess<T>(str.c_str(),str.Length())<=0)
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
