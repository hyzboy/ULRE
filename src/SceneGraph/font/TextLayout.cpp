#include<hgl/graph/font/TextLayoutEngine.h>
#include<hgl/graph/font/TileFont.h>
#include<hgl/graph/font/TextCharSSBO.h>
#include<hgl/graph/geo/GeometryCreater.h>   // FloatToHalf
#include<hgl/type/Extent.h>
#include<hgl/type/ValueArray.h>
#include<cmath>

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

    namespace
    {
        // ── 矩形排版(折行/对齐/截断)共用的度量工具 ──────────────────────
        //
        // 与旧实现的差别：把「度量」与「摆放」拆成两趟。
        //   第一趟只决定每行的字符区间（自动折行、标点禁则、行数上限、省略号）；
        //   第二趟按水平/垂直对齐把笔位写入 gpu_char_instances。
        // 对齐量依赖整行宽度，而整行宽度只有切成行之后才知道，所以必须两趟。
        // 同一套切行逻辑被 MeasureTextLayout 复用，保证「测量」与「绘制」不会漂移。

        constexpr u32char ELLIPSIS_CHARACTER=0x2026;        ///<'…'

        struct LayoutCell
        {
            const CLA   *cla        =nullptr;
            TileUVFloat  uv         {};
            int32_t      advance    =0;
            bool         visible    =false;
            bool         newline    =false;
            bool         space      =false;                 ///<半角/全角空格(可作为词边界)
        };//struct LayoutCell

        struct LayoutLine
        {
            int32_t begin       =0;                         ///<首个源下标(含)
            int32_t end         =0;                         ///<结束源下标(不含)
            int32_t width       =0;                         ///<行宽(像素)
            int32_t visible     =0;                         ///<行内可见字符数
            bool    ellipsis    =false;                     ///<行尾需要追加省略号
        };//struct LayoutLine

        struct LayoutLines
        {
            ValueArray<LayoutCell> cells;
            ValueArray<LayoutLine> lines;
            bool    truncated   =false;
            int32_t line_step   =0;                         ///<行步进(像素)
            int32_t char_height =0;                         ///<字形高度(像素)
            int32_t max_width   =0;                         ///<内容最大行宽(含省略号)
        };//struct LayoutLines

        bool IsBlankSpaceChar(const u32char ch)
        {
            return ch==' '||ch==U32_FULL_WIDTH_SPACE;
        }

        int32_t AdvanceOf(const CLA *cla,const TextDrawStyle &st,const bool rot_swap)
        {
            if(!cla)
                return(0);

            const float scale=st.scale;

            if(cla->visible)
            {
                // 旋转 90/270 时字符水平占位 = 字形高度（宽高互换）
                const int32_t adv_w=rot_swap?cla->metrics.h:cla->metrics.adv_x;

                return(static_cast<int32_t>((adv_w+st.extra_advance_x+st.char_gap)*scale));
            }

            const u32char ch=cla->attr->ch;

            if(ch==' ')                  return(static_cast<int32_t>(st.space_size     *scale));
            if(ch==U32_FULL_WIDTH_SPACE) return(static_cast<int32_t>(st.full_space_size*scale));
            if(ch=='\t')                 return(static_cast<int32_t>(st.tab_size       *scale));

            // '\r' 与 '\n' 不占横向空间（Windows 文本的 "\r\n" 只产生一次换行）
            if(ch=='\n'||ch=='\r')       return(0);

            return(static_cast<int32_t>((cla->metrics.adv_x+st.extra_advance_x+st.char_gap)*scale));
        }

        /// 省略号字形：优先 '…'，缺字形时回退 '.'，两者都无则返回 nullptr
        const CLA *EllipsisCLA(FontSource *fs)
        {
            if(!fs)
                return(nullptr);

            const CLA *cla=fs->GetCLA(ELLIPSIS_CHARACTER);

            if(!cla||!cla->visible)
                cla=fs->GetCLA('.');

            if(!cla||!cla->visible)
                return(nullptr);

            return(cla);
        }

        void BuildCell(LayoutCell &cell,const CLA *cla,const TileUVFloat &uv,
                       const TextDrawStyle &st,const bool rot_swap)
        {
            cell.cla    =cla;
            cell.uv     =uv;
            cell.visible=(cla&&cla->visible);
            cell.newline=(!cell.visible&&cla&&cla->attr->ch=='\n');
            cell.space  =(!cell.visible&&cla&&IsBlankSpaceChar(cla->attr->ch));
            cell.advance=AdvanceOf(cla,st,rot_swap);
        }

        int32_t SumWidth(const ValueArray<LayoutCell> &cells,const int32_t begin,const int32_t end)
        {
            int32_t w=0;

            for(int32_t i=begin;i<end;i++)
                w+=cells[i].advance;

            return(w);
        }

        int32_t CountVisible(const ValueArray<LayoutCell> &cells,const int32_t begin,const int32_t end)
        {
            int32_t n=0;

            for(int32_t i=begin;i<end;i++)
                if(cells[i].visible)
                    ++n;

            return(n);
        }

        int32_t FirstVisibleIndex(const ValueArray<LayoutCell> &cells,const int32_t begin,const int32_t end)
        {
            for(int32_t i=begin;i<end;i++)
                if(cells[i].visible)
                    return(i);

            return(-1);
        }

        int32_t LastVisibleIndex(const ValueArray<LayoutCell> &cells,const int32_t begin,const int32_t end)
        {
            for(int32_t i=end-1;i>=begin;i--)
                if(cells[i].visible)
                    return(i);

            return(-1);
        }

        /**
        * 第一趟：把逐字符度量切成行
        *
        * @param ll      in: cells / line_step / char_height；out: lines / truncated / max_width
        * @param fs      字体源(省略号宽度与字形查询)
        * @param st      排版风格
        */
        void BreakLines(LayoutLines &ll,FontSource *fs,const TextDrawStyle &st)
        {
            const int32_t total=ll.cells.GetCount();

            ll.lines.Clear();
            ll.truncated=false;

            const int32_t rect_w=st.rect_width;
            const bool    check_border=st.para_style.disable_border_symbols;
            const bool    word_wrap=(st.wrap_mode==TextWrapMode::Word);

            int32_t pos=0;

            while(pos<total)
            {
                LayoutLine line;
                line.begin=pos;
                line.end  =pos;

                int32_t last_space=-1;
                bool    auto_wrap =false;

                while(pos<total)
                {
                    const LayoutCell &cell=ll.cells[pos];

                    if(cell.newline)                    // 硬换行：'\n' 本身不占位
                    {
                        ++pos;
                        break;
                    }

                    if(rect_w>0&&line.visible>0&&(line.width+cell.advance)>rect_w)
                    {
                        auto_wrap=true;

                        if(cell.space)
                        {
                            // 行尾空白不参与折行判定：丢掉该空格并在它之前断行，
                            // 否则"恰好差一个尾随空格"会把本已排好的词推到下一行
                            line.end=pos;
                            ++pos;
                        }
                        else
                        if(word_wrap&&last_space>line.begin)
                        {
                            line.end=last_space;        // 断在空格处(空格丢弃)
                            pos     =last_space+1;
                        }
                        else
                        {
                            line.end=pos;               // 断在当前字符之前
                        }

                        break;
                    }

                    line.width+=cell.advance;
                    line.end   =pos+1;

                    if(cell.visible) ++line.visible;
                    if(cell.space)   last_space=pos;

                    ++pos;
                }

                if(auto_wrap)
                {
                    line.width  =SumWidth(ll.cells,line.begin,line.end);
                    line.visible=CountVisible(ll.cells,line.begin,line.end);

                    if(check_border&&line.end<total)
                    {
                        const int32_t last_vis=LastVisibleIndex(ll.cells,line.begin,line.end);

                        if(last_vis>line.begin
                         &&CountVisible(ll.cells,line.begin,last_vis)>0
                         &&ll.cells[last_vis].cla->attr->end_disable
                         &&FirstVisibleIndex(ll.cells,line.end,total)>=0)
                        {
                            // 行尾禁用符号(如左括号)不能停在行尾 → 回退到下一行
                            line.end=last_vis;
                            pos     =last_vis;
                        }
                        else
                        {
                            const int32_t next_vis=FirstVisibleIndex(ll.cells,line.end,total);

                            // 行首禁用符号(如逗号)不能出现在行首 → 悬挂到当前行末尾
                            if(next_vis>=0
                             &&next_vis==line.end
                             &&ll.cells[next_vis].cla->attr->begin_disable)
                            {
                                line.end=next_vis+1;
                                pos     =line.end;
                            }
                        }

                        line.width  =SumWidth(ll.cells,line.begin,line.end);
                        line.visible=CountVisible(ll.cells,line.begin,line.end);
                    }
                }

                // 保证前进：极端窄矩形下也要吃掉至少一个字符，否则死循环
                if(line.end<=line.begin)
                {
                    line.end=line.begin+1;

                    if(pos<line.end)
                        pos=line.end;

                    line.width  =SumWidth(ll.cells,line.begin,line.end);
                    line.visible=CountVisible(ll.cells,line.begin,line.end);
                }

                ll.lines.Add(line);
            }

            // ── 行数上限：max_lines 与矩形高度取更小者 ──────────────────
            int32_t line_limit=0;                   // 0 = 不限制

            if(st.max_lines>0)
                line_limit=static_cast<int32_t>(st.max_lines);

            if(st.rect_height>0&&ll.line_step>0)
            {
                const int32_t by_height=(st.rect_height<ll.char_height)
                    ?1
                    :1+(st.rect_height-ll.char_height)/ll.line_step;

                if(line_limit==0||by_height<line_limit)
                    line_limit=by_height;
            }

            // Overflow 模式只把矩形用于折行与对齐，不截断内容
            const bool cut=(st.overflow_mode!=TextOverflowMode::Overflow);

            if(cut&&line_limit>0&&ll.lines.GetCount()>line_limit)
            {
                ll.lines.Resize(line_limit);
                ll.truncated=true;
            }

            // ── 省略号：从最后一行尾部回退，腾出省略号宽度 ─────────────
            if(ll.truncated
             &&st.overflow_mode==TextOverflowMode::Ellipsis
             &&!ll.lines.IsEmpty())
            {
                const CLA *ecla=EllipsisCLA(fs);

                if(ecla)
                {
                    const int32_t ell_advance=AdvanceOf(ecla,st,false);

                    LayoutLine &last=ll.lines[ll.lines.GetCount()-1];

                    while(rect_w>0
                        &&last.end>last.begin
                        &&(last.width+ell_advance)>rect_w)
                    {
                        --last.end;
                        last.width  =SumWidth(ll.cells,last.begin,last.end);
                        last.visible=CountVisible(ll.cells,last.begin,last.end);
                    }

                    last.ellipsis=true;
                }
            }

            // ── 内容宽度 ────────────────────────────────────────────────
            const CLA *ecla=EllipsisCLA(fs);
            const int32_t ell_advance=ecla?AdvanceOf(ecla,st,false):0;

            ll.max_width=0;

            for(int32_t i=0;i<ll.lines.GetCount();i++)
            {
                const LayoutLine &line=ll.lines[i];
                const int32_t w=line.width+(line.ellipsis?ell_advance:0);

                if(w>ll.max_width)
                    ll.max_width=w;
            }
        }

        /// 把文本按当前字体度量拆成 cells(不改动图集，供纯测量使用)
        bool BuildCellsFromText(LayoutLines &ll,FontSource *fs,const U16StringView &str,const TextDrawStyle &st)
        {
            if(!fs)
                return(false);

            const int32_t len=str.Length();

            ll.cells.Clear();
            ll.cells.Reserve(len);

            const bool rot_swap=(st.rotation==90||st.rotation==270);
            const u16char *p=str.c_str();

            for(int32_t i=0;i<len;i++)
            {
                const CLA *cla=fs->GetCLA(p[i]);

                if(!cla)
                    return(false);

                LayoutCell cell;
                BuildCell(cell,cla,TileUVFloat(),st,rot_swap);
                ll.cells.Add(cell);
            }

            ll.line_step  =static_cast<int32_t>((fs->GetCharHeight()+st.line_gap+st.extra_advance_y)*st.scale);
            ll.char_height=static_cast<int32_t>(fs->GetCharHeight()*st.scale);

            if(ll.line_step<=0)
                ll.line_step=1;

            BreakLines(ll,fs,st);

            return(true);
        }
    }//namespace

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

        hit_chars.Clear();
        hit_origin_x    =0;
        hit_origin_y    =0;
        hit_line_step   =0;
        hit_char_height =0;
        hit_line_count  =0;
        hit_text_length =0;

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

        // 需要省略号截断的字符串：提前把省略号字形注册进图集。
        // 截断点只能到排版阶段才知道，而图集注册必须在此之前完成，
        // 因此这里按"该批次是否有省略号需求"无条件预留一个字形。
        for(const DrawStringItem &dsi:draw_string_list)
        {
            if(dsi.style.overflow_mode!=TextOverflowMode::Ellipsis)
                continue;

            const CLA *ecla=EllipsisCLA(font_source);

            if(ecla)
                chars_sets.Add(ecla->attr->ch);

            break;
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
        if(!font_source)
            return(0);

        const TextDrawStyle &st=dsi.style;
        const int32_t str_len=dsi.str.length;
        const bool    rot_swap=(st.rotation==90||st.rotation==270);

        // ── 第一趟：采集逐字符度量并切行 ────────────────────────────────
        LayoutLines ll;
        ll.cells.Reserve(str_len);

        {
            CharDrawAttrIt it_cda=dsi.it;

            for(int32_t i=0;i<str_len;i++)
            {
                const CharDrawAttr &cda=*it_cda;

                LayoutCell cell;
                BuildCell(cell,cda.cla,cda.uv,st,rot_swap);
                ll.cells.Add(cell);

                ++it_cda;
            }
        }

        ll.line_step  =static_cast<int32_t>((font_source->GetCharHeight()+st.line_gap+st.extra_advance_y)*st.scale);
        ll.char_height=static_cast<int32_t>(font_source->GetCharHeight()*st.scale);

        if(ll.line_step<=0)
            ll.line_step=1;

        BreakLines(ll,font_source,st);

        // ── 垂直对齐：整体行块在矩形内的纵向偏移 ────────────────────────
        const int32_t line_count=ll.lines.GetCount();
        const int32_t total_height=(line_count>0)
            ?(line_count-1)*ll.line_step+ll.char_height
            :0;

        int32_t voff=0;

        if(st.rect_height>0)
        {
            if(st.v_align==TextVerticalAlign::Center)
                voff=(st.rect_height-total_height)/2;
            else
            if(st.v_align==TextVerticalAlign::Bottom)
                voff=st.rect_height-total_height;

            if(voff<0)                              // 内容高于矩形时以顶部为锚，不向上溢出
                voff=0;
        }

        // ── 第二趟：按对齐摆放并输出实例 ────────────────────────────────
        hit_chars.Clear();
        hit_chars.Reserve(str_len);

        hit_origin_x    =st.start_position.x;
        hit_origin_y    =st.start_position.y;
        hit_line_step   =ll.line_step;
        hit_char_height =ll.char_height;
        hit_line_count  =line_count;
        hit_text_length =str_len;

        const CLA     *ecla     =EllipsisCLA(font_source);
        const int32_t  ell_adv  =ecla?AdvanceOf(ecla,st,false):0;

        int emitted=0;

        for(int32_t li=0;li<line_count;li++)
        {
            const LayoutLine &line=ll.lines[li];

            const int32_t line_width=line.width+(line.ellipsis?ell_adv:0);

            int32_t hoff=0;

            if(st.rect_width>0)
            {
                if(st.para_style.align==TextAlign::Right)
                    hoff=st.rect_width-line_width;
                else
                if(st.para_style.align==TextAlign::Center)
                    hoff=(st.rect_width-line_width)/2;

                if(hoff<0)                          // 行宽超出矩形时以左边缘为锚
                    hoff=0;
            }

            const int32_t pen_y=st.start_position.y+voff+li*ll.line_step;
            int32_t pen_x=st.start_position.x+hoff;

            for(int32_t ci=line.begin;ci<line.end;ci++)
            {
                const LayoutCell &cell=ll.cells[ci];

                // 命中表按"源字符"记录(含空格等不可见字符)，供点击定位与光标绘制
                HitChar hc;
                hc.pen_x     =pen_x;
                hc.pen_y     =pen_y;
                hc.advance   =cell.advance;
                hc.src_index =ci;
                hc.line_index=li;
                hit_chars.Add(hc);

                if(cell.visible)
                {
                    const uint16_t cid=GetOrRegisterCharId(cell.cla->attr->ch,cell.cla->metrics,cell.uv);

                    gpu_char_instances.push_back({static_cast<int16_t>(pen_x),static_cast<int16_t>(pen_y),cid,st.style_id,0});

                    ++emitted;
                }

                pen_x+=cell.advance;
            }

            if(line.ellipsis&&ecla)
            {
                TileUVFloat euv;

                chars_uv.Get(ecla->attr->ch,euv);

                const uint16_t cid=GetOrRegisterCharId(ecla->attr->ch,ecla->metrics,euv);

                gpu_char_instances.push_back({static_cast<int16_t>(pen_x),static_cast<int16_t>(pen_y),cid,st.style_id,0});

                ++emitted;
            }
        }

        // 记录排版后的最终尺寸(内容尺寸，不含矩形留白)
        layout_width_ =ll.max_width;
        layout_height_=total_height;

        return(emitted); //返回绘制的字符数量
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

        // 以实例表长度为准：省略号等合成字符也是真实实例，不能只统计源可见字符
        if((int)gpu_char_instances.size()!=total)
            total=(int)gpu_char_instances.size();

        return(total);
    }

    // ── 风格解析(管线与 GUI 层共用) ─────────────────────────────────
    void BuildDrawStyle(TextDrawStyle &out_style,
                        const ParagraphStyle &para_style,
                        const TEXT_COORD_VEC &start_pos,
                        const int char_height,
                        const uint16_t style_id,
                        const CharStyle *char_style)
    {
        out_style.para_style    =para_style;
        out_style.start_position=start_pos;
        out_style.style_id      =style_id;

        const float origin_char_height=static_cast<float>(char_height);

        out_style.char_height    =static_cast<TEXT_COORD_TYPE>(std::ceil(origin_char_height));
        out_style.space_size     =static_cast<TEXT_COORD_TYPE>(std::ceil(origin_char_height*para_style.space_size));
        out_style.full_space_size=static_cast<TEXT_COORD_TYPE>(std::ceil(origin_char_height*para_style.full_space_size));
        out_style.tab_size       =static_cast<TEXT_COORD_TYPE>(std::ceil(origin_char_height*para_style.tab_size));
        out_style.char_gap       =static_cast<TEXT_COORD_TYPE>(std::ceil(origin_char_height*para_style.char_gap));
        out_style.line_gap       =static_cast<TEXT_COORD_TYPE>(std::ceil(origin_char_height*para_style.line_gap));
        out_style.line_height    =static_cast<TEXT_COORD_TYPE>(std::ceil(origin_char_height+out_style.line_gap));

        // 矩形约束原样透传(单位已是像素)
        out_style.rect_width   =para_style.rect_width;
        out_style.rect_height  =para_style.rect_height;
        out_style.wrap_mode    =para_style.wrap_mode;
        out_style.overflow_mode=para_style.overflow_mode;
        out_style.v_align      =para_style.v_align;
        out_style.max_lines    =para_style.max_lines;

        // bold/outline 使字形视觉尺寸每侧扩展 bold+outline 像素，
        // 因此水平/垂直行间距各增加 2*(bold+outline) 以避免字符重叠
        if(char_style)
        {
            const float extra=2.0f*(char_style->bold_px+char_style->outline_px);

            out_style.extra_advance_x=extra;
            out_style.extra_advance_y=extra;
            out_style.scale          =char_style->scale;
            out_style.rotation       =char_style->rotation;
        }
    }

    // ── 纯测量(不碰图集) ────────────────────────────────────────────
    bool MeasureTextLayout(TextMetrics &out_metrics,
                           FontSource *font_source,
                           const U16StringView &str,
                           const TextDrawStyle &style)
    {
        out_metrics=TextMetrics();

        if(!font_source)
            return(false);

        LayoutLines ll;

        if(!BuildCellsFromText(ll,font_source,str,style))
            return(false);

        const int32_t line_count=ll.lines.GetCount();

        out_metrics.width     =ll.max_width;
        out_metrics.height    =(line_count>0)
            ?(line_count-1)*ll.line_step+ll.char_height
            :0;
        out_metrics.line_count=line_count;
        out_metrics.truncated =ll.truncated;

        return(true);
    }

    // ── 命中测试 / 光标定位(水平排版) ───────────────────────────────
    int TextLayout::GetIndexAtPosition(const TEXT_COORD_VEC &local_pos) const
    {
        if(hit_chars.IsEmpty()||hit_line_count<=0)
            return(-1);

        int32_t line=0;

        if(hit_line_step>0)
            line=(local_pos.y-hit_origin_y)/hit_line_step;

        if(line<0)                      line=0;
        if(line>=hit_line_count)        line=hit_line_count-1;

        int32_t first=-1;
        int32_t last =-1;

        for(int32_t i=0;i<hit_chars.GetCount();i++)
        {
            const HitChar &hc=hit_chars[i];

            if(hc.line_index!=line)
                continue;

            if(first<0)
                first=i;

            last=i;

            if(local_pos.x>=hc.pen_x&&local_pos.x<hc.pen_x+hc.advance)
                return(hc.src_index);
        }

        if(first<0)
            return(-1);

        if(local_pos.x<hit_chars[first].pen_x)
            return(hit_chars[first].src_index);

        // 行尾之后：光标落在该行末字符之后
        return(hit_chars[last].src_index+1);
    }

    bool TextLayout::GetCursorPosition(const int src_index,TEXT_COORD_VEC &out_pos,int &out_height) const
    {
        if(hit_chars.IsEmpty())
            return(false);

        out_height=hit_char_height;

        for(int32_t i=0;i<hit_chars.GetCount();i++)
        {
            const HitChar &hc=hit_chars[i];

            if(hc.src_index==src_index)
            {
                out_pos=TEXT_COORD_VEC(hc.pen_x,hc.pen_y);
                return(true);
            }

            // 源下标落在不可绘制的字符(如 '\n')上：挂到下一个字符的行首笔位
            if(hc.src_index>src_index)
            {
                out_pos=TEXT_COORD_VEC(hc.pen_x,hc.pen_y);
                return(true);
            }
        }

        return(GetEndCursorPosition(out_pos,out_height));
    }

    bool TextLayout::GetEndCursorPosition(TEXT_COORD_VEC &out_pos,int &out_height) const
    {
        if(hit_chars.IsEmpty())
            return(false);

        out_height=hit_char_height;

        const HitChar &last=hit_chars[hit_chars.GetCount()-1];

        out_pos=TEXT_COORD_VEC(last.pen_x+last.advance,last.pen_y);

        return(true);
    }
}//namespace hgl::graph::layout
