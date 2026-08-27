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

    uint16_t TextLayout::GetOrRegisterCharId(u32char ch,const CharMetricsInfo &metrics,const TileUVFloat &uv,uint16_t variant)
    {
        const uint64 key=(static_cast<uint64>(variant)<<32)|static_cast<uint32_t>(ch);

        uint16_t id;
        if(char_to_id.Get(key,id))
            return id;

        id=(uint16_t)char_info_table.size();
        char_to_id.Add(key,id);

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

    // ── 禁断（行首/行尾禁用符号）决策 ─────────────────────────────
    // sl_l2r 与 sl_v 共用：前瞻下一可见字符，判定换行/换列时是否需要
    // 行尾回退（end_disable 符号不能停在行尾）或行首禁断（begin_disable
    // 符号不能出现在行首）。
    enum class BorderSymbolAction { NormalBreak, Rollback, Keep, RollbackAndKeep };

    BorderSymbolAction DecideBorderSymbolAction(const bool check_border_symbols,
                                                const CharDrawAttrIt &it_cda,
                                                const int i,
                                                const int str_length,
                                                const bool last_vis_has_end_disable,
                                                const int visible_char_count,
                                                const bool at_paragraph_start)
    {
        if(!check_border_symbols)
            return BorderSymbolAction::NormalBreak;

        // 前瞻：查找后续第一个可见字符
        bool next_vis_has_begin_disable = false;
        bool has_more_visible = false;
        {
            CharDrawAttrIt scan = it_cda;
            for(int j = i + 1; j < str_length; j++)
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

        // 行尾回退（上一可见字符有 end_disable 且有后续可见字符）
        const bool need_end_rollback = last_vis_has_end_disable
                                    && visible_char_count > 0
                                    && has_more_visible;

        // 行首禁断（下一可见字符有 begin_disable 且非段首）
        const bool need_begin_keep = !at_paragraph_start
                                  && has_more_visible
                                  && next_vis_has_begin_disable;

        if(need_end_rollback && need_begin_keep)  return BorderSymbolAction::RollbackAndKeep;
        if(need_end_rollback)                     return BorderSymbolAction::Rollback;
        if(need_begin_keep)                       return BorderSymbolAction::Keep;
        return BorderSymbolAction::NormalBreak;
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

                    const BorderSymbolAction action = DecideBorderSymbolAction(check_border_symbols, it_cda, i, dsi.str.length,
                                                                               last_vis_has_end_disable, visible_char_count,
                                                                               at_paragraph_start);

                    if(action == BorderSymbolAction::Rollback)
                    {
                        // 仅行尾禁用：回退上一可见字符到下一行
                        gpu_char_instances.pop_back();

                        left = dsi.style.start_position.x;
                        top += line_step;

                        gpu_char_instances.push_back({static_cast<int16_t>(left),static_cast<int16_t>(top),last_vis_cid,last_vis_sid,0});
                        left += last_vis_adv_x;
                    }
                    else if(action == BorderSymbolAction::NormalBreak)
                    {
                        // 正常换行（或未启用禁断检查）
                        left = dsi.style.start_position.x;
                        top += line_step;
                    }
                    // Keep / RollbackAndKeep：不换行，被回退的字符留在当前行末尾

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

        // 竖排居中标点：逗号/句号/分号/叹号/问号/顿号/冒号等正立标点，
        // 在竖排中需要于整个字符位置（全角格）内居中显示
        // （vrotate 旋转符号不居中，保持旋转后占位）
        constexpr u32char   VerticalCenterSymbols []=U32_TEXT("，。；！？、：");
        constexpr int       VerticalCenterSymbolsCount=(sizeof(VerticalCenterSymbols)/sizeof(u32char))-1;

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

        // ── vrot 组整体旋转状态 ─────────────────────────────────────
        // 连续 vrotate 字符按横向排版（框顶对齐、字距 = 横向 adv）排列，
        // 再整体绕段首字符中心旋转 90°（视觉顺时针）。mesh 侧每字母绕
        // 自身框中心旋转，CPU 放置"旋转前框左上角" = 目标中心 - (cx, cy)。
        // 横向基线（= 框顶 + char_height，所有字母统一）旋转后成为竖直线
        // x = C0_x + cy_0 - char_height = pen_x - my_0——与段内字母无关，
        // 基线严格对齐，无需逐字母补偿公式。
        bool in_vrot_run   = false;
        int  vrot_run_adv  = 0;      // 段内累计横向字距（从段首起）
        int  vrot_base_cx  = 0;      // 段首字符旋转中心（相对框左上角）
        int  vrot_base_cy  = 0;
        int  vrot_c0x      = 0;      // 段首字符旋转中心（绝对坐标）
        int  vrot_c0y      = 0;

        // 竖排列宽（从右到左换列步进）：用字体高度（方字假设，并容纳 vrotate 字符旋转后的宽度）
        const int column_step = static_cast<int>((font_source->GetCharHeight() + dsi.style.line_gap + dsi.style.extra_advance_y) * scale);

        for(int i=0;i<dsi.str.length;i++)
        {
            const CharDrawAttr &cda=*it_cda;

            if(cda.cla->visible)
            {
                const bool vrot = cda.cla->attr->vrotate;

                const int  full_h = font_source->GetCharHeight();

                // 前瞻：下一个字符是否也是 vrotate（判断当前是否 vrot 组尾）
                bool next_vrot = false;
                if(i + 1 < dsi.str.length)
                {
                    CharDrawAttrIt next = it_cda;
                    ++next;
                    const CharDrawAttr &nxt = *next;
                    if(nxt.cla->visible)
                        next_vrot = nxt.cla->attr->vrotate;
                }

                int adv;
                if(vrot)
                {
                    // vrotate：原样照搬 sl_l2r 横向排版——横向字距（adv_x + extra_advance_x），
                    // 段内整体右转 90° 后字母沿 y 排列，字距 = 横向字距（不拉大）
                    adv = static_cast<int>((cda.cla->metrics.adv_x + dsi.style.extra_advance_x) * scale);
                }
                else
                {
                    // 正立字符（含标点）至少占一个全角格（max(字形高, 字体高度)），
                    // 否则逗号句号等小标点会把行距压缩得很小
                    const int adv_h = cda.cla->metrics.h > full_h ? cda.cla->metrics.h : full_h;
                    adv = static_cast<int>((adv_h + dsi.style.extra_advance_y) * scale);
                }
                // 字符框宽：vrotate 旋转 90° 后水平占位 = 原字形高（否则右溢列宽）；
                // 正立字符框宽至少一个全角格（标点不再贴右缘，居右下显示）
                const int  cw     = vrot ? cda.cla->metrics.h
                                         : (cda.cla->metrics.w > full_h ? cda.cla->metrics.w : full_h);

                int left = pen_x - cw;                  // 框左上角 x（正立默认）
                int top  = pen_y;                       // 框顶部 y（正立默认）
                const int16_t rot = static_cast<int16_t>(vrot ? 90 : 0);

                if(vrot)
                {
                    // 段内第 k 字母：整体旋转中心（绝对坐标）
                    //   Ck = C0 + (-(cy_k - cy_0), Σadv + (cx_k - cx_0))
                    const int cx = cda.cla->metrics.x + cda.cla->metrics.w / 2;
                    const int cy = -cda.cla->metrics.y + full_h + cda.cla->metrics.h / 2;

                    if(!in_vrot_run)
                    {
                        // 段首：旋转中心 C0——右缘贴列右缘（pen_x）、顶贴列顶（pen_y）
                        in_vrot_run  = true;
                        vrot_run_adv = 0;
                        vrot_base_cx = cx;
                        vrot_base_cy = cy;
                        vrot_c0x     = pen_x - cda.cla->metrics.h / 2;
                        vrot_c0y     = pen_y + cda.cla->metrics.w / 2;
                    }

                    const int ck_abs_x = vrot_c0x - (cy - vrot_base_cy);
                    const int ck_abs_y = vrot_c0y + vrot_run_adv + (cx - vrot_base_cx);

                    left = ck_abs_x - cx;               // 旋转前框左上角
                    top  = ck_abs_y - cy;

                    if(!next_vrot)
                    {
                        // 组尾：pen_y 跳到最后字母视觉底（中心 y + 原宽/2）。
                        // 下一正立字符位图顶 = pen_y + char_height - my ≈ 视觉底
                        //（中文 my ≈ char_height），紧贴不重叠——与横向排版
                        // 整体右转 90° 语义一致（不拉大间距）
                        pen_y = ck_abs_y + cda.cla->metrics.w / 2;
                        in_vrot_run = false;
                    }
                    else
                    {
                        vrot_run_adv += adv;
                    }
                }
                else
                {
                    in_vrot_run = false;
                }

                // 居中修正：位图中心对齐全角格中心（仅正立标点）。
                // 通过修正 offset_x/y 实现——mesh 用 char_info.offset 定位位图，
                // 修正后 quad 中心 = 格中心（水平/垂直都居中）。
                // 注意：修正版 char_info 用 variant=1 单独注册，与横排（variant=0）
                // 互不冲突——同字符横竖混排时各自取用正确版本。
                const bool vcenter = (!vrot && hgl::strchr(VerticalCenterSymbols, cda.cla->attr->ch, VerticalCenterSymbolsCount));
                CharMetricsInfo metrics = cda.cla->metrics;
                if(vcenter)
                {
                    metrics.x = (cw - metrics.w) / 2;
                    metrics.y = (adv - metrics.h) / 2;
                }

                // 更新列尾禁用追踪
                last_vis_has_end_disable = cda.cla->attr->end_disable;
                last_vis_left_before     = left;
                last_vis_adv_y           = adv;
                last_vis_rot             = rot;

                // Store GPU instance data: 框左上角 + char_id + style_id + 实例旋转
                const uint16_t cid=GetOrRegisterCharId(cda.cla->attr->ch, metrics, cda.uv, vcenter ? 1 : 0);
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

                    const BorderSymbolAction action = DecideBorderSymbolAction(check_border_symbols, it_cda, i, dsi.str.length,
                                                                               last_vis_has_end_disable, visible_char_count,
                                                                               at_paragraph_start);

                    if(action == BorderSymbolAction::Rollback)
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
                    else if(action == BorderSymbolAction::NormalBreak)
                    {
                        // 正常换列（或未启用禁断检查）
                        pen_x -= column_step;
                        pen_y  = col_top;
                    }
                    // Keep / RollbackAndKeep：不换列，被回退的字符留在当前列末尾

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
