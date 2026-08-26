#include<hgl/graph/font/TextLayoutEngine.h>
#include<hgl/graph/font/TileFont.h>
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

    bool TextLayout::Begin(U32CharSet *cs,int Estimate)
    {
        if(!cs||Estimate<=0)
            return(false);

        draw_chars_count=0;
        chars_sets.Clear();
        draw_chars_list.Clear();
        draw_chars_list.Reserve(Estimate);

        atlas_chars_sets=cs;

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
        if(!atlas_chars_sets)
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
        if(!atlas_chars_sets
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
            clear_chars_sets=*atlas_chars_sets;                     //获取不再使用的字符合集

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

        *atlas_chars_sets=chars_sets;                                             //注册需要使用的字符合集

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
        const float scale = dsi.style.scale;
        int left=dsi.style.start_position.x;
        int top =dsi.style.start_position.y;

        int visible_char_count=0;

        CharDrawAttrIt it_cda=dsi.it;

        const bool check_border_symbols = dsi.style.para_style.disable_border_symbols;
        bool at_paragraph_start = true;     // 当前是否处于段落起始位置（第一个可见字符不做行首禁用检查）

        // 行尾禁用追踪：记录上一个可见字符的信息
        bool     last_vis_has_end_disable = false;
        int      last_vis_left_before     = 0;     // 上一个可见字符放置前的 left
        int      last_vis_adv_x           = 0;     // 上一个可见字符的 advance（用于位置恢复）
        uint16_t last_vis_cid             = 0;     // 上一个可见字符的 char_id
        uint16_t last_vis_sid             = 0;     // 上一个可见字符的 style_id

        for(int i=0;i<dsi.str.length;i++)
        {
            const CharDrawAttr &cda=*it_cda;

            if(cda.cla->visible)
            {
                // 旋转 90/270° 时字符水平占位 = 字形高度（宽高互换）；
                // 0/180° 保持原水平 advance。
                // 注意不能改用 metrics.adv_y：FreeType 横排加载时垂直
                // advance 恒为 0，会导致整行坍缩。
                const bool rot_swap = (dsi.style.rotation == 90 || dsi.style.rotation == 270);
                const int  adv_w    = rot_swap ? cda.cla->metrics.h : cda.cla->metrics.adv_x;
                const int  adv      = static_cast<int>((adv_w + dsi.style.extra_advance_x) * scale);

                // 更新行尾禁用追踪
                last_vis_has_end_disable = cda.cla->attr->end_disable;
                last_vis_left_before     = left;
                last_vis_adv_x           = adv;

                // Store GPU instance data: pen position + char_id + style_id
                const uint16_t cid=GetOrRegisterCharId(cda.cla->attr->ch,cda.cla->metrics,cda.uv);
                gpu_char_instances.push_back({static_cast<int16_t>(left),static_cast<int16_t>(top),cid,dsi.style.style_id,0});

                last_vis_cid = cid;
                last_vis_sid = dsi.style.style_id;

                left+=adv;

                ++visible_char_count;
                at_paragraph_start = false;
            }
            else
            {
                if(cda.cla->attr->ch==' ')                  left+=static_cast<int>(dsi.style.space_size * scale);       else
                if(cda.cla->attr->ch==U32_FULL_WIDTH_SPACE) left+=static_cast<int>(dsi.style.full_space_size * scale);  else
                if(cda.cla->attr->ch=='\t')                 left+=static_cast<int>(dsi.style.tab_size * scale);         else
                if(cda.cla->attr->ch=='\n')
                {
                    const int line_step=static_cast<int>((font_source->GetCharHeight() + dsi.style.line_gap + dsi.style.extra_advance_y) * scale);

                    if(check_border_symbols)
                    {
                        // 前瞻：查找后续第一个可见字符
                        bool next_vis_has_begin_disable = false;
                        bool has_more_visible = false;
                        {
                            CharDrawAttrIt scan = it_cda;
                            for(int j = i + 1; j < dsi.str.length; j++)
                            {
                                ++scan;
                                const CharDrawAttr &sc = *scan;
                                if(sc.cla->visible)
                                {
                                    has_more_visible = true;
                                    next_vis_has_begin_disable = sc.cla->attr->begin_disable;
                                    break;
                                }
                            }
                        }

                        // 判断是否需要行尾回退（上一可见字符有 end_disable 且有后续可见字符）
                        const bool need_end_rollback = last_vis_has_end_disable
                                                    && visible_char_count > 0
                                                    && has_more_visible;

                        // 判断是否需要行首禁断（下一可见字符有 begin_disable 且非段首）
                        const bool need_begin_keep = !at_paragraph_start
                                                  && has_more_visible
                                                  && next_vis_has_begin_disable;

                        if(need_end_rollback && need_begin_keep)
                        {
                            // 两者同时触发：end-disable 要移到下一行，begin-disable 要留在当前行
                            // 结果：不換行，被回退的字符留在当前行末尾
                            // 无需任何操作：字符已在当前位置，left 也正确
                        }
                        else if(need_end_rollback)
                        {
                            // 仅行尾禁用：回退上一可见字符到下一行
                            left = last_vis_left_before;
                            gpu_char_instances.pop_back();

                            // 执行换行
                            left = dsi.style.start_position.x;
                            top += line_step;

                            // 在下一行重新放置被回退的字符
                            gpu_char_instances.push_back({static_cast<int16_t>(left),static_cast<int16_t>(top),last_vis_cid,last_vis_sid});
                            left += last_vis_adv_x;
                        }
                        else if(need_begin_keep)
                        {
                            // 仅行首禁用：不换行，留在当前行
                            // 无需任何操作
                        }
                        else
                        {
                            // 正常换行
                            left = dsi.style.start_position.x;
                            top += line_step;
                        }
                    }
                    else
                    {
                        left=dsi.style.start_position.x;
                        top+=line_step;
                    }

                    at_paragraph_start = true;
                    last_vis_has_end_disable = false;
                }
                else
                {
                    left+=static_cast<int>((cda.cla->metrics.adv_x + dsi.style.extra_advance_x) * scale);
                }
            }

            ++it_cda;
        }

        // 记录排版后的最终尺寸
        layout_width_ = left;
        layout_height_ = top + font_source->GetCharHeight() + dsi.style.line_gap;

        return visible_char_count; //返回绘制的字符数量
    }

    int TextLayout::sl_r2l(const DrawStringItem &){return 0;}
    int TextLayout::sl_v(const DrawStringItem &dsi)
    {
        const float scale = dsi.style.scale;

        // 竖排（从上到下，从右到左）：首坐标为第一个字符的右上角
        int pen_x = dsi.style.start_position.x;   // 列位置（字符右边缘 x）
        int pen_y = dsi.style.start_position.y;   // 列顶（字符顶部 y）

        int visible_char_count=0;

        CharDrawAttrIt it_cda=dsi.it;

        const bool check_border_symbols = dsi.style.para_style.disable_border_symbols;
        bool at_paragraph_start = true;     // 当前是否处于段首位置（第一个可见字符不做列首禁用检查）

        // 列尾禁用追踪：记录上一个可见字符的信息（竖排"行尾"= 列尾，即底部）
        bool     last_vis_has_end_disable = false;
        int      last_vis_left_before     = 0;     // 上一个可见字符放置前的 left（框左上角 x）
        int      last_vis_adv_y           = 0;     // 上一个可见字符的垂直推进
        uint16_t last_vis_cid             = 0;     // 上一个可见字符的 char_id
        uint16_t last_vis_sid             = 0;     // 上一个可见字符的 style_id
        int16_t  last_vis_rot             = 0;     // 上一个可见字符的实例旋转

        // 竖排列宽（从右到左换列步进）：用字体高度（方字假设，并容纳 vrotate 字符旋转后的宽度）
        const int column_step = static_cast<int>((font_source->GetCharHeight() + dsi.style.line_gap + dsi.style.extra_advance_y) * scale);

        for(int i=0;i<dsi.str.length;i++)
        {
            const CharDrawAttr &cda=*it_cda;

            if(cda.cla->visible)
            {
                const bool vrot = cda.cla->attr->vrotate;

                // 垂直推进：vrotate 字符右旋 90° 后垂直占位 = 原字形宽；
                // 正立字符 = 原字形高（adv_y 在横排 FreeType 下恒为 0，不可用）
                const int  adv_h = vrot ? cda.cla->metrics.w : cda.cla->metrics.h;
                const int  adv   = static_cast<int>((adv_h + dsi.style.extra_advance_y) * scale);
                const int  cw    = cda.cla->metrics.w;          // 字符框宽（右上角 → 左上角换算）

                const int   left = pen_x - cw;                  // 框左上角 x
                const int   top  = pen_y;                       // 框顶部 y
                const int16_t rot = static_cast<int16_t>(vrot ? 90 : 0);

                // 更新列尾禁用追踪
                last_vis_has_end_disable = cda.cla->attr->end_disable;
                last_vis_left_before     = left;
                last_vis_adv_y           = adv;
                last_vis_rot             = rot;

                // Store GPU instance data: 框左上角 + char_id + style_id + 实例旋转
                const uint16_t cid=GetOrRegisterCharId(cda.cla->attr->ch,cda.cla->metrics,cda.uv);
                gpu_char_instances.push_back({static_cast<int16_t>(left),static_cast<int16_t>(top),cid,dsi.style.style_id,rot});

                last_vis_cid = cid;
                last_vis_sid = dsi.style.style_id;

                pen_y += adv;

                ++visible_char_count;
                at_paragraph_start = false;
            }
            else
            {
                if(cda.cla->attr->ch==' ')                  pen_y += static_cast<int>(dsi.style.space_size * scale);       else
                if(cda.cla->attr->ch==U32_FULL_WIDTH_SPACE) pen_y += static_cast<int>(dsi.style.full_space_size * scale);  else
                if(cda.cla->attr->ch=='\t')                 pen_y += static_cast<int>(dsi.style.tab_size * scale);         else
                if(cda.cla->attr->ch=='\n')
                {
                    const int col_top = dsi.style.start_position.y;   // 列顶 = 首坐标 y

                    if(check_border_symbols)
                    {
                        // 前瞻：查找后续第一个可见字符
                        bool next_vis_has_begin_disable = false;
                        bool has_more_visible = false;
                        {
                            CharDrawAttrIt scan = it_cda;
                            for(int j = i + 1; j < dsi.str.length; j++)
                            {
                                ++scan;
                                const CharDrawAttr &sc = *scan;
                                if(sc.cla->visible)
                                {
                                    has_more_visible = true;
                                    next_vis_has_begin_disable = sc.cla->attr->begin_disable;
                                    break;
                                }
                            }
                        }

                        // 判断是否需要列尾回退（上一可见字符有 end_disable 且有后续可见字符）
                        const bool need_end_rollback = last_vis_has_end_disable
                                                    && visible_char_count > 0
                                                    && has_more_visible;

                        // 判断是否需要列首禁断（下一可见字符有 begin_disable 且非段首）
                        const bool need_begin_keep = !at_paragraph_start
                                                  && has_more_visible
                                                  && next_vis_has_begin_disable;

                        if(need_end_rollback && need_begin_keep)
                        {
                            // 两者同时触发：end-disable 移到下一列、begin-disable 留在当前列
                            // 结果：不换列，被回退的字符留在当前列末尾
                        }
                        else if(need_end_rollback)
                        {
                            // 仅列尾禁用：回退上一可见字符到下一列
                            gpu_char_instances.pop_back();

                            // 换列（从右到左）
                            pen_x -= column_step;
                            pen_y  = col_top;

                            // 在下一列重新放置被回退的字符（left 随列左移一列宽）
                            gpu_char_instances.push_back({static_cast<int16_t>(last_vis_left_before - column_step),static_cast<int16_t>(col_top),last_vis_cid,last_vis_sid,last_vis_rot});
                            pen_y = col_top + last_vis_adv_y;
                        }
                        else if(need_begin_keep)
                        {
                            // 仅列首禁用：不换列，留在当前列
                        }
                        else
                        {
                            // 正常换列
                            pen_x -= column_step;
                            pen_y  = col_top;
                        }
                    }
                    else
                    {
                        pen_x -= column_step;
                        pen_y  = col_top;
                    }

                    at_paragraph_start = true;
                    last_vis_has_end_disable = false;
                }
                else
                {
                    pen_y += static_cast<int>((cda.cla->metrics.h + dsi.style.extra_advance_y) * scale);
                }
            }

            ++it_cda;
        }

        // 记录排版后的最终尺寸
        layout_width_  = dsi.style.start_position.x - pen_x + column_step;
        layout_height_ = pen_y + font_source->GetCharHeight();

        return visible_char_count; //返回绘制的字符数量
    }

    int TextLayout::End()
    {
        if(!atlas_chars_sets)
            return(-1);

        if(draw_all_strings.IsEmpty())
            return(0);

        if(!StatChars())
            return(-1);

        if(draw_chars_count<=0)             //可绘制字符为0？？？这是全空格？
            return(-4);

        int total=0;
        int dc;

        auto it_cda=draw_chars_list.begin();

        for(DrawStringItem &dsi:draw_string_list)
        {
            dsi.it=it_cda;

            if(dsi.style.para_style.text_direction==TextDirection::Vertical)    dc=sl_v  (dsi);else
            if(dsi.style.para_style.text_direction==TextDirection::RightToLeft) dc=sl_r2l(dsi);else
                                                                                dc=sl_l2r(dsi);

            it_cda+=dsi.str.length;

            total+=dc;
        }

        atlas_chars_sets=nullptr;
        return(total);
    }
}//namespace hgl::graph::layout
