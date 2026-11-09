#include<hgl/graph/font/MonoBitmapFontSource.h>
#include<hgl/io/FileInputStream.h>
#include<hgl/type/MemoryUtil.h>
#include<hgl/type/MemoryAlloc.h>
#include<hgl/Macro.h>
#include<hgl/utf.h>

namespace hgl::graph
{
    MonoBitmapFontSource::MonoBitmapFontSource(const int width, const int height, const int count)
    {
        char_width  = (width  > 0) ? width  : DEFAULT_CHAR_WIDTH;
        char_height = (height > 0) ? height : DEFAULT_CHAR_HEIGHT;
        char_count  = (count  > 0) ? count  : DEFAULT_CHAR_COUNT;

        // 点阵字形占满整个字格，无内边距：
        //   基线位于行盒底部，字形顶部 = 基线 - char_height = 行盒顶部。
        //   （char_quad_body.glsl: pos_top = pen_y - metrics.y + char_height * scale，
        //     故 metrics.y 取 char_height 时字形顶部正好与行盒顶部重合。）
        metrics.x    = 0;
        metrics.y    = char_height;
        metrics.w    = char_width;
        metrics.h    = char_height;
        metrics.adv_x= char_width;
        metrics.adv_y= 0;
    }

    MonoBitmapFontSource::~MonoBitmapFontSource()
    {
        Clear();
    }

    void MonoBitmapFontSource::Clear()
    {
        SAFE_CLEAR_ARRAY(grayscale);
        SAFE_CLEAR_ARRAY(bitmaps);
        loaded = false;
    }

    bool MonoBitmapFontSource::Init(const OSString &filename)
    {
        return Init(filename, char_width, char_height, char_count);
    }

    bool MonoBitmapFontSource::Init(const OSString &filename, const int width, const int height, const int count)
    {
        if(loaded)
            return(true);

        if(width <= 0 || height <= 0 || count <= 0)
        {
            LogError(u8"点阵字体参数无效: %s (width=%d, height=%d, count=%d)",
                     ToU8String(filename).c_str(), width, height, count);
            return(false);
        }

        io::OpenFileInputStream fis(filename);

        if(!fis)
        {
            LogError(u8"无法打开点阵字体文件: %s", ToU8String(filename).c_str());
            return(false);
        }

        const int row_packed_bytes  = (width + 7) >> 3;         //每行在文件中的字节数(1位1像素)
        const int char_packed_bytes = row_packed_bytes * height;
        const int expected_bytes    = count * char_packed_bytes;

        const int64 file_size = fis->GetSize();

        if(file_size != expected_bytes)
        {
            LogError(u8"点阵字体文件尺寸不正确: %s (期望 %d 字节，实际 %d 字节)",
                     ToU8String(filename).c_str(), expected_bytes, int(file_size));
            return(false);
        }

        uint8 *packed = zero_new<uint8>(expected_bytes);        //原始 1 位点阵数据（临时，展开后即丢弃）
        if(!packed)
        {
            LogError(u8"点阵字体读取缓冲分配失败 (%d 字节): %s", expected_bytes, ToU8String(filename).c_str());
            return(false);
        }

        if(fis->Read(packed, expected_bytes) != expected_bytes)
        {
            SAFE_CLEAR_ARRAY(packed);
            LogError(u8"点阵字体文件读取失败: %s", ToU8String(filename).c_str());
            return(false);
        }

        Clear();

        char_width  = width;
        char_height = height;
        char_count  = count;

        metrics.x    = 0;
        metrics.y    = char_height;
        metrics.w    = char_width;
        metrics.h    = char_height;
        metrics.adv_x= char_width;
        metrics.adv_y= 0;

        const int char_bytes            = char_width * char_height;
        const int total_grayscale_bytes = char_count * char_bytes;

        grayscale = zero_new<uint8>(total_grayscale_bytes);
        bitmaps   = zero_new<FontBitmap>(char_count);

        if(!grayscale || !bitmaps)
        {
            SAFE_CLEAR_ARRAY(packed);
            Clear();
            LogError(u8"点阵字体灰度/位图描述缓冲分配失败: %s", ToU8String(filename).c_str());
            return(false);
        }

        // 一次性展开：1 位/像素 → 8bit 灰度(0/255)，MSB 为最左像素
        for(int ch = 0; ch < char_count; ch++)
        {
            const uint8 *src = packed    + ch * char_packed_bytes;
            uint8       *dst = grayscale + ch * char_bytes;

            for(int row = 0; row < char_height; row++)
            {
                const uint8 *row_src = src + row * row_packed_bytes;

                for(int col = 0; col < char_width; col++)
                {
                    const uint8 byte_val = row_src[col >> 3];
                    const uint8 bit_mask = 0x80u >> (col & 7);

                    *dst++ = (byte_val & bit_mask) ? 0xFF : 0x00;
                }
            }

            // 建立字符位图描述：data 直接指向灰度缓冲内的固定位置，
            // 本对象生命周期内地址不变，因此无需任何缓存或引用计数。
            FontBitmap &bmp = bitmaps[ch];

            bmp.count       = 1;
            bmp.metrics_info= metrics;
            bmp.data        = grayscale + ch * char_bytes;
        }

        SAFE_CLEAR_ARRAY(packed);

        loaded = true;

        LogInfo(u8"点阵字体加载完成: %s (%d 字符 %dx%d)",
                ToU8String(filename).c_str(), char_count, char_width, char_height);

        return(true);
    }

    FontBitmap *MonoBitmapFontSource::GetCharBitmap(const u32char &ch)
    {
        if(ch >= (u32char)char_count)   //超出字符范围无字形
            return(nullptr);

        if(hgl::is_space(ch))           //空格宽度由 TextLayout 按 space_size 计算，不需要位图
            return(nullptr);

        return bitmaps + ch;
    }

    const bool MonoBitmapFontSource::GetCharMetrics(CharMetricsInfo &cmi, const u32char &ch)
    {
        if(ch >= (u32char)char_count)   //超出字符范围无字形
            return(false);

        if(hgl::is_space(ch))           //空格无可绘制字形，与 GetCharBitmap 保持一致
            return(false);

        cmi = metrics;
        return(true);
    }
}//namespace hgl::graph
