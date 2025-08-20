#include"TextLayoutEngine.h"
#include<hgl/graph/font/TileFont.h>
#include<hgl/graph/font/TextPrimitive.h>
#include<hgl/type/Extent.h>

namespace hgl::graph
{
    void TextLayout::Set(const CharLayoutAttribute *c,const TextLayoutAttribute *t)
    {
        if(c)
            hgl_cpy<CharLayoutAttribute>(tda.cla,*c);

        if(t)
        {
            const float origin_char_height=font_source->GetCharHeight();

            hgl_cpy<TextLayoutAttribute>(tda.tla,*t);

            tda.char_height     =std::ceil(origin_char_height);
            tda.space_size      =std::ceil(origin_char_height*tda.space_size);
            tda.full_space_size =std::ceil(origin_char_height*tda.full_space_size);
            tda.tab_size        =std::ceil(origin_char_height*tda.tab_size);
            tda.char_gap        =std::ceil(origin_char_height*tda.char_gap);
            tda.line_gap        =std::ceil(origin_char_height*tda.line_gap);
            tda.line_height     =std::ceil(origin_char_height+tda.line_gap);
        }
    }

    bool TextLayout::Begin(TextPrimitive *tr,TileFont *tf,int Estimate)
    {
        if(!tr||!tf||Estimate<=0)
            return(false);

        draw_chars_count=0;
        chars_sets.Clear();
        chars_sets.PreAlloc(Estimate);
        draw_chars_list.Clear();
        draw_chars_list.PreAlloc(Estimate);

        text_primitive=tr;

        return(true);
    }

    /**
    * 预处理所有的字符，获取所有字符的宽高，以及是否标点符号等信息
    */
    template<typename T> 
    bool TextLayout::StatChars(TextPrimitive *tr,TileFont *tile_font,const T *str,const int str_length)
    {
        if(!tr
            ||!tile_font
            ||!str||!*str||str_length<=0
            ||!font_source)
            return(false);

        //遍历所有字符，取得每一个字符的基本绘制信息
        {
            const T *cp=str;
            CharDrawAttr cda;

            for(int i=0;i<str_length;i++)
            {
                cda.cla=font_source->GetCLA(*cp);

                if(cda.cla->visible)
                {
                    chars_sets.Add(*cp);                //统计所有不重复字符
                    ++draw_chars_count;
                }

                cda.uv.Set(0,0,0,0);                    //初始化UV

                draw_chars_list.Add(cda);

                ++cp;
            }
        }

        //释放不再使用的字符
        {
            clear_chars_sets=tr->GetCharsSets();                    //获取不再使用的字符合集

            clear_chars_sets.Delete(chars_sets);                    //清除下一步要用的字符合集

            if(clear_chars_sets.GetCount()>0)                       //可以彻底清除的字符
            {
                tile_font->Unregistry(clear_chars_sets);

                clear_chars_sets.Clear();
            }
        }

        //注册不重复字符给tile font系统，获取所有字符的UV
        if(!tile_font->Registry(chars_uv,chars_sets))
        {
            draw_chars_list.Clear();
            chars_sets.Clear();

            return(false);
        }

        tr->SetCharsSets(chars_sets);                               //注册需要使用的字符合集

        //为可绘制字符列表中的字符获取UV
        for(CharDrawAttr &cda:draw_chars_list)
        {
            chars_uv.Get(cda.cla->attr->ch,
                         cda.uv);
        }

        return(true);
    }

    /**
        * 将字符串断成多行，处理标点禁用，以及英文单词禁拆分
        */
    bool TextLayout::h_splite_to_lines(float view_limit)
    {
        //const int count=cla_list.GetCount();
        //const CLA **cla=cla_list.GetData();

        //int cur_size=0;

        //for(int i=0;i<count;i++)
        //{
        //}

        return(true);
    }

    bool TextLayout::v_splite_to_lines(float view_limit)
    {

        return(true);
    }

    //int TextLayout::Layout(const int mc,const String<T> &str)
    //{
    //    if(mc<=0
    //        ||!str
    //        ||!(*str))
    //        return(-1);
    //
    //    max_chars=mc;
    //    origin_string=str;
    //
    //    if(StatChars()<=0)
    //        return(-3);
    //                
    //    if(!rc->Init(draw_chars_count))
    //        return(-4);
    //
    //    vertex      =rc->AccessVAD<VB4f>(VAN::Position);
    //    tex_coord   =rc->AccessVAD<VB4f>(VAN::TexCoord);
    //
    //    if(!vertex||!tex_coord)
    //        return(-5);
    //
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
    //
    //    return 0;
    //}

    int TextLayout::sl_l2r()
    {
        int cur_size=0;
        int left=0,top=0;

        int16 *tp=vertex;
        float *tcp=tex_coord;

        for(const CharDrawAttr &cda:draw_chars_list)
        {
            if(cda.cla->visible)
            {
                tp=WriteRect(   tp,
                                left+cda.cla->metrics.x,
                                top -cda.cla->metrics.y+font_source->GetCharHeight(),
                                cda.cla->metrics.w,
                                cda.cla->metrics.h);

                tcp=WriteRect(tcp,cda.uv);

                left+=cda.cla->metrics.adv_x;
            }
            else
            {
                if(cda.cla->attr->ch==' ')
                    left+=tda.space_size;
                else
                if(cda.cla->attr->ch==U32_FULL_WIDTH_SPACE)
                    left+=tda.full_space_size;
                else
                if(cda.cla->attr->ch=='\t')
                    left+=tda.tab_size;
                else
                if(cda.cla->attr->ch=='\n')
                {
                    left=0;
                    top+=font_source->GetCharHeight()+tda.line_gap;
                }
                else
                {
                    left+=cda.cla->metrics.adv_x;
                }
            }
        }

        return draw_chars_list.GetCount(); //返回绘制的字符数量
    }

    int TextLayout::sl_r2l(){return 0;}
    int TextLayout::sl_v(){return 0;}

    //bool TextLayout::PrepareVBO()
    //{
    //    if(draw_chars_count<=0
    //        ||!text_primitive
    //        ||!text_primitive->IsValid())
    //    {
    //        return(false);
    //    }

    //    vertex      .SetCount(max_chars*4);
    //    tex_coord   .SetCount(max_chars*4);

    //    if(!vertex||!tex_coord)
    //        return(-5);
    //}

    /**
    * 简易文本排版。无任何特殊处理，不支持\t\n之外任何转义符
    */
    template<typename T>
    int TextLayout::SimpleLayout(TextPrimitive *tr,TileFont *tf,const String<T> &str)
    {
        if(!tr)
            return(-1);

        if(!tf||str.IsEmpty())
            return(-1);

        int max_chars=str.Length();

        if(!StatChars<T>(tr,tf,str.c_str(),max_chars))
            return(-2);

        if(draw_chars_count<=0)             //可绘制字符为0？？？这是全空格？
            return(-3);

        vertex      .SetCount(max_chars*4);
        tex_coord   .SetCount(max_chars*4);

        if(!vertex||!tex_coord)
            return(-5);

        int result;

        if(tda.tla.text_direction==TextDirection::Vertical)     result=sl_v();else
        if(tda.tla.text_direction==TextDirection::RightToLeft)  result=sl_r2l();else
                                                                result=sl_l2r();

        if(result>0)
        {
            tr->SetCharCount(result);
            tr->WriteVertex(vertex);
            tr->WriteTexCoord(tex_coord);
        }

        return result;
    }
        
    int TextLayout::SimpleLayout(TextPrimitive *tr,TileFont *tf,const U16String &str){return this->SimpleLayout<u16char>(tr,tf,str);}
    int TextLayout::SimpleLayout(TextPrimitive *tr,TileFont *tf,const U32String &str){return this->SimpleLayout<u32char>(tr,tf,str);}

    //template<typename T>
    //int TextLayout::SimpleLayout(TileFont *tf,const StringList<String<T>> &sl)
    //{
    //}

    //int TextLayout::SimpleLayout(TileFont *tf,const UTF16StringList &sl){return this->SimpleLayout<u16char>(tf,sl);}
    //int TextLayout::SimpleLayout(TileFont *tf,const UTF32StringList &sl){return this->SimpleLayout<u32char>(tf,sl);}
}//namespace hgl::graph
