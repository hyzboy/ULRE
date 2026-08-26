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

/**
 * 多 FontSource 演示：多个字体源同屏独立渲染、互不影响。
 *
 * 引擎不提供多 FontSource 合并渲染——每个 FontSource 拥有独立的
 * TileFont/TileData 图集、材质描述符集（PerObject/Material MP）与
 * 三层 SSBO，TextRenderPipeline 按 FontSource 分组，各自一次 draw。
 * 本示例验证：
 *   1. 不同字号（36/72/48）字体源共存，图集尺寸互不相同
 *   2. SDF 路径（fs1/fs2）与原始位图路径（fs3）共存
 *   3. 各字体源样式（颜色/勾边/加粗）互不影响
 */
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

        const Color4ub WHITE(255, 255, 255, 255);
        const Color4ub RED  (255,   0,   0, 255);
        const Color4ub BLUE (  0,   0, 255, 255);
        const Color4ub GREEN(  0, 200,   0, 255);

        hgl::graph::layout::CharStyle cs;
        math::Vector2i pos = {60, 60};

        // ===== 字体源1：微软雅黑 36 号（SDF 距离场路径）=====
        FontSource *fs1 = CreateFontSource(OS_TEXT("微软雅黑"), 36);
        if(!fs1)
            return(false);

        // 必须在任何文本排版之前启用 SDF 距离场
        fs1->SetSDFEnabled(true);
        if(!fs1->IsSDFEnabled())
            return(false);

        cs = layout::CharStyle{};
        cs.text_color = WHITE.toRGBA8();
        if(!CreateTextLine(fs1, cs, U16String(OS_TEXT("字体源1: 微软雅黑 36px SDF (普通)")), pos))
            return(false);

        pos.y += 36*1.4;

        cs = layout::CharStyle{};
        cs.text_color = BLUE.toRGBA8();
        cs.outline_px = 2.0f;
        cs.outline_color = WHITE.toRGBA8();
        if(!CreateTextLine(fs1, cs, U16String(OS_TEXT("字体源1: 36px 蓝字白边 (Outline)")), pos))
            return(false);

        // ===== 字体源2：微软雅黑 72 号（SDF 距离场路径，大字号）=====
        pos.y += 36*1.4 + 120;

        FontSource *fs2 = CreateFontSource(OS_TEXT("微软雅黑"), 72);
        if(!fs2)
            return(false);

        fs2->SetSDFEnabled(true);
        if(!fs2->IsSDFEnabled())
            return(false);

        cs = layout::CharStyle{};
        cs.text_color = WHITE.toRGBA8();
        if(!CreateTextLine(fs2, cs, U16String(OS_TEXT("字体源2: 微软雅黑 72px SDF (普通)")), pos))
            return(false);

        pos.y += 72*1.4;

        cs = layout::CharStyle{};
        cs.text_color = RED.toRGBA8();
        cs.bold_px = 2.0f;
        if(!CreateTextLine(fs2, cs, U16String(OS_TEXT("字体源2: 72px 红字加粗 (Bold)")), pos))
            return(false);

        // ===== 字体源3：楷体 48 号（原始位图路径，与 SDF 共存）=====
        pos.y += 72*1.4 + 120;

        FontSource *fs3 = CreateCJKFontSource(OS_TEXT("Consolas"), OS_TEXT("楷体"), 48);
        if(!fs3)
            return(false);

        // 不启用 SDF → 走原始位图采样路径（Nearest 采样）

        cs = layout::CharStyle{};
        cs.text_color = WHITE.toRGBA8();
        if(!CreateTextLine(fs3, cs, U16String(OS_TEXT("字体源3: 楷体 48px 位图 (Bitmap)")), pos))
            return(false);

        pos.y += 48*1.4;

        cs = layout::CharStyle{};
        cs.text_color = GREEN.toRGBA8();
        if(!CreateTextLine(fs3, cs, U16String(OS_TEXT("字体源3: 48px 绿字 (位图)")), pos))
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
    return RunFramework<TestApp>(OS_TEXT("MultiFontSource_ECS"), argc, argv, WINDOW_WIDTH, WINDOW_HEIGHT);
}
