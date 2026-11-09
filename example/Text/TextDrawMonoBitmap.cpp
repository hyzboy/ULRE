#include<hgl/ecs/core/Context.h>
#include<hgl/ecs/components/TextComponent.h>
#include<hgl/graph/font/FontSource.h>
#include<hgl/graph/font/MonoBitmapFontSource.h>
#include<hgl/graph/font/TextCharSSBO.h>
#include<hgl/color/Color.h>
#include<hgl/color/Color4ub.h>
#include<hgl/framework/WorkManager.h>

using namespace hgl;
using namespace hgl::ecs;
using namespace hgl::graph;

constexpr const uint32_t WINDOW_WIDTH   = 1600;
constexpr const uint32_t WINDOW_HEIGHT  = 900;

/**
 * MonoBitmapFontSource 演示：使用整块预烘点阵字体文件。
 *
 * 演示两种不同规格的单色点阵字体：
 *   1. res/font/VGA8.F16    - 256 个标准 ASCII 字符的 8x16 单色点阵（4096 字节）
 *   2. res/font/FM-T-437.F08 - 256 个标准 ASCII 字符的 8x8 单色点阵（2048 字节）
 *
 * MonoBitmapFontSource 构造与 Init 支持传入字符宽高（如 8x16、8x8），
 * 一次性将整个文件展开成 8bit 灰度并建立全部 256 份 FontBitmap。
 */
class TextDrawMonoBitmapApp:public WorkObject
{
private:

    ECSContext *        ecs_world           = nullptr;

private:

    /**
    * 创建一个由点阵字体文件驱动的字体源
    * @param filename 点阵字体文件路径
    * @param width 字符宽度(像素)
    * @param height 字符高度(像素)
    * @return 字体源，失败返回 nullptr
    */
    static FontSource *CreateMonoBitmapFontSource(const OSString &filename, const int width, const int height)
    {
        MonoBitmapFontSource *ds=new MonoBitmapFontSource(width, height);

        if(!ds->Init(filename))
        {
            delete ds;
            return(nullptr);
        }

        //FontSource 只接管引用计数，不接管所有权，故数据源与字体源同生命周期
        return new FontSource(ds);
    }

    bool CreateTextLine(FontSource *fs,const layout::CharStyle &cs,const os_char *text,const math::Vector2i &position)
    {
        auto *entity=ecs_world->CreateEntity();
        if(!entity)
            return(false);

        auto text_comp=entity->AddComponent<TextComponent>();
        if(!text_comp)
            return(false);

        text_comp->SetText(U16String(text));
        text_comp->SetFontSource(fs);
        text_comp->SetStartPosition(position);
        text_comp->SetCharStyle(cs);

        return(true);
    }

    bool AddFontShowcase(FontSource *fs, const os_char *title, math::Vector2i &pos)
    {
        const Color4ub TITLE_COLOR(255, 180, 100, 255);
        const Color4ub WHITE      (255, 255, 255, 255);
        const Color4ub YELLOW     (255, 220,  60, 255);
        const Color4ub CYAN       ( 80, 220, 255, 255);
        const Color4ub GREEN      (120, 255, 120, 255);

        const int ch_h = fs->GetCharHeight();

        layout::CharStyle cs;

        // 标题行
        cs.text_color = TITLE_COLOR.packUnorm4x8();
        cs.scale = 1.0f;
        if(!CreateTextLine(fs, cs, title, pos))
            return false;
        pos.y += ch_h * 2;

        // 标准 ASCII 可打印区 0x20-0x3F
        cs.text_color = YELLOW.packUnorm4x8();
        if(!CreateTextLine(fs, cs, OS_TEXT(" !\"#$%&'()*+,-./0123456789:;<=>?"), pos))
            return false;
        pos.y += ch_h + 8;

        // 0x40-0x5F
        cs.text_color = WHITE.packUnorm4x8();
        if(!CreateTextLine(fs, cs, OS_TEXT("@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_"), pos))
            return false;
        pos.y += ch_h + 8;

        // 0x60-0x7E
        cs.text_color = CYAN.packUnorm4x8();
        if(!CreateTextLine(fs, cs, OS_TEXT("`abcdefghijklmnopqrstuvwxyz{|}~"), pos))
            return false;
        pos.y += ch_h + 12;

        // 放大显示：位图路径走 Nearest 采样，整数倍放大仍然锐利
        cs.text_color = GREEN.packUnorm4x8();
        cs.scale = 2.0f;
        if(!CreateTextLine(fs, cs, OS_TEXT("Scale 2.0: The quick brown fox jumps over the lazy dog!"), pos))
            return false;
        pos.y += ch_h * 2 + 32;

        return true;
    }

protected:

    bool InitTextRenderable()
    {
        ecs_world=GetECSContext();
        if(!ecs_world)
            return(false);

        // 1. 8x16 点阵字体：VGA8.F16
        FontSource *fs_vga16=CreateMonoBitmapFontSource(OS_TEXT("res/font/VGA8.F16"), 8, 16);
        if(!fs_vga16)
            return(false);

        // 2. 8x8 点阵字体：FM-T-437.F08
        FontSource *fs_fmt08=CreateMonoBitmapFontSource(OS_TEXT("res/font/FM-T-437.F08"), 8, 8);
        if(!fs_fmt08)
            return(false);

        // 点阵字形本身就是最终灰度，没有距离场，无需也不必开启 SDF
        if(fs_vga16->IsSDFEnabled() || fs_fmt08->IsSDFEnabled())
            return(false);

        math::Vector2i pos={40, 40};

        if(!AddFontShowcase(fs_vga16, OS_TEXT("=== MonoBitmapFontSource: VGA8.F16 (8x16) ==="), pos))
            return(false);

        if(!AddFontShowcase(fs_fmt08, OS_TEXT("=== MonoBitmapFontSource: FM-T-437.F08 (8x8) ==="), pos))
            return(false);

        return(true);
    }

public:

    bool Init() override
    {
        SetClearColor(Color4f(0.05f,0.06f,0.09f,1.0f));

        if(!InitTextRenderable())
            return(false);

        return(true);
    }
};//class TextDrawMonoBitmapApp:public WorkObject

int os_main(int argc,os_char **argv)
{
    return RunFramework<TextDrawMonoBitmapApp>(OS_TEXT("TextDrawMonoBitmap"),argc,argv,WINDOW_WIDTH,WINDOW_HEIGHT);
}
