#include <hgl/graph/font/MonoBitmapFontSource.h>
#include <hgl/graph/font/FontSource.h>
#include <hgl/log/Log.h>

using namespace hgl;
using namespace hgl::graph;

#define EXPECT(cond) \
    do { \
        if (!(cond)) { \
            GLogError(u8"断言失败 [%s:%d]: %s", __FILE__, __LINE__, #cond); \
            return false; \
        } \
    } while (0)

namespace
{
    bool TestVGA8_16()
    {
        MonoBitmapFontSource fs16(8, 16);
        EXPECT(fs16.Init(OS_TEXT("res/font/VGA8.F16")));

        EXPECT(fs16.GetCharWidth() == 8);
        EXPECT(fs16.GetCharHeight() == 16);
        EXPECT(fs16.GetCharCount() == 256);

        // 'A' 有有效位图与度量
        FontBitmap *bmp_A = fs16.GetCharBitmap('A');
        EXPECT(bmp_A != nullptr);
        EXPECT(bmp_A->data != nullptr);
        EXPECT(bmp_A->metrics_info.w == 8);
        EXPECT(bmp_A->metrics_info.h == 16);
        EXPECT(bmp_A->metrics_info.y == 16);
        EXPECT(bmp_A->metrics_info.adv_x == 8);

        CharMetricsInfo cmi;
        EXPECT(fs16.GetCharMetrics(cmi, 'A'));
        EXPECT(cmi.w == 8 && cmi.h == 16);

        // 空格无位图
        EXPECT(fs16.GetCharBitmap(' ') == nullptr);
        EXPECT(!fs16.GetCharMetrics(cmi, ' '));

        // 越界码位
        EXPECT(fs16.GetCharBitmap(256) == nullptr);
        EXPECT(fs16.GetCharBitmap(1000) == nullptr);

        // FontSource 封装
        FontSource font_source(&fs16);
        EXPECT(font_source.GetCharHeight() == 16);
        EXPECT(!font_source.IsSDFEnabled());

        return true;
    }

    bool TestFMT437_08()
    {
        MonoBitmapFontSource fs8(8, 8);
        EXPECT(fs8.Init(OS_TEXT("res/font/FM-T-437.F08")));

        EXPECT(fs8.GetCharWidth() == 8);
        EXPECT(fs8.GetCharHeight() == 8);
        EXPECT(fs8.GetCharCount() == 256);

        // 'A' 有有效位图与度量
        FontBitmap *bmp_A = fs8.GetCharBitmap('A');
        EXPECT(bmp_A != nullptr);
        EXPECT(bmp_A->data != nullptr);
        EXPECT(bmp_A->metrics_info.w == 8);
        EXPECT(bmp_A->metrics_info.h == 8);
        EXPECT(bmp_A->metrics_info.y == 8);
        EXPECT(bmp_A->metrics_info.adv_x == 8);

        CharMetricsInfo cmi;
        EXPECT(fs8.GetCharMetrics(cmi, 'A'));
        EXPECT(cmi.w == 8 && cmi.h == 8);

        // 空格无位图
        EXPECT(fs8.GetCharBitmap(' ') == nullptr);
        EXPECT(!fs8.GetCharMetrics(cmi, ' '));

        // 越界码位
        EXPECT(fs8.GetCharBitmap(256) == nullptr);
        EXPECT(fs8.GetCharBitmap(500) == nullptr);

        // FontSource 封装
        FontSource font_source(&fs8);
        EXPECT(font_source.GetCharHeight() == 8);
        EXPECT(!font_source.IsSDFEnabled());

        return true;
    }

    bool TestDynamicInit()
    {
        // 动态使用 Init(filename, width, height)
        MonoBitmapFontSource fs;
        EXPECT(fs.Init(OS_TEXT("res/font/FM-T-437.F08"), 8, 8));
        EXPECT(fs.GetCharWidth() == 8);
        EXPECT(fs.GetCharHeight() == 8);

        // 尺寸与参数不符应报错并返回 false
        MonoBitmapFontSource fs_bad;
        EXPECT(!fs_bad.Init(OS_TEXT("res/font/FM-T-437.F08"), 8, 16)); // 期望 4096 字节，实际 2048

        return true;
    }
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    if (!TestVGA8_16())
    {
        GLogError(u8"TestVGA8_16 失败");
        return 1;
    }

    if (!TestFMT437_08())
    {
        GLogError(u8"TestFMT437_08 失败");
        return 1;
    }

    if (!TestDynamicInit())
    {
        GLogError(u8"TestDynamicInit 失败");
        return 1;
    }

    GLogInfo(u8"TestMonoBitmapFontSource: 全部测试通过 (VGA8.F16 8x16, FM-T-437.F08 8x8)");
    return 0;
}
