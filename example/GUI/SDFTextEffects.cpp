#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/components/TextComponent.h>
#include<hgl/graph/font/FontSource.h>
#include<hgl/graph/font/TextLayout.h>
#include<hgl/graph/font/TextCharSSBO.h>
#include<hgl/framework/WorkManager.h>
#include<hgl/color/Color.h>
#include<hgl/color/Color4ub.h>

using namespace hgl;
using namespace hgl::ecs;
using namespace hgl::graph;

constexpr const uint32_t WINDOW_WIDTH   = 2560;
constexpr const uint32_t WINDOW_HEIGHT  = 1440;

constexpr const uint32_t FONT_SIZE      = 48;     ///<独立字号，避开其它示例共享的数据源缓存
constexpr const int      START_X        = 60;
constexpr const int      START_Y        = 60;
constexpr const int      LINE_GAP       = 70;     ///<每行垂直间隔(含特效溢出余量)

class TestApp:public WorkObject
{
private:

    ECSContext *        ecs_world           = nullptr;

protected:

    bool CreateTextLine(FontSource *fs, const hgl::graph::layout::CharStyle &cs, const U16String &text, int line_index)
    {
        auto* entity = ecs_world->CreateEntity();
        if(!entity)
            return(false);

        auto text_comp = entity->AddComponent<TextComponent>();
        if(!text_comp)
            return(false);

        text_comp->SetText(text);
        text_comp->SetFontSource(fs);
        text_comp->SetStartPosition({START_X, START_Y + line_index * LINE_GAP});
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

        hgl::graph::layout::CharStyle cs;

        // 行1 普通：白色，无特效(对照组)
        cs = {};
        cs.text_color = WHITE.toRGBA8();
        if(!CreateTextLine(fs, cs, U16String(OS_TEXT("SDF 普通文本 Normal")), 0))
            return(false);

        // 行2 加粗：bold = 2.0 像素
        cs = {};
        cs.text_color = WHITE.toRGBA8();
        cs.bold_px = 2.0f;
        if(!CreateTextLine(fs, cs, U16String(OS_TEXT("SDF 加粗文本 Bold 2px")), 1))
            return(false);

        // 行3 勾边：outline = 2.0 像素，勾边红色，字身白色
        cs = {};
        cs.text_color = WHITE.toRGBA8();
        cs.outline_px = 2.0f;
        cs.outline_color = RED.toRGBA8();
        if(!CreateTextLine(fs, cs, U16String(OS_TEXT("SDF 勾边文本 Outline 2px")), 2))
            return(false);

        // 行4 阴影：默认黑色阴影，向右下偏移字号 1/10
        cs = {};
        cs.text_color = WHITE.toRGBA8();
        cs.flags = 1;
        if(!CreateTextLine(fs, cs, U16String(OS_TEXT("SDF 阴影文本 Shadow")), 3))
            return(false);

        // 行5 叠加：加粗 + 勾边 + 阴影全开
        cs = {};
        cs.text_color = WHITE.toRGBA8();
        cs.bold_px = 2.0f;
        cs.outline_px = 2.0f;
        cs.outline_color = RED.toRGBA8();
        cs.flags = 1;
        if(!CreateTextLine(fs, cs, U16String(OS_TEXT("SDF 叠加文本 Bold+Outline+Shadow")), 4))
            return(false);

        // 行6 对照：再来一行普通白色长文本
        cs = {};
        cs.text_color = WHITE.toRGBA8();
        if(!CreateTextLine(fs, cs, U16String(OS_TEXT("SDF 对照文本：道可道，非常道；名可名，非常名。")), 5))
            return(false);

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
