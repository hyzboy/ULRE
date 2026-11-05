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

    GLogInfo(u8"=== All TextRectLayout tests passed successfully! ===");
    return 0;
}
