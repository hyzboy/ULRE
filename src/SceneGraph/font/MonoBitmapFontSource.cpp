#include<hgl/graph/font/MonoBitmapFontSource.h>
#include<hgl/io/FileInputStream.h>
#include<hgl/type/MemoryUtil.h>
#include<hgl/utf.h>

namespace hgl::graph
{
    MonoBitmapFontSource::MonoBitmapFontSource()
    {
        mem_zero(grayscale);
        mem_zero(bitmaps);

        // 点阵字形占满整个字格，无内边距：
        //   基线位于行盒底部，字形顶部 = 基线 - CHAR_HEIGHT = 行盒顶部。
        //   （char_quad_body.glsl: pos_top = pen_y - metrics.y + char_height * scale，
        //     故 metrics.y 取 CHAR_HEIGHT 时字形顶部正好与行盒顶部重合。）
        metrics.x    =0;
        metrics.y    =CHAR_HEIGHT;
        metrics.w    =CHAR_WIDTH;
        metrics.h    =CHAR_HEIGHT;
        metrics.adv_x=CHAR_WIDTH;
        metrics.adv_y=0;
    }

    bool MonoBitmapFontSource::Init(const OSString &filename)
    {
        if(loaded)
            return(true);

        io::OpenFileInputStream fis(filename);

        if(!fis)
        {
            LogError(u8"无法打开点阵字体文件: %s",ToU8String(filename).c_str());
            return(false);
        }

        const int64 file_size=fis->GetSize();

        if(file_size!=FILE_BYTES)
        {
            LogError(u8"点阵字体文件尺寸不正确: %s (期望 %d 字节，实际 %d 字节)",
                     ToU8String(filename).c_str(),FILE_BYTES,int(file_size));
            return(false);
        }

        uint8 packed[FILE_BYTES];           //原始 1 位点阵数据（临时，展开后即丢弃）

        if(fis->Read(packed,FILE_BYTES)!=FILE_BYTES)
        {
            LogError(u8"点阵字体文件读取失败: %s",ToU8String(filename).c_str());
            return(false);
        }

        //一次性展开：1 位/像素 → 8bit 灰度(0/255)，MSB 为最左像素
        for(int ch=0;ch<CHAR_COUNT;ch++)
        {
            const uint8 *src=packed    +ch*CHAR_BYTES;
            uint8       *dst=grayscale +ch*CHAR_BYTES;

            for(int row=0;row<CHAR_HEIGHT;row++)
            {
                const uint8 bits=*src++;

                for(int col=0;col<CHAR_WIDTH;col++)
                    *dst++=(bits&(0x80u>>col))?0xFF:0x00;
            }

            //建立字符位图描述：data 直接指向灰度缓冲内的固定位置，
            //本对象生命周期内地址不变，因此无需任何缓存或引用计数。
            FontBitmap &bmp=bitmaps[ch];

            bmp.count       =1;
            bmp.metrics_info=metrics;
            bmp.data        =grayscale+ch*CHAR_BYTES;
        }

        loaded=true;

        LogInfo(u8"点阵字体加载完成: %s (%d 字符 %dx%d)",
                ToU8String(filename).c_str(),CHAR_COUNT,CHAR_WIDTH,CHAR_HEIGHT);

        return(true);
    }

    FontBitmap *MonoBitmapFontSource::GetCharBitmap(const u32char &ch)
    {
        if(ch>=CHAR_COUNT)          //字符集固定 256，超出范围即无字形
            return(nullptr);

        if(hgl::is_space(ch))       //空格宽度由 TextLayout 按 space_size 计算，不需要位图
            return(nullptr);

        return bitmaps+ch;
    }

    const bool MonoBitmapFontSource::GetCharMetrics(CharMetricsInfo &cmi,const u32char &ch)
    {
        if(ch>=CHAR_COUNT)          //字符集固定 256，超出范围即无字形
            return(false);

        if(hgl::is_space(ch))       //空格无可绘制字形，与 GetCharBitmap 保持一致
            return(false);

        cmi=metrics;
        return(true);
    }
}//namespace hgl::graph
