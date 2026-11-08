#pragma once

#include<hgl/graph/font/FontSource.h>
#include<hgl/type/String.h>
#include<hgl/log/Log.h>

namespace hgl::graph
{
    /**
    * 固定 256 字符的单色点阵字体源
    *
    * 面向"整块预烘好的点阵字体文件"（典型如 VGA8.F16）：
    *
    *   文件 = 256 个字符 × CHAR_HEIGHT 行 × 1 字节/行
    *   1 字节 = 8 个像素点，MSB 为最左像素，字节自上而下排列。
    *
    * 设计要点：
    *   - 字符集固定 256、尺寸固定，容量极小，所以 Init() 一次性把整个文件展开成
    *     8bit 灰度（1 位点阵 → 0/255），并建立全部 256 份 FontBitmap；
    *   - 运行期 GetCharBitmap() 只返回内部指针，不再分配、不再做缓存查找，
    *     也没有引用计数（位图数据归本对象所有，与对象同生命周期）；
    *   - 因为字形就是最终灰度，不需要SDF，IsSDFEnabled() 恒为 false，
    *     渲染管线会走 Nearest 采样 + 直取覆盖度的位图分支。
    *
    * 使用方式：
    * @code
    *   MonoBitmapFontSource *fs=new MonoBitmapFontSource();
    *   if(!fs->Init(OS_TEXT("res/font/VGA8.F16"))){...}
    *   font_source=new FontSource(fs);        // FontSource 接管引用计数，不接管所有权
    * @endcode
    */
    class MonoBitmapFontSource:public FontDataSource
    {
        OBJECT_LOGGER

    public:

        static constexpr int CHAR_COUNT =256;                       ///<字符总数(标准ASCII 256码位)
        static constexpr int CHAR_WIDTH =8;                         ///<字符宽度(像素)
        static constexpr int CHAR_HEIGHT=16;                        ///<字符高度(像素)
        static constexpr int CHAR_BYTES =CHAR_WIDTH*CHAR_HEIGHT;    ///<单字符展开后的灰度字节数(128)
        static constexpr int CHAR_PACKED_BYTES=CHAR_HEIGHT;         ///<单字符在文件中的字节数(每行1字节)
        static constexpr int FILE_BYTES =CHAR_COUNT*CHAR_PACKED_BYTES;  ///<完整字体文件字节数(4096)

    private:

        uint8           grayscale[CHAR_COUNT*CHAR_BYTES];           ///<展开后的 8bit 灰度位图(0/255)
        FontBitmap      bitmaps[CHAR_COUNT];                        ///<256 份字符位图描述(data 指向 grayscale)
        CharMetricsInfo metrics;                                    ///<固定字符度量(全字符集一致)

        bool            loaded=false;                               ///<是否已成功加载

    public:

        MonoBitmapFontSource();
        ~MonoBitmapFontSource()override=default;

        /**
        * 加载点阵字体文件并一次性展开为 8bit 灰度
        * @param filename 字体文件路径(如 res/font/VGA8.F16)
        * @return 是否加载成功
        */
        bool Init(const OSString &filename);

        /**
        * 取得字符位图
        * @param ch 字符码位
        * @return 字符位图指针，空格或超出 256 范围返回 nullptr
        */
        FontBitmap *GetCharBitmap(const u32char &ch)override;

        /**
        * 取得字符绘制信息
        * @param cmi 输出绘制信息
        * @param ch 字符码位
        * @return 是否有有效绘制信息
        */
        const bool GetCharMetrics(CharMetricsInfo &cmi,const u32char &ch)override;

        int GetCharHeight()const override{return CHAR_HEIGHT;}
    };//class MonoBitmapFontSource:public FontDataSource
}//namespace hgl::graph
