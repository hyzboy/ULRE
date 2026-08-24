#include<hgl/graph/font/TextLayoutEngine.h>
#include<hgl/graph/font/TileFont.h>
#include<hgl/graph/font/TextGeometry.h>
#include<hgl/graph/font/TextCharSSBO.h>
#include<hgl/graph/geo/GeometryCreater.h>   // FloatToHalf
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

    bool TextLayout::Begin(TextGeometry *tr,int Estimate)
    {
        if(!tr||Estimate<=0)
            return(false);

        draw_chars_count=0;
        chars_sets.Clear();
        draw_chars_list.Clear();
        draw_chars_list.Reserve(Estimate);

        text_primitive=tr;
        vertex.reserve(Estimate*12);
        tex_coord.reserve(Estimate*12);

        draw_all_strings.Clear();
        draw_string_list.Clear();
        gpu_char_instances.clear();
        gpu_char_instances.reserve(Estimate);

        char_info_table.clear();
        char_to_id.Clear();
        char_styles.clear();

        return(true);
    }

    bool TextLayout::AddString(const U16StringView &str, const TextDrawStyle &style)
    {
        if(!text_primitive)
            return(false);

        if(str.IsEmpty())
            return(false);

        DrawStringItem item;

        // ✅ 使用 AddAndGet 获取 ConstStringView，并添加到 draw_all_strings
        auto *csv = draw_all_strings.AddAndGet(str);

        if(!csv)
            return(false);

        item.str = *csv;  // 保存 ConstStringView
        item.style = style;

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

            // 清除下一步要用的字符合集
            for(auto ch : chars_sets)
            {
                clear_chars_sets.Delete(ch);
            }

            if(clear_chars_sets.GetCount()>0)                       //可以彻底清除的字符
            {
                std::vector<u32char> temp_clear_chars(clear_chars_sets.begin(), clear_chars_sets.end());
                tile_font->Unregistry(temp_clear_chars);

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

    uint16_t TextLayout::GetOrRegisterCharId(u32char ch,const CharMetricsInfo &metrics,const TileUVFloat &uv)
    {
        uint16_t id;
        if(char_to_id.Get(ch,id))
            return id;

        id=(uint16_t)char_info_table.size();
        char_to_id.Add(ch,id);

        layout::TextCharInfo info;
        info.offset_x  =(int16_t)metrics.x;
        info.offset_y  =(int16_t)metrics.y;
        info.metrics_w =(uint16_t)metrics.w;
        info.metrics_h =(uint16_t)metrics.h;
        info.uv_left   =FloatToHalf(uv.GetLeft());
        info.uv_top    =FloatToHalf(uv.GetTop());
        info.uv_right  =FloatToHalf(uv.GetRight());
        info.uv_bottom =FloatToHalf(uv.GetBottom());

        char_info_table.push_back(info);
        return id;
    }

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
                const int16 rect_left  =int16(left+cda.cla->metrics.x);
                const int16 rect_top   =int16(top -cda.cla->metrics.y+font_source->GetCharHeight());
                const int16 rect_right =int16(rect_left+cda.cla->metrics.w);
                const int16 rect_bottom=int16(rect_top +cda.cla->metrics.h);

                const float uv_left   =cda.uv.GetLeft();
                const float uv_top    =cda.uv.GetTop();
                const float uv_right  =cda.uv.GetRight();
                const float uv_bottom =cda.uv.GetBottom();

                tp[0]=rect_left;  tp[1]=rect_top;
                tp[2]=rect_left;  tp[3]=rect_bottom;
                tp[4]=rect_right; tp[5]=rect_top;
                tp[6]=rect_right; tp[7]=rect_top;
                tp[8]=rect_left;  tp[9]=rect_bottom;
                tp[10]=rect_right;tp[11]=rect_bottom;
                tp+=12;

                tcp[0]=uv_left;   tcp[1]=uv_top;
                tcp[2]=uv_left;   tcp[3]=uv_bottom;
                tcp[4]=uv_right;  tcp[5]=uv_top;
                tcp[6]=uv_right;  tcp[7]=uv_top;
                tcp[8]=uv_left;   tcp[9]=uv_bottom;
                tcp[10]=uv_right; tcp[11]=uv_bottom;
                tcp+=12;

                // Store GPU instance data: pen position + char_id + style_id
                const uint16_t cid=GetOrRegisterCharId(cda.cla->attr->ch,cda.cla->metrics,cda.uv);
                gpu_char_instances.push_back({static_cast<int16_t>(left),static_cast<int16_t>(top),cid,dsi.style.style_id});

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

        vertex      .resize(draw_chars_count*12);
        tex_coord   .resize(draw_chars_count*12);

        if(vertex.empty()||tex_coord.empty())
            return(-5);

        int total=0;
        int dc;

        auto it_cda=draw_chars_list.begin();
        int16 *vp=vertex.data();
        float *tcp=tex_coord.data();

        for(DrawStringItem &dsi:draw_string_list)
        {
            dsi.it=it_cda;
            dsi.vertex=vp;
            dsi.tex_coord=tcp;

            if(dsi.style.para_style.text_direction==TextDirection::Vertical)    dc=sl_v  (dsi);else
            if(dsi.style.para_style.text_direction==TextDirection::RightToLeft) dc=sl_r2l(dsi);else
                                                                                dc=sl_l2r(dsi);

            it_cda+=dsi.str.length;
            vp+=dc*12;
            tcp+=dc*12;

            total+=dc;
        }

        if(total>0) //理论上total==draw_chars_count，不然就是错误的
        {
            // --- old CPU vertex path (kept for backward compatibility) ---
            text_primitive->SetCharCount(total*6);
            text_primitive->WriteVertex(vertex.data());
            text_primitive->WriteTexCoord(tex_coord.data());
        }

        text_primitive=nullptr;
        return(total);
    }
}//namespace hgl::graph::layout
