#include"TextLayoutEngine.h"
#include<hgl/graph/font/TileFont.h>
#include<hgl/graph/font/TextPrimitive.h>
#include<hgl/type/Extent.h>

namespace hgl::graph::layout
{
    TextLayout::TextLayout(TileFont *tf)
    {
        tile_font=tf;

        if(tf)
        {
            font_source=tf->GetFontSource();
        }
        else
        {
            font_source=nullptr;
            //HGL_ERROR(L"TextLayout::TextLayout(): tile_font==nullptr");
        }
    }

    bool TextLayout::Begin(TextPrimitive *tr,int Estimate)
    {
        if(!tr||Estimate<=0)
            return(false);

        draw_chars_count=0;
        chars_sets.Clear();
        chars_sets.PreAlloc(Estimate);
        draw_chars_list.Clear();
        draw_chars_list.PreAlloc(Estimate);

        text_primitive=tr;
        vertex.Alloc(Estimate*4);
        tex_coord.Alloc(Estimate*4);

        draw_all_strings.Clear();
        draw_string_list.Clear();

        return(true);
    }

    bool TextLayout::AddString(const U16String &str,const TextDrawStyle &style)
    {
        if(!text_primitive)
            return(false);

        if(str.IsEmpty())
            return(false);

        DrawStringItem item;

        if(draw_all_strings.AddString(item.str,str.c_str(),str.Length())<0)
            return(false);

        item.style=style;

        draw_string_list.Add(item);
        return(true);
    }

    /**
    * 预处理所有的字符，获取所有字符的宽高，以及是否标点符号等信息
    */
    bool TextLayout::StatChars(const u16char *str,const int str_length)
    {
        if(!text_primitive
         ||!tile_font
         ||!str||!*str||str_length<=0
         ||!font_source)
            return(false);

        //遍历所有字符，取得每一个字符的基本绘制信息
        {
            const u16char *cp=str;
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
            clear_chars_sets=text_primitive->GetCharsSets();        //获取不再使用的字符合集

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

        text_primitive->SetCharsSets(chars_sets);                               //注册需要使用的字符合集

        //为可绘制字符列表中的字符获取UV
        for(CharDrawAttr &cda:draw_chars_list)
        {
            chars_uv.Get(cda.cla->attr->ch,
                         cda.uv);
        }

        return(true);
    }

    //int TextLayout::SimpleLayout(const int mc,const String<T> &str)
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
    //        if(!v_splite_to_lines(para_style->max_height))
    //            return(-4);
    //    }
    //    else
    //    {
    //        if(!h_splite_to_lines(para_style->max_width))
    //            return(-4);
    //    }
    //
    //    return 0;
    //}

    int TextLayout::sl_l2r(const TextDrawStyle &tds)
    {
        int cur_size=0;
        int left=tds.start_x;
        int top =tds.start_y;

        int16 *tp=vertex;
        float *tcp=tex_coord;

        int visible_char_count=0;

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

                ++visible_char_count;
            }
            else
            {
                if(cda.cla->attr->ch==' ')                  left+=tds.space_size;       else
                if(cda.cla->attr->ch==U32_FULL_WIDTH_SPACE) left+=tds.full_space_size;  else
                if(cda.cla->attr->ch=='\t')                 left+=tds.tab_size;         else
                if(cda.cla->attr->ch=='\n')
                {
                    left=tds.start_x;
                    top+=font_source->GetCharHeight()+tds.line_gap;
                }
                else
                {
                    left+=cda.cla->metrics.adv_x;
                }
            }
        }

        return visible_char_count; //返回绘制的字符数量
    }

    int TextLayout::sl_r2l(const TextDrawStyle &){return 0;}
    int TextLayout::sl_v(const TextDrawStyle &){return 0;}

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

    ///**
    //* 简易文本排版。无任何特殊处理，不支持\t\n之外任何转义符
    //*/
    //int TextLayout::SimpleLayout(const U16String &str,const TextDrawStyle &tds)
    //{
    //    if(!text_primitive)
    //        return(-1);

    //    if(str.IsEmpty())
    //        return(-1);

    //    int max_chars=str.Length();

    //    if(!StatChars(str.c_str(),max_chars))
    //        return(-2);

    //    if(draw_chars_count<=0)             //可绘制字符为0？？？这是全空格？
    //        return(-3);

    //    vertex      .SetCount(max_chars*4);
    //    tex_coord   .SetCount(max_chars*4);

    //    if(!vertex||!tex_coord)
    //        return(-5);

    //    int result;

    //    if(tds.para_style.text_direction==TextDirection::Vertical)      result=sl_v(tds);else
    //    if(tds.para_style.text_direction==TextDirection::RightToLeft)   result=sl_r2l(tds);else
    //                                                                    result=sl_l2r(tds);

    //    if(result>0)
    //    {
    //        tr->SetCharCount(result);
    //        tr->WriteVertex(vertex);
    //        tr->WriteTexCoord(tex_coord);
    //    }

    //    return result;
    //}

    int TextLayout::End()
    {
        if(!text_primitive)
            return(-1);

        if(draw_all_strings.IsEmpty())
            return(0);

        const int all_chars_count=draw_all_strings.GetTotalLength();

        if(all_chars_count<=0)
            return(-1);

        const u16char *all_chars=draw_all_strings.GetStringData().GetData();

        if(!all_chars)      //当然这个不太可能
            return(-2);

        if(!StatChars(all_chars,all_chars_count))
            return(-3);

        if(draw_chars_count<=0)             //可绘制字符为0？？？这是全空格？
            return(-4);

        vertex      .SetCount(draw_chars_count*4);
        tex_coord   .SetCount(draw_chars_count*4);

        if(!vertex||!tex_coord)
            return(-5);

        int total=0;
        int dc;

        for(const DrawStringItem &dsi:draw_string_list)
        {
            if(dsi.style.para_style.text_direction==TextDirection::Vertical)    dc=sl_v(dsi.style);else
            if(dsi.style.para_style.text_direction==TextDirection::RightToLeft) dc=sl_r2l(dsi.style);else
                                                                                dc=sl_l2r(dsi.style);

            total+=dc;
        }

        if(total>0) //理论上total==draw_chars_count，不然就是错误的
        {
            text_primitive->SetCharCount(total);
            text_primitive->WriteVertex(vertex);
            text_primitive->WriteTexCoord(tex_coord);
        }

        text_primitive=nullptr;
        return(total);
    }
}//namespace hgl::graph::layout
