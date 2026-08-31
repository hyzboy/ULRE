#include<hgl/type/StringViewList.h>
#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/components/TextComponent.h>
#include<hgl/graph/font/FontSource.h>
#include<hgl/framework/WorkManager.h>
#include<hgl/color/Color.h>
#include<hgl/color/Color4ub.h>
#include<random>

using namespace hgl;
using namespace hgl::ecs;
using namespace hgl::graph;

constexpr const uint32_t WINDOW_WIDTH = 2560;
constexpr const uint32_t WINDOW_HEIGHT = 1440;

constexpr const uint32_t FONT_SIZE = 48;

class TestApp:public WorkObject
{
private:

    ECSContext *        ecs_world           = nullptr;

protected:

    bool InitTextRenderable()
    {
        U16StringViewList str_list;

        LoadStringViewListFromTextFile(str_list, OS_TEXT("res/text/百家姓.txt"));

        if(str_list.IsEmpty())
            return(false);

        ecs_world = GetECSContext();
        if(!ecs_world)
            return(false);

        // Create CJK font source
        FontSource *fs = CreateCJKFontSource(OS_TEXT("Consolas"), OS_TEXT("楷体"), FONT_SIZE);
        if(!fs)
            return(false);

        // For random positioning
        std::default_random_engine dre;
        std::uniform_int_distribution<int> rand_x(0, WINDOW_WIDTH - FONT_SIZE);
        std::uniform_int_distribution<int> rand_y(0, WINDOW_HEIGHT - FONT_SIZE);

        // 固定颜色调色板，便于视觉验证 per-paragraph 样式
        static const hgl::Color4ub palette[] = {
            hgl::Color4ub(255,   0,   0, 255),   // Red
            hgl::Color4ub(  0, 255,   0, 255),   // Green
            hgl::Color4ub(  0,   0, 255, 255),   // Blue
            hgl::Color4ub(255, 255,   0, 255),   // Yellow
            hgl::Color4ub(255,   0, 255, 255),   // Magenta
            hgl::Color4ub(  0, 255, 255, 255),   // Cyan
            hgl::Color4ub(255, 128,   0, 255),   // Orange
            hgl::Color4ub(128,   0, 255, 255),   // Purple
        };
        constexpr int PALETTE_SIZE = 8;

        // Create one entity per text string
        int line_index = 0;
        for (auto str : str_list)
        {
            auto* entity = ecs_world->CreateEntity();
            if(!entity)
                continue;

            auto text_comp = entity->AddComponent<TextComponent>();
            if(!text_comp)
                continue;

            hgl::graph::layout::CharStyle cs{};

            cs.text_color = palette[line_index % PALETTE_SIZE].toRGBA8();

            // 每 3 行设置不同的倾斜角度，测试 italic 特性
            if (line_index % 3 == 1)
                cs.italic = 0.3f;    // 轻微右斜
            else if (line_index % 3 == 2)
                cs.italic = -0.2f;   // 轻微左斜
            // else cs.italic == 0.0f (default)

            // Set text, font, and random position
            text_comp->SetText(U16String(str.c_str(), str.Length()));
            text_comp->SetFontSource(fs);
            text_comp->SetStartPosition({rand_x(dre), rand_y(dre)});
            text_comp->SetCharStyle(cs);

            ++line_index;
            // TextRenderPipeline will batch by FontSource automatically!
        }

        return(true);
    }

public:
    bool Init() override
    {
        if(!InitTextRenderable())
            return(false);

        return(true);
    }
};//class TestApp:public WorkObject

int os_main(int argc, os_char **argv)
{
    return RunFramework<TestApp>(OS_TEXT("DrawMultiLineText_ECS"), argc, argv, WINDOW_WIDTH, WINDOW_HEIGHT);
}

