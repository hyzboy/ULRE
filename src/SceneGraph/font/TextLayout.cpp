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
        chars_sets.Reserve(Estimate);
        draw_chars_list.Clear();
        draw_chars_list.Reserve(Estimate);

        text_primitive=tr;
        vertex.Reserve(Estimate*4);
        tex_coord.Reserve(Estimate*4);

        draw_all_strings.Clear();
        draw_string_list.Clear();

        return(true);
    }

    bool TextLayout::AddString(const U16StringView &str,const TextDrawStyle &style)
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
    bool TextLayout::StatChars()
    {
        if(!text_primitive
         ||!tile_font
         ||!font_source)
            return(false);

        if(draw_all_strings.IsEmpty())
            return(false);

        const int str_count=draw_all_strings.GetCount();

        if(str_count<=0)
            return(false);

        //遍历所有字符，取得每一个字符的基本绘制信息
        //for(int i=0;i<str_count;i++)
        for (auto &csv:draw_all_strings)  //C++11 range-for
        {
            const u16char *cp=csv.GetString();
            CharDrawAttr cda;

            for(int i=0;i<csv.length;i++)
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

    int TextLayout::sl_l2r(const DrawStringItem &dsi)
    {
        int cur_size=0;
        int left=dsi.style.start_position.x;
        int top =dsi.style.start_position.y;

        int16 *tp=dsi.vertex;
        float *tcp=dsi.tex_coord;

        int visible_char_count=0;

        CharDrawAttrIt it_cda=dsi.it;

        for(int i=0;i<dsi.str.length;i++)
        {
            const CharDrawAttr &cda=*it_cda;

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
                if(cda.cla->attr->ch==' ')                  left+=dsi.style.space_size;       else
                if(cda.cla->attr->ch==U32_FULL_WIDTH_SPACE) left+=dsi.style.full_space_size;  else
                if(cda.cla->attr->ch=='\t')                 left+=dsi.style.tab_size;         else
                if(cda.cla->attr->ch=='\n')
                {
                    left=dsi.style.start_position.x;
                    top+=font_source->GetCharHeight()+dsi.style.line_gap;
                }
                else
                {
                    left+=cda.cla->metrics.adv_x;
                }
            }

            ++it_cda;
        }

        return visible_char_count; //返回绘制的字符数量
    }

    int TextLayout::sl_r2l(const DrawStringItem &){return 0;}
    int TextLayout::sl_v(const DrawStringItem &){return 0;}

    int TextLayout::End()
    {
        if(!text_primitive)
            return(-1);

        if(draw_all_strings.IsEmpty())
            return(0);

        if(!StatChars())
            return(-1);

        if(draw_chars_count<=0)             //可绘制字符为0？？？这是全空格？
            return(-4);

        vertex      .Resize(draw_chars_count*4);
        tex_coord   .Resize(draw_chars_count*4);

        if(!vertex||!tex_coord)
            return(-5);

        int total=0;
        int dc;

        auto it_cda=draw_chars_list.begin();
        int16 *vp=vertex;
        float *tcp=tex_coord;

        for(DrawStringItem &dsi:draw_string_list)
        {
            dsi.it=it_cda;
            dsi.vertex=vp;
            dsi.tex_coord=tcp;

            if(dsi.style.para_style.text_direction==TextDirection::Vertical)    dc=sl_v  (dsi);else
            if(dsi.style.para_style.text_direction==TextDirection::RightToLeft) dc=sl_r2l(dsi);else
                                                                                dc=sl_l2r(dsi);

            it_cda+=dsi.str.length;
            vp+=dc*4;
            tcp+=dc*4;

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
