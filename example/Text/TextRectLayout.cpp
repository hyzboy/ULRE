#include<hgl/io/LoadString.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/components/TextComponent.h>
#include<hgl/graph/font/FontSource.h>
#include<hgl/graph/font/TextLayout.h>
#include<hgl/graph/font/TextCharSSBO.h>
#include<hgl/framework/WorkManager.h>
#include<hgl/color/Color.h>
#include<hgl/color/Color4ub.h>
#include<hgl/math/Vector.h>
#include<hgl/log/Log.h>

using namespace hgl;
using namespace hgl::ecs;
using namespace hgl::graph;

/**
 * 编辑器 GUI 文本的矩形(RECT)排版示例。
 *
 * 覆盖本批补强的全部能力：
 *   1. 固定宽度自动折行（CJK 标点禁则 / 西文按单词折行）
 *   2. 水平对齐（Left / Center / Right）
 *   3. 垂直对齐（Top / Center / Bottom）
 *   4. 溢出截断 + 行尾省略号
 *   5. 纯测量 MeasureTextLayout（不碰图集，供 GUI 自适应尺寸）
 *   6. 未设置矩形时与旧版自由排版完全一致
 *
 * 注意：本示例的每个文本块只使用一套字体/字号/样式——编辑器 GUI 不做段内混排。
 */
constexpr const uint32_t WINDOW_WIDTH  = 2560;
constexpr const uint32_t WINDOW_HEIGHT = 1440;

constexpr const uint32_t FONT_SIZE     = 32;

constexpr const int BOX_W   = 560;
constexpr const int GAP_X   = 40;
constexpr const int GAP_Y   = 40;

class TextRectLayoutApp:public WorkObject
{
private:

    ECSContext *        ecs_world   = nullptr;
    FontSource *        font        = nullptr;

protected:

    math::RectScope2i MakeRect(const int x,const int y,const int w,const int h)
    {
        return math::RectScope2i(x,y,w,h);
    }

    TextComponent *CreateText(const U16String &text,
                              const math::RectScope2i &rect,
                              const Color4ub &color,
                              const layout::TextWrapMode wrap       = layout::TextWrapMode::None,
                              const layout::TextOverflowMode overflow= layout::TextOverflowMode::Overflow,
                              const layout::TextAlign h_align       = layout::TextAlign::Left,
                              const layout::TextVerticalAlign v_align= layout::TextVerticalAlign::Top,
                              const uint32_t max_lines              = 0)
    {
        auto *entity=ecs_world->CreateEntity();
        if(!entity)
            return(nullptr);

        auto *text_comp=entity->AddComponent<TextComponent>();
        if(!text_comp)
            return(nullptr);

        layout::CharStyle cs{};

        cs.text_color=color.packUnorm4x8();

        text_comp->SetText(text);
        text_comp->SetFontSource(font);
        text_comp->SetCharStyle(cs);

        // 矩形排版：左/上即起始位置，宽/高为内容区约束
        text_comp->SetLayoutRect(rect);

        text_comp->SetTextWrap(wrap);
        text_comp->SetTextOverflow(overflow);
        text_comp->SetHorizontalAlign(h_align);
        text_comp->SetVerticalAlign(v_align);
        text_comp->SetMaxLines(max_lines);

        return(text_comp);
    }

    /// 传统自由排版（不设矩形）：用于对照，行为与补强前一致
    TextComponent *CreateFreeText(const U16String &text,const math::Vector2i &pos,const Color4ub &color)
    {
        auto *entity=ecs_world->CreateEntity();
        if(!entity)
            return(nullptr);

        auto *text_comp=entity->AddComponent<TextComponent>();
        if(!text_comp)
            return(nullptr);

        layout::CharStyle cs{};

        cs.text_color=color.packUnorm4x8();

        text_comp->SetText(text);
        text_comp->SetFontSource(font);
        text_comp->SetStartPosition(pos);
        text_comp->SetCharStyle(cs);

        return(text_comp);
    }

    bool InitTextRenderable()
    {
        ecs_world=GetECSContext();
        if(!ecs_world)
            return(false);

        font=CreateFontSource(OS_TEXT("微软雅黑"),FONT_SIZE);
        if(!font)
            return(false);

        const Color4ub WHITE (255,255,255,255);
        const Color4ub CYAN  (120,220,255,255);
        const Color4ub YELLOW(255,220,120,255);
        const Color4ub GREEN (140,255,160,255);

        const U16String cjk_sample(OS_TEXT("道可道，非常道。名可名，非常名。无名天地之始；有名万物之母。故常无欲，以观其妙；常有欲，以观其徼。此两者，同出而异名，同谓之玄。玄之又玄，众妙之门。"));

        const U16String en_sample(OS_TEXT("The quick brown fox jumps over the lazy dog. Word wrapping prefers breaking at spaces, while averylongunbreakablewordmust fall back to character wrapping."));

        // ── 第 1 列：按字符折行（CJK + 标点禁则）────────────────────
        int y=GAP_Y;

        CreateText(U16String(OS_TEXT("1) Wrap=Char (CJK 禁则)")),MakeRect(GAP_X,y,BOX_W,40),CYAN);
        CreateText(cjk_sample,MakeRect(GAP_X,y+50,BOX_W,300),WHITE,
                   layout::TextWrapMode::Char,
                   layout::TextOverflowMode::Clip);

        // ── 第 2 列：按单词折行（西文）─────────────────────────────
        const int col2=GAP_X+BOX_W+GAP_X;

        CreateText(U16String(OS_TEXT("2) Wrap=Word (西文词边界)")),MakeRect(col2,y,BOX_W,40),CYAN);
        CreateText(en_sample,MakeRect(col2,y+50,BOX_W,300),WHITE,
                   layout::TextWrapMode::Word,
                   layout::TextOverflowMode::Clip);

        // ── 第 3 列：水平对齐 ───────────────────────────────────────
        const int col3=col2+BOX_W+GAP_X;

        CreateText(U16String(OS_TEXT("3) 水平对齐")),MakeRect(col3,y,BOX_W,40),CYAN);

        CreateText(U16String(OS_TEXT("Align = Left")),MakeRect(col3,y+50,BOX_W,60),YELLOW,
                   layout::TextWrapMode::None,
                   layout::TextOverflowMode::Overflow,
                   layout::TextAlign::Left);

        CreateText(U16String(OS_TEXT("Align = Center")),MakeRect(col3,y+120,BOX_W,60),YELLOW,
                   layout::TextWrapMode::None,
                   layout::TextOverflowMode::Overflow,
                   layout::TextAlign::Center);

        CreateText(U16String(OS_TEXT("Align = Right")),MakeRect(col3,y+190,BOX_W,60),YELLOW,
                   layout::TextWrapMode::None,
                   layout::TextOverflowMode::Overflow,
                   layout::TextAlign::Right);

        // ── 第 4 列：垂直对齐 ───────────────────────────────────────
        const int col4=col3+BOX_W+GAP_X;

        CreateText(U16String(OS_TEXT("4) 垂直对齐")),MakeRect(col4,y,BOX_W,40),CYAN);

        CreateText(U16String(OS_TEXT("VAlign = Top")),MakeRect(col4,y+50,BOX_W,100),GREEN,
                   layout::TextWrapMode::None,
                   layout::TextOverflowMode::Clip,
                   layout::TextAlign::Left,
                   layout::TextVerticalAlign::Top);

        CreateText(U16String(OS_TEXT("VAlign = Center")),MakeRect(col4,y+160,BOX_W,100),GREEN,
                   layout::TextWrapMode::None,
                   layout::TextOverflowMode::Clip,
                   layout::TextAlign::Center,
                   layout::TextVerticalAlign::Center);

        CreateText(U16String(OS_TEXT("VAlign = Bottom")),MakeRect(col4,y+270,BOX_W,100),GREEN,
                   layout::TextWrapMode::None,
                   layout::TextOverflowMode::Clip,
                   layout::TextAlign::Right,
                   layout::TextVerticalAlign::Bottom);

        // ── 底部：省略号截断 / 自由排版对照 ─────────────────────────
        const int bottom=GAP_Y+400;

        CreateText(U16String(OS_TEXT("5) Overflow=Ellipsis, MaxLines=2")),MakeRect(GAP_X,bottom,BOX_W*2+GAP_X,40),CYAN);
        CreateText(cjk_sample,MakeRect(GAP_X,bottom+50,BOX_W*2+GAP_X,110),WHITE,
                   layout::TextWrapMode::Char,
                   layout::TextOverflowMode::Ellipsis,
                   layout::TextAlign::Left,
                   layout::TextVerticalAlign::Top,
                   2);

        CreateText(U16String(OS_TEXT("6) 未设置矩形 = 旧版自由排版")),MakeRect(GAP_X,bottom+190,BOX_W*2+GAP_X,40),CYAN);
        CreateFreeText(U16String(OS_TEXT("Legacy free layout: no rect, no wrap, single line.")),
                       math::Vector2i(GAP_X,bottom+240),YELLOW);

        // ── 纯测量：与绘制共用同一套折行规则 ────────────────────────
        {
            layout::ParagraphStyle para;

            para.align       = layout::TextAlign::Left;
            para.wrap_mode   = layout::TextWrapMode::Char;
            para.rect_width  = BOX_W;
            para.rect_height = 300;
            para.overflow_mode=layout::TextOverflowMode::Clip;

            layout::TextDrawStyle draw_style;

            layout::BuildDrawStyle(draw_style,para,math::Vector2i(0,0),font->GetCharHeight());

            layout::TextMetrics metrics;

            if(layout::MeasureTextLayout(metrics,font,
                                         U16StringView(cjk_sample.c_str(),cjk_sample.Length()),
                                         draw_style))
            {
                GLogInfo("[TextRect] measure: %d x %d px, lines=%d truncated=%d",
                         metrics.width,metrics.height,metrics.line_count,metrics.truncated);

                const U16String info=U16String::numberOf(metrics.width)
                                    +U16String(OS_TEXT(" x "))
                                    +U16String::numberOf(metrics.height)
                                    +U16String(OS_TEXT(" px, lines="))
                                    +U16String::numberOf(metrics.line_count);

                CreateText(U16String(OS_TEXT("Measure => "))+info,
                           MakeRect(GAP_X,bottom+300,BOX_W*2+GAP_X,40),GREEN);
            }
            else
                GLogError("[TextRect] measure failed");
        }

        return(true);
    }

public:

    bool Init() override
    {
        SetClearColor(Color4f(0.15f,0.15f,0.18f,1.0f));

        if(!InitTextRenderable())
            return(false);

        return(true);
    }
};//class TextRectLayoutApp:public WorkObject

int os_main(int argc,os_char **argv)
{
    return RunFramework<TextRectLayoutApp>(OS_TEXT("TextRectLayout"),argc,argv,WINDOW_WIDTH,WINDOW_HEIGHT);
}
