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
 * MonoBitmapFontSource 演示：使用整块预烘点阵字体文件（res/font/VGA8.F16）。
 *
 * VGA8.F16 内容是 256 个标准 ASCII 字符的 8x16 单色点阵：
 *   一字节一行，每字节 8 个像素点（MSB 为最左），每字符 16 行 = 16 字节。
 * MonoBitmapFontSource 在 Init() 时一次性把整个文件展开成 8bit 灰度并建好
 * 全部 256 份 FontBitmap，因此这里除创建字体源之外，与 TextDraw 的用法完全一致。
 */
class TextDrawMonoBitmapApp:public WorkObject
{
private:

    ECSContext *        ecs_world           = nullptr;

private:

    /**
    * 创建一个由点阵字体文件驱动的字体源
    * @param filename 点阵字体文件路径
    * @return 字体源，失败返回 nullptr
    */
    static FontSource *CreateMonoBitmapFontSource(const OSString &filename)
    {
        MonoBitmapFontSource *ds=new MonoBitmapFontSource();

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

protected:

    bool InitTextRenderable()
    {
        ecs_world=GetECSContext();
        if(!ecs_world)
            return(false);

        FontSource *fs=CreateMonoBitmapFontSource(OS_TEXT("res/font/VGA8.F16"));
        if(!fs)
            return(false);

        //点阵字形本身就是最终灰度，没有距离场，无需也不必开启 SDF
        if(fs->IsSDFEnabled())
            return(false);

        const Color4ub WHITE (255,255,255,255);
        const Color4ub YELLOW(255,220, 60,255);
        const Color4ub CYAN  ( 80,220,255,255);
        const Color4ub GREEN (120,255,120,255);

        layout::CharStyle cs;
        math::Vector2i pos={40,40};

        //标准 ASCII 可打印区 0x20-0x3F
        cs.text_color=YELLOW.packUnorm4x8();
        if(!CreateTextLine(fs,cs,OS_TEXT(" !\"#$%&'()*+,-./0123456789:;<=>?"),pos))
            return(false);

        pos.y+=16*2;

        //0x40-0x5F
        cs.text_color=WHITE.packUnorm4x8();
        if(!CreateTextLine(fs,cs,OS_TEXT("@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_"),pos))
            return(false);

        pos.y+=16*2;

        //0x60-0x7E
        cs.text_color=CYAN.packUnorm4x8();
        if(!CreateTextLine(fs,cs,OS_TEXT("`abcdefghijklmnopqrstuvwxyz{|}~"),pos))
            return(false);

        pos.y+=16*3;

        //放大显示：位图路径走 Nearest 采样，整数倍放大仍然锐利
        cs.text_color=GREEN.packUnorm4x8();
        cs.scale=2.0f;
        if(!CreateTextLine(fs,cs,OS_TEXT("MonoBitmapFontSource 8x16 scale 2"),pos))
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
