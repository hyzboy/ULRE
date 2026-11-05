// 矩形(RECT)排版回归测试：折行 / 对齐 / 截断 / 纯测量 / TextComponent 接口。
//
// 只走 CPU 路径（MeasureTextLayout + 组件 setter），不建 Vulkan 设备、不碰字形图集，
// 因此可在 CI 或本地控制台直接跑：退出码 0 表示全部通过。

#include <hgl/graph/font/FontSource.h>
#include <hgl/graph/font/TextLayout.h>
#include <hgl/ecs/components/TextComponent.h>
#include <hgl/log/Log.h>
#include <hgl/log/Logger.h>

using namespace hgl;
using namespace hgl::graph;

namespace
{
    constexpr const os_char *TEST_FONT = OS_TEXT("微软雅黑");
    constexpr const uint32_t TEST_SIZE = 32;

    bool Measure(layout::TextMetrics &metrics,
                 FontSource *fs,
                 const U16String &text,
                 const layout::ParagraphStyle &para)
    {
        layout::TextDrawStyle style;

        layout::BuildDrawStyle(style,para,math::Vector2i(0,0),fs->GetCharHeight());

        return layout::MeasureTextLayout(metrics,fs,
                                         U16StringView(text.c_str(),text.Length()),
                                         style);
    }
}//namespace

int main(int argc,char **argv)
{
    (void)argc;
    (void)argv;

    FontSource *fs=CreateFontSource(TEST_FONT,TEST_SIZE);

    if(!fs)
    {
        GLogError(u8"Test 1 Failed: 无法创建字体源（缺字体或无字体后端）");
        return 1;
    }

    const int char_height=fs->GetCharHeight();

    const U16String cjk(OS_TEXT("道可道，非常道。名可名，非常名。无名天地之始；有名万物之母。"));
    const U16String ascii(OS_TEXT("aaaa bbbb cccc"));
    const U16String mixed(OS_TEXT("Hello，世界！"));

    // ── 1. 无矩形 = 旧版自由排版 ────────────────────────────────────
    layout::TextMetrics base;

    {
        layout::ParagraphStyle para;

        if(!Measure(base,fs,ascii,para))
        {
            GLogError(u8"Test 1 Failed: 测量失败");
            return 1;
        }

        if(base.line_count!=1||base.width<=0||base.height<=0||base.truncated)
        {
            GLogError(u8"Test 1 Failed: 自由排版应为单行不截断，实得 lines=%d w=%d h=%d trunc=%d",
                      base.line_count,base.width,base.height,(int)base.truncated);
            return 1;
        }

        GLogInfo(u8"Test 1 Passed: 自由排版 %d x %d px, lines=%d",base.width,base.height,base.line_count);
    }

    // ── 2. 显式矩形宽度 + 按字符折行 ────────────────────────────────
    {
        layout::ParagraphStyle para;

        para.rect_width =200;
        para.wrap_mode  =layout::TextWrapMode::Char;

        layout::TextMetrics m;

        if(!Measure(m,fs,cjk,para))
        {
            GLogError(u8"Test 2 Failed: 测量失败");
            return 2;
        }

        // 允许禁则悬挂一个字符的溢出
        const int bound=para.rect_width+char_height;

        if(m.line_count<2||m.width>bound||m.truncated)
        {
            GLogError(u8"Test 2 Failed: 期望多行且行宽<=%d，实得 lines=%d w=%d trunc=%d",
                      bound,m.line_count,m.width,(int)m.truncated);
            return 2;
        }

        GLogInfo(u8"Test 2 Passed: 按字符折行 %d 行, 内容宽 %d px (矩形 %d)",
                 m.line_count,m.width,para.rect_width);
    }

    // ── 3. 按单词折行：断点必须落在空格处 ───────────────────────────
    {
        layout::ParagraphStyle no_rect;
        layout::TextMetrics full;

        const U16String two_words(OS_TEXT("aaaa bbbb"));

        Measure(full,fs,two_words,no_rect);

        layout::ParagraphStyle para;

        para.rect_width=full.width+char_height;    // 够放下两词与后续空格，但放不下第三个词
        para.wrap_mode =layout::TextWrapMode::Word;

        layout::TextMetrics word_m;

        if(!Measure(word_m,fs,ascii,para))
        {
            GLogError(u8"Test 3 Failed: 测量失败");
            return 3;
        }

        if(word_m.line_count!=2||word_m.width!=full.width)
        {
            GLogError(u8"Test 3 Failed: 期望断在空格(2 行, 行宽=%d)，实得 lines=%d w=%d",
                      full.width,word_m.line_count,word_m.width);
            return 3;
        }

        // 同样的矩形用字符折行必然填得更满 —— 证明 word/char 是两条不同规则
        para.wrap_mode=layout::TextWrapMode::Char;

        layout::TextMetrics char_m;

        if(!Measure(char_m,fs,ascii,para))
        {
            GLogError(u8"Test 3 Failed: 字符折行测量失败");
            return 3;
        }

        if(char_m.width<=word_m.width)
        {
            GLogError(u8"Test 3 Failed: 字符折行行宽 %d 应大于单词折行 %d",
                      char_m.width,word_m.width);
            return 3;
        }

        GLogInfo(u8"Test 3 Passed: 单词折行 %d 行/%d px，字符折行 %d 行/%d px",
                 word_m.line_count,word_m.width,char_m.line_count,char_m.width);
    }

    // ── 4. 最大行数 + 省略号 ────────────────────────────────────────
    {
        layout::ParagraphStyle para;

        para.rect_width   =200;
        para.wrap_mode    =layout::TextWrapMode::Char;
        para.overflow_mode=layout::TextOverflowMode::Ellipsis;
        para.max_lines    =1;

        layout::TextMetrics m;

        if(!Measure(m,fs,cjk,para))
        {
            GLogError(u8"Test 4 Failed: 测量失败");
            return 4;
        }

        if(!m.truncated||m.line_count!=1||m.width>para.rect_width)
        {
            GLogError(u8"Test 4 Failed: 期望 1 行截断且宽<=%d，实得 lines=%d w=%d trunc=%d",
                      para.rect_width,m.line_count,m.width,(int)m.truncated);
            return 4;
        }

        GLogInfo(u8"Test 4 Passed: 省略号截断 1 行, 宽 %d px (矩形 %d)",m.width,para.rect_width);
    }

    // ── 5. 矩形高度裁剪（Clip）与 Overflow 的差别 ───────────────────
    {
        const U16String four_lines(OS_TEXT("L1\nL2\nL3\nL4"));

        layout::ParagraphStyle clip;

        clip.wrap_mode    =layout::TextWrapMode::None;
        clip.overflow_mode=layout::TextOverflowMode::Clip;
        clip.rect_height  =2*char_height;       // 只放得下两行

        layout::TextMetrics clip_m;

        if(!Measure(clip_m,fs,four_lines,clip))
        {
            GLogError(u8"Test 5 Failed: 测量失败");
            return 5;
        }

        layout::ParagraphStyle overflow;
        overflow.overflow_mode=layout::TextOverflowMode::Overflow;
        overflow.rect_height  =2*char_height;

        layout::TextMetrics over_m;

        if(!Measure(over_m,fs,four_lines,overflow))
        {
            GLogError(u8"Test 5 Failed: 测量失败");
            return 5;
        }

        if(!clip_m.truncated||clip_m.line_count>2)
        {
            GLogError(u8"Test 5 Failed: Clip 应截断到 2 行，实得 lines=%d trunc=%d",
                      clip_m.line_count,(int)clip_m.truncated);
            return 5;
        }

        if(over_m.truncated||over_m.line_count!=4)
        {
            GLogError(u8"Test 5 Failed: Overflow 不应截断，实得 lines=%d trunc=%d",
                      over_m.line_count,(int)over_m.truncated);
            return 5;
        }

        GLogInfo(u8"Test 5 Passed: Clip=%d 行(截断)，Overflow=%d 行(不截断)",
                 clip_m.line_count,over_m.line_count);
    }

    // ── 6. TextComponent 矩形接口往返 ───────────────────────────────
    {
        hgl::ecs::TextComponent comp;

        const math::RectScope2i rect(10,20,300,120);

        comp.SetLayoutRect(rect);
        comp.SetTextWrap(layout::TextWrapMode::Word);
        comp.SetTextOverflow(layout::TextOverflowMode::Ellipsis);
        comp.SetMaxLines(3);
        comp.SetHorizontalAlign(layout::TextAlign::Center);
        comp.SetVerticalAlign(layout::TextVerticalAlign::Bottom);

        const math::RectScope2i got=comp.GetLayoutRect();

        if(got.GetLeft()!=rect.GetLeft()
         ||got.GetTop()!=rect.GetTop()
         ||got.GetWidth()!=rect.GetWidth()
         ||got.GetHeight()!=rect.GetHeight())
        {
            GLogError(u8"Test 6 Failed: LayoutRect 往返不一致 (%d,%d,%d,%d)",
                      got.GetLeft(),got.GetTop(),got.GetWidth(),got.GetHeight());
            return 6;
        }

        if(comp.GetStartPosition().x!=rect.GetLeft()||comp.GetStartPosition().y!=rect.GetTop())
        {
            GLogError(u8"Test 6 Failed: 起始位置未同步到矩形左上角");
            return 6;
        }

        if(comp.GetTextWrap()!=layout::TextWrapMode::Word
         ||comp.GetTextOverflow()!=layout::TextOverflowMode::Ellipsis
         ||comp.GetMaxLines()!=3
         ||comp.GetHorizontalAlign()!=layout::TextAlign::Center
         ||comp.GetVerticalAlign()!=layout::TextVerticalAlign::Bottom)
        {
            GLogError(u8"Test 6 Failed: 排版属性往返不一致");
            return 6;
        }

        if(comp.GetChangeMask()==0)
        {
            GLogError(u8"Test 6 Failed: 设置排版属性后未置变更位");
            return 6;
        }

        GLogInfo(u8"Test 6 Passed: TextComponent 矩形与排版属性往返正常");
    }

    // ── 7. Wrap=None 与矩形宽度：不自动折行，以及宽度截断测试 ────────
    {
        // 7a: Wrap=None, Overflow=Overflow: 超宽单行绝不折行
        layout::ParagraphStyle no_wrap_over;
        no_wrap_over.rect_width    = 100;
        no_wrap_over.wrap_mode     = layout::TextWrapMode::None;
        no_wrap_over.overflow_mode = layout::TextOverflowMode::Overflow;

        layout::TextMetrics m_over;
        if (!Measure(m_over, fs, cjk, no_wrap_over))
        {
            GLogError(u8"Test 7a Failed: 测量失败");
            return 7;
        }

        if (m_over.line_count != 1 || m_over.width <= 100 || m_over.truncated)
        {
            GLogError(u8"Test 7a Failed: Wrap=None 应保持单行且不截断，实得 lines=%d w=%d trunc=%d",
                      m_over.line_count, m_over.width, (int)m_over.truncated);
            return 7;
        }

        // 7b: Wrap=None, Overflow=Clip: 单行超宽按 rect_width 截断
        layout::ParagraphStyle no_wrap_clip;
        no_wrap_clip.rect_width    = 150;
        no_wrap_clip.wrap_mode     = layout::TextWrapMode::None;
        no_wrap_clip.overflow_mode = layout::TextOverflowMode::Clip;

        layout::TextMetrics m_clip;
        if (!Measure(m_clip, fs, cjk, no_wrap_clip))
        {
            GLogError(u8"Test 7b Failed: 测量失败");
            return 7;
        }

        if (m_clip.line_count != 1 || m_clip.width > 150 || !m_clip.truncated)
        {
            GLogError(u8"Test 7b Failed: Wrap=None,Clip 应单行截断至 <=150px，实得 lines=%d w=%d trunc=%d",
                      m_clip.line_count, m_clip.width, (int)m_clip.truncated);
            return 7;
        }

        // 7c: Wrap=None, Overflow=Ellipsis: 单行超宽在行尾出省略号
        layout::ParagraphStyle no_wrap_ell;
        no_wrap_ell.rect_width    = 150;
        no_wrap_ell.wrap_mode     = layout::TextWrapMode::None;
        no_wrap_ell.overflow_mode = layout::TextOverflowMode::Ellipsis;

        layout::TextMetrics m_ell;
        if (!Measure(m_ell, fs, cjk, no_wrap_ell))
        {
            GLogError(u8"Test 7c Failed: 测量失败");
            return 7;
        }

        if (m_ell.line_count != 1 || m_ell.width > 150 || !m_ell.truncated)
        {
            GLogError(u8"Test 7c Failed: Wrap=None,Ellipsis 应单行截断至 <=150px，实得 lines=%d w=%d trunc=%d",
                      m_ell.line_count, m_ell.width, (int)m_ell.truncated);
            return 7;
        }

        // 7d: 验证 CharStyle 默认无参构造所有字段全部为 0/默认值，以及 BuildDrawStyle 解析
        layout::CharStyle default_cs;
        if (default_cs.italic != 0.0f
         || default_cs.bold_px != 0.0f
         || default_cs.outline_px != 0.0f
         || default_cs.scale != 1.0f
         || default_cs.rotation != 0
         || default_cs.flags != 0
         || default_cs.shadow_uv_offset != 0)
        {
            GLogError(u8"Test 7d Failed: CharStyle 默认成员未正确初始化");
            return 7;
        }

        layout::TextDrawStyle ds;
        layout::BuildDrawStyle(ds, no_wrap_over, math::Vector2i(50, 100), char_height, 0, &default_cs);
        if (ds.extra_advance_x != 0.0f || ds.extra_advance_y != 0.0f || ds.scale != 1.0f || ds.rotation != 0)
        {
            GLogError(u8"Test 7d Failed: BuildDrawStyle 从默认 CharStyle 解析出非零 extra 或异常 scale");
            return 7;
        }

        GLogInfo(u8"Test 7 Passed: Wrap=None 行为与 CharStyle 默认初始化及样式解析均正常");
    }

    // ── 8. 纯色 TILE 度量：scale 补偿 / offset_y=基线 ───────────────
    {
        // scale=2 时：屏幕想要 100x6 px ⇒ CPU 度量应为 50x3，绘制时 *2 还原
        CharMetricsInfo m;

        layout::SolidQuadMetrics(m,100,6,2.0f,char_height);

        if(m.w!=50||m.h!=3)
        {
            GLogError(u8"Test 8 Failed: scale 补偿度量错误 w=%d h=%d (期望 50/3)",m.w,m.h);
            return 8;
        }

        // offset_x=0；offset_y=基字高(不随 scale 变)，使 tile 顶边对齐行起点
        if(m.x!=0||m.y!=char_height)
        {
            GLogError(u8"Test 8 Failed: 偏移错误 x=%d y=%d (期望 0/%d)",m.x,m.y,char_height);
            return 8;
        }

        // 极细线：屏幕 1px、scale=2 → round(0.5) 至少保留 1
        CharMetricsInfo thin;

        layout::SolidQuadMetrics(thin,1,1,2.0f,char_height);

        if(thin.w<1||thin.h<1)
        {
            GLogError(u8"Test 8 Failed: 极细线被清零 w=%d h=%d",thin.w,thin.h);
            return 8;
        }
    }

    // ── 9. 装饰线几何求解：默认比例 / 显式比例 / 空宽度 ──────────────
    {
        // 9a: 空行宽不画
        math::Vector2i dpos; uint32_t dw,dh;

        if(layout::ResolveTextDecoration(dpos,dw,dh,100,100,50,char_height,0,0,0.06f,0.92f))
        {
            GLogError(u8"Test 9a Failed: 零宽度不应生成装饰线");
            return 9;
        }

        // 9b: 默认(0)→用传入默认比例: th=round(0.06*32)=2, pos=round(0.92*32)=29
        if(!layout::ResolveTextDecoration(dpos,dw,dh,10,210,100,char_height,0,0,0.06f,0.92f))
        {
            GLogError(u8"Test 9b Failed: 默认装饰线求解失败");
            return 9;
        }

        if(dw!=200||dh!=2||dpos.x!=10||dpos.y!=100+29)
        {
            GLogError(u8"Test 9b Failed: 默认几何 x=%d y=%d w=%d h=%d (期望 10/129/200/2)",
                      dpos.x,dpos.y,dw,dh);
            return 9;
        }

        // 9c: 显式比例覆盖默认: th=0.5*32=16, pos=0.5*32=16
        if(!layout::ResolveTextDecoration(dpos,dw,dh,0,100,0,char_height,0.5f,0.5f,0.06f,0.92f))
        {
            GLogError(u8"Test 9c Failed: 显式装饰线求解失败");
            return 9;
        }

        if(dw!=100||dh!=16||dpos.y!=16)
        {
            GLogError(u8"Test 9c Failed: 显式比例几何 y=%d h=%d (期望 16/16)",dpos.y,dh);
            return 9;
        }

        GLogInfo(u8"Test 8-9 Passed: 纯色 TILE 度量与装饰线几何求解正常");
    }

    // ── 10. TextComponent 装饰线接口往返 ────────────────────────────
    {
        hgl::ecs::TextComponent comp;

        comp.SetUnderline(true);
        comp.SetStrikethrough(true);
        comp.SetUnderlineStyle(0.08f,0.90f);
        comp.SetStrikethroughStyle(0.06f,0.48f);

        const auto &ps=comp.GetParagraphStyle();

        if(!ps.underline||!ps.strikethrough)
        {
            GLogError(u8"Test 10 Failed: 装饰线开关未写入段落样式");
            return 10;
        }

        if(ps.underline_thickness!=0.08f||ps.underline_position!=0.90f
         ||ps.strikethrough_thickness!=0.06f||ps.strikethrough_position!=0.48f)
        {
            GLogError(u8"Test 10 Failed: 装饰线比例往返不一致");
            return 10;
        }

        if(comp.GetChangeMask()==0)
        {
            GLogError(u8"Test 10 Failed: 设置装饰线后未置变更位");
            return 10;
        }

        comp.SetUnderline(false);

        if(comp.GetParagraphStyle().underline)
        {
            GLogError(u8"Test 10 Failed: 关闭下划线无效");
            return 10;
        }

        GLogInfo(u8"Test 10 Passed: TextComponent 装饰线接口往返正常");
    }

    // ── 11. TextComponent 字符级装饰区间往返 ────────────────────────
    {
        hgl::ecs::TextComponent comp;

        comp.SetText(U16String(OS_TEXT("Hello World!")));
        comp.AddUnderline(0,5);           // "Hello"
        comp.AddStrikethrough(6,5);       // "World"

        const auto &spans=comp.GetDecorationSpans();

        if(spans.GetCount()!=2)
        {
            GLogError(u8"Test 11 Failed: 装饰区间数量错误 count=%d (期望 2)",spans.GetCount());
            return 11;
        }

        if(spans[0].start!=0||spans[0].count!=5||spans[0].type!=layout::TextDecorationType::Underline)
        {
            GLogError(u8"Test 11 Failed: 第 0 个区间不匹配 (start=%d count=%d)",spans[0].start,spans[0].count);
            return 11;
        }

        if(spans[1].start!=6||spans[1].count!=5||spans[1].type!=layout::TextDecorationType::Strikethrough)
        {
            GLogError(u8"Test 11 Failed: 第 1 个区间不匹配 (start=%d count=%d)",spans[1].start,spans[1].count);
            return 11;
        }

        comp.ClearDecorations();

        if(comp.GetDecorationSpans().GetCount()!=0)
        {
            GLogError(u8"Test 11 Failed: 清空装饰区间失败");
            return 11;
        }

        GLogInfo(u8"Test 11 Passed: TextComponent 字符级装饰区间往返正常");
    }

    GLogInfo(u8"=== All TextRectLayout tests passed successfully! ===");
    return 0;
}
