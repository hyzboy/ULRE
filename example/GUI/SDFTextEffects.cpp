#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/components/TextComponent.h>
#include<hgl/graph/font/FontSource.h>
#include<hgl/graph/font/TextLayout.h>
#include<hgl/graph/font/TextCharSSBO.h>
#include<hgl/graph/geo/GeometryCreater.h>
#include<hgl/framework/WorkManager.h>
#include<hgl/color/Color.h>
#include<hgl/color/Color4ub.h>
#include<hgl/math/Vector.h>

using namespace hgl;
using namespace hgl::ecs;
using namespace hgl::graph;

constexpr const uint32_t WINDOW_WIDTH   = 2560;
constexpr const uint32_t WINDOW_HEIGHT  = 1440;

constexpr const uint32_t FONT_SIZE      = 48;     ///<独立字号，避开其它示例共享的数据源缓存
constexpr const int      START_X        = 60;
constexpr const int      START_Y        = 60;

class TestApp:public WorkObject
{
private:

    ECSContext *        ecs_world           = nullptr;

protected:

    bool CreateTextLine(FontSource *fs, const hgl::graph::layout::CharStyle &cs, const U16String &text, const math::Vector2i &position)
    {
        auto* entity = ecs_world->CreateEntity();
        if(!entity)
            return(false);

        auto text_comp = entity->AddComponent<TextComponent>();
        if(!text_comp)
            return(false);

        text_comp->SetText(text);
        text_comp->SetFontSource(fs);
        text_comp->SetStartPosition(position);
        text_comp->SetCharStyle(cs);

        return(true);
    }

    bool InitTextRenderable()
    {
        ecs_world = GetECSContext();
        if(!ecs_world)
            return(false);

        // Create font source (微软雅黑支持中文，字号48独立于其它示例)
        FontSource *fs = CreateFontSource(OS_TEXT("微软雅黑"), FONT_SIZE);
        if(!fs)
            return(false);

        // 必须在任何文本排版之前启用 SDF 距离场
        fs->SetSDFEnabled(true);
        if(!fs->IsSDFEnabled())
            return(false);

        const Color4ub WHITE(255, 255, 255, 255);
        const Color4ub RED  (255,   0,   0, 255);
        const Color4ub BLUE (  0,   0, 255, 255);

        hgl::graph::layout::CharStyle cs;
        math::Vector2i pos = {START_X, START_Y};

        uint line_gap=FONT_SIZE*1.4;  // 行间距

        // 行1 普通：白色，无特效(对照组)
        cs = layout::CharStyle{};
        cs.text_color = WHITE.packUnorm4x8();
        if(!CreateTextLine(fs, cs, U16String(OS_TEXT("普通文本 (48px)")), pos))
            return(false);

        pos.y += line_gap;

        // 行2 加粗：bold = 3.0 像素
        cs = layout::CharStyle{};
        cs.text_color = WHITE.packUnorm4x8();
        cs.bold_px = 3.0f;
        if(!CreateTextLine(fs, cs, U16String(OS_TEXT("加粗 Bold (3px)")), pos))
            return(false);

        pos.y += line_gap;

        // 行3 勾边：outline = 3.0 像素，勾边红色，字身白色
        cs = layout::CharStyle{};
        cs.text_color = WHITE.packUnorm4x8();
        cs.outline_px = 3.0f;
        cs.outline_color = RED.packUnorm4x8();
        if(!CreateTextLine(fs, cs, U16String(OS_TEXT("勾边 Outline (3px)")), pos))
            return(false);

        pos.y += line_gap;

        // 行4 阴影：灰色阴影，向右下偏移(2,2)
        cs = layout::CharStyle{};
        cs.text_color = WHITE.packUnorm4x8();
        cs.shadow_color = Color4ub(64, 64, 64, 128).packUnorm4x8();
        cs.flags = 1;  // shadow enabled
        cs.shadow_uv_offset = hgl::graph::packHalf2x16(2.0f, 2.0f);
        if(!CreateTextLine(fs, cs, U16String(OS_TEXT("阴影 Shadow (2,2)")), pos))
            return(false);

        pos.y += line_gap;

        // 行5 叠加：加粗 + 勾边 + 阴影全开
        cs = layout::CharStyle{};
        cs.text_color = WHITE.packUnorm4x8();
        cs.bold_px = 2.0f;
        cs.outline_px = 2.0f;
        cs.outline_color = BLUE.packUnorm4x8();
        cs.shadow_color = Color4ub(0, 0, 0, 96).packUnorm4x8();
        cs.flags = 1;
        cs.shadow_uv_offset = hgl::graph::packHalf2x16(3.0f, 3.0f);
        if(!CreateTextLine(fs, cs, U16String(OS_TEXT("叠加 Bold+Outline+Shadow")), pos))
            return(false);

        pos.y += line_gap;

        // 行6 对照：再来一行普通白色长文本
        cs = layout::CharStyle{};
        cs.text_color = WHITE.packUnorm4x8();
        if(!CreateTextLine(fs, cs, U16String(OS_TEXT("对照: 无特效 (Bitmap)")), pos))
            return(false);

        // --- 放大测试：展示 SDF 缩放功能 ---
        pos.y += line_gap;

        cs = layout::CharStyle{};
        cs.text_color = WHITE.packUnorm4x8();
        cs.scale = 1.25f;
        if(!CreateTextLine(fs, cs, U16String(OS_TEXT("SDF 1.25x Scale 缩放测试")), pos))
            return(false);

        pos.y += line_gap*1.25;

        cs = layout::CharStyle{};
        cs.text_color = WHITE.packUnorm4x8();
        cs.scale = 1.5f;
        if(!CreateTextLine(fs, cs, U16String(OS_TEXT("SDF 1.5x Scale 缩放测试")), pos))
            return(false);

        pos.y += line_gap*1.5;

        cs = layout::CharStyle{};
        cs.text_color = WHITE.packUnorm4x8();
        cs.scale = 2.0f;
        cs.outline_px = 1.0f;
        cs.outline_color = RED.packUnorm4x8();
        if(!CreateTextLine(fs, cs, U16String(OS_TEXT("SDF 2x + Outline 缩放测试")), pos))
            return(false);

        pos.y += line_gap*2;

        // 字符旋转（90/180/270）为 CharStyle 正式功能，竖排旋转见独立示例：TextVertical（example/GUI/TextVertical.cpp）

        // 多 FontSource 互不影响演示见独立示例：MultiFontSource（example/GUI/MultiFontSource.cpp）

        return(true);
    }

public:
    bool Init() override
    {
        SetClearColor(Color4f(0.3f, 0.3f, 0.3f, 1.0f));

        if(!InitTextRenderable())
            return(false);

        return(true);
    }
};//class TestApp:public WorkObject

int os_main(int argc, os_char **argv)
{
    return RunFramework<TestApp>(OS_TEXT("SDFTextEffects_ECS"), argc, argv, WINDOW_WIDTH, WINDOW_HEIGHT);
}
