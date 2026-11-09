#pragma once

#include<hgl/graph/font/FontSource.h>
#include<hgl/type/String.h>
#include<hgl/log/Log.h>

namespace hgl::graph
{
    /**
    * 单色点阵字体源
    *
    * 面向"整块预烘好的点阵字体文件"（如 VGA8.F16 8x16、FM-T-437.F08 8x8 等）：
    *
    *   文件 = char_count 个字符 × char_height 行 × row_packed_bytes 字节/行
    *   row_packed_bytes = (char_width + 7) / 8
    *   1 字节 = 8 个像素点，MSB 为最左像素，字节自上而下排列。
    *   - 8x16 字体（如 VGA8.F16）：256 字符，每字符 16 字节，文件大小 4096 字节
    *   - 8x8  字体（如 FM-T-437.F08）：256 字符，每字符 8 字节，文件大小 2048 字节
    *   - 支持由构造参数或 Init() 参数显式传入字符宽高与字符数
    *
    * 设计要点：
    *   - 尺寸与字符集由参数传入，Init() 一次性把整个文件展开成 8bit 灰度（1 位点阵 → 0/255），
    *     并建立全部字符的 FontBitmap；
    *   - 运行期 GetCharBitmap() 只返回内部指针，不再分配、不再做缓存查找，
    *     也没有引用计数（位图数据归本对象所有，与对象同生命周期）；
    *   - 因为字形就是最终灰度，不需要SDF，IsSDFEnabled() 恒为 false，
    *     渲染管线会走 Nearest 采样 + 直取覆盖度的位图分支。
    *
    * 使用方式：
    * @code
    *   // 8x16 样式（如 VGA8.F16）：
    *   MonoBitmapFontSource *fs16 = new MonoBitmapFontSource(8, 16);
    *   fs16->Init(OS_TEXT("res/font/VGA8.F16"));
    *
    *   // 8x8 样式（如 FM-T-437.F08）：
    *   MonoBitmapFontSource *fs8 = new MonoBitmapFontSource(8, 8);
    *   fs8->Init(OS_TEXT("res/font/FM-T-437.F08"));
    * @endcode
    */
    class MonoBitmapFontSource:public FontDataSource
    {
        OBJECT_LOGGER

    public:

        static constexpr int DEFAULT_CHAR_WIDTH  = 8;                       ///<缺省字符宽度
        static constexpr int DEFAULT_CHAR_HEIGHT = 16;                      ///<缺省字符高度
        static constexpr int DEFAULT_CHAR_COUNT  = 256;                     ///<缺省字符总数

        // 兼容历史常量
        static constexpr int CHAR_WIDTH          = DEFAULT_CHAR_WIDTH;
        static constexpr int CHAR_HEIGHT         = DEFAULT_CHAR_HEIGHT;
        static constexpr int CHAR_COUNT          = DEFAULT_CHAR_COUNT;
        static constexpr int CHAR_BYTES          = CHAR_WIDTH*CHAR_HEIGHT;
        static constexpr int CHAR_PACKED_BYTES   = CHAR_HEIGHT;
        static constexpr int FILE_BYTES          = CHAR_COUNT*CHAR_PACKED_BYTES;

    private:

        int             char_width  = DEFAULT_CHAR_WIDTH;                   ///<字符宽度(像素)
        int             char_height = DEFAULT_CHAR_HEIGHT;                  ///<字符高度(像素)
        int             char_count  = DEFAULT_CHAR_COUNT;                   ///<字符总数

        uint8 *         grayscale   = nullptr;                              ///<展开后的 8bit 灰度位图(0/255)
        FontBitmap *    bitmaps     = nullptr;                              ///<字符位图描述数组(data 指向 grayscale)
        CharMetricsInfo metrics;                                            ///<固定字符度量(全字符集一致)

        bool            loaded      = false;                                ///<是否已成功加载

    private:

        void Clear();

    public:

        /**
        * 构造函数：由参数指定字符宽高与字符数
        * @param width 字符宽度(像素)
        * @param height 字符高度(像素)
        * @param count 字符总数(缺省 256 标准 ASCII)
        */
        MonoBitmapFontSource(const int width = DEFAULT_CHAR_WIDTH,
                             const int height = DEFAULT_CHAR_HEIGHT,
                             const int count = DEFAULT_CHAR_COUNT);

        ~MonoBitmapFontSource() override;

        /**
        * 加载点阵字体文件并一次性展开为 8bit 灰度（使用构造时指定的宽高）
        * @param filename 字体文件路径
        * @return 是否加载成功
        */
        bool Init(const OSString &filename);

        /**
        * 指定字符宽高与字符数并加载点阵字体文件
        * @param filename 字体文件路径
        * @param width 字符宽度(像素)
        * @param height 字符高度(像素)
        * @param count 字符总数(缺省 256)
        * @return 是否加载成功
        */
        bool Init(const OSString &filename, const int width, const int height, const int count = DEFAULT_CHAR_COUNT);

        /**
        * 取得字符位图
        * @param ch 字符码位
        * @return 字符位图指针，空格或超出字符范围返回 nullptr
        */
        FontBitmap *GetCharBitmap(const u32char &ch) override;

        /**
        * 取得字符绘制信息
        * @param cmi 输出绘制信息
        * @param ch 字符码位
        * @return 是否有有效绘制信息
        */
        const bool GetCharMetrics(CharMetricsInfo &cmi, const u32char &ch) override;

        int GetCharWidth()  const { return char_width; }
        int GetCharHeight() const override { return char_height; }
        int GetCharCount()  const { return char_count; }
    };//class MonoBitmapFontSource:public FontDataSource
}//namespace hgl::graph
