#pragma once

#include<hgl/graph/font/FontSource.h>
#include<hgl/graph/font/TextCharSSBO.h>
#include<hgl/math/Vector.h>
#include<hgl/type/StringView.h>
#include<cstdint>

namespace hgl::graph
{
    class TileFont;

    namespace layout
    {
        // CharStyle 定义已移至 TextCharSSBO.h（CPU/GPU 共用，40B std430 布局，含 scale/rotation）
        // CPU 侧通过 TextRenderPipeline 做 Color4ub → packed uint32 转换

        using CharDrawStyleID=uint8;

        constexpr const size_t CharDrawStyleBytes=sizeof(CharStyle);

        using TEXT_COORD_TYPE=int;                      //字符必须坐标对齐显示才能不模糊，所以这里坐标系全部使用整型坐标
        using TEXT_COORD_VEC=math::Vector2i;

        /**
        * 文本排列方向
        */
        enum class TextDirection : uint8
        {
            LeftToRight,    //<横排从左到右
            RightToLeft,    //<横排从右到左
            Vertical,       //<竖排从上到下，从右到左

            //有竖排从下到上的吗？从左到右？
        };//enum class TextDirection

        /**
        * 文本对齐
        */
        enum class TextAlign
        {
            Left=0,                     ///<左对齐
            Right,                      ///<右对齐
            Center,                     ///<居中
            Fill,                       ///<排满整行/列(水平方向按左对齐处理)

            Top     =Left,              ///<上对齐
            Bottom  =Right              ///<下对齐
        };//enum class TextAlign

        /**
        * 自动折行模式
        *
        * 仅在设置了矩形宽度(ParagraphStyle::rect_width > 0)时生效；
        * 显式 '\n' 始终是硬换行，不受本项影响。
        */
        enum class TextWrapMode : uint8
        {
            None=0,                     ///<不自动折行(缺省，保持自由排版)
            Char,                       ///<按字符折行(CJK 与无空格西文适用)
            Word,                       ///<按单词折行(优先断在空格处，单词过长时回退字符折行)
        };//enum class TextWrapMode

        /**
        * 溢出处理模式
        *
        * 矩形高度 / 最大行数只有在 Clip / Ellipsis 下才会截断内容；
        * Overflow 只把矩形作为排版宽度与对齐依据，允许内容纵向溢出。
        */
        enum class TextOverflowMode : uint8
        {
            Overflow=0,                 ///<不截断(缺省；矩形仅约束宽度与对齐)
            Clip,                       ///<超出矩形/最大行数的字符不输出
            Ellipsis,                   ///<截断并在最后一行行尾输出省略号
        };//enum class TextOverflowMode

        /**
        * 垂直对齐(仅在设置了矩形高度 rect_height > 0 时生效)
        */
        enum class TextVerticalAlign : uint8
        {
            Top=0,                      ///<顶部对齐
            Center,                     ///<垂直居中
            Bottom,                     ///<底部对齐
        };//enum class TextVerticalAlign

        /**
        * 文本装饰线类型(下划线 / 删除线，支持按位或组合)
        */
        enum class TextDecorationType : uint8
        {
            None          = 0,
            Underline     = 1 << 0,     ///<下划线
            Strikethrough = 1 << 1,     ///<删除线
        };//enum class TextDecorationType

        inline constexpr TextDecorationType operator|(TextDecorationType a, TextDecorationType b)
        {
            return static_cast<TextDecorationType>(static_cast<uint8>(a) | static_cast<uint8>(b));
        }
        inline constexpr TextDecorationType operator&(TextDecorationType a, TextDecorationType b)
        {
            return static_cast<TextDecorationType>(static_cast<uint8>(a) & static_cast<uint8>(b));
        }
        inline constexpr bool HasDecoration(TextDecorationType val, TextDecorationType flag)
        {
            return (static_cast<uint8>(val) & static_cast<uint8>(flag)) != 0;
        }

        /**
        * 文本装饰线区间(字符级下划线/删除线)
        *
        * 用于精确到字符的划线标记(如某几个字下划线、某个词删除线)。
        * start/count 对应源文本的字符下标与长度。
        */
        struct TextDecorationSpan
        {
            uint32_t           start = 0;                          ///<起始字符下标(UTF-16 索引)
            uint32_t           count = 0;                          ///<字符数量
            TextDecorationType type  = TextDecorationType::None;   ///<装饰类型

            auto operator <=> (const TextDecorationSpan &) const = default;
        };//struct TextDecorationSpan

        /**
        * 段落风格
        */
        struct ParagraphStyle
        {
            TextDirection   text_direction          =TextDirection::LeftToRight;                    ///<文本排列方向
            TextAlign       align                   =TextAlign::Left;                               ///<水平对齐
            TextVerticalAlign v_align               =TextVerticalAlign::Top;                        ///<垂直对齐
            float           char_gap                =0.0f;                                          ///<字间距
            float           line_gap                =0.1f;                                          ///<行间距(相对于字符高度)

            bool            disable_border_symbols  =true;                                          ///<禁用边界符号(如行首禁用逗号)
    //      bool            disable_break_word      =true;                                          ///<禁用断字(如英文单词不允许断开)
    //      bool            auto_symbols_convert    =true;                                          ///<自动符号转换(如tm/(r)/(c)等)

            float           space_size              =0.5f;                                          ///<半角空格尺寸(对应字符高度的系数)
            float           full_space_size         =1.0f;                                          ///<全角空格尺寸(对应字符高度的系数)
            float           tab_size                =4.0f;                                          ///<Tab符号尺寸(对应字符高度的系数)

            bool            compress_punctuation    =false;                                         ///<压缩标点符号

            // ── 矩形排版约束(RECT) ──────────────────────────────────────
            // 矩形 = (文本起始坐标 start_position, rect_width × rect_height)。
            // 0 表示该方向不限制，因此默认值等价于旧的自由排版行为。
            // 内边距(padding)属于控件层，由 GUI 计算好内容矩形后再传入。
            TEXT_COORD_TYPE rect_width              =0;                                             ///<排版矩形宽度(像素，0=不限制)
            TEXT_COORD_TYPE rect_height             =0;                                             ///<排版矩形高度(像素，0=不限制)
            TextWrapMode    wrap_mode               =TextWrapMode::None;                            ///<自动折行模式
            TextOverflowMode overflow_mode          =TextOverflowMode::Overflow;                    ///<溢出处理模式
            uint32_t        max_lines               =0;                                             ///<最大行数(0=不限制)

            // ── 装饰线(纯色 TILE) ──────────────────────────────────────────
            // 下划线/删除线以图集纯色 tile 作为"虚拟字符"实例绘制，颜色跟随文本主色
            // (复用该文本的 CharStyle，无需新增 GPU 样式)。比例 <=0 表示用内置默认值。
            bool            underline               =false;                                         ///<启用下划线
            bool            strikethrough           =false;                                         ///<启用删除线
            float           underline_thickness     =0.0f;                                          ///<下划线粗细(相对字高比例，<=0 用默认约 6%)
            float           underline_position      =0.0f;                                          ///<下划线位置(相对行顶的字高比例，<=0 用默认约 92%)
            float           strikethrough_thickness =0.0f;                                          ///<删除线粗细(相对字高比例，<=0 用默认约 6%)
            float           strikethrough_position  =0.0f;                                          ///<删除线位置(相对行顶的字高比例，<=0 用默认约 50%)

            auto operator <=> (const ParagraphStyle &)const=default;
        };//struct ParagraphStyle

        struct TextDrawStyle
        {
            ParagraphStyle para_style;

            // CharDrawStyle与TextLayoutAttribute的区别在于：

            // CharStyle    是针对单个字符的绘制风格，其所有的属性都将储存于UBO，在Shader中使用。
            //                  所以每个CharDrawStyle其实都对应了一个TextRender的材质实例(MaterialInstance)。
            //                  也因此CharDrawStyle的数量是有限制的，因为所有的材质实例属性加起来不能超过一个UBO大小。
            //                  但其实这个值也还很大的，100来个还是可以支撑的。

            // ParagraphStyle 是针对整段文本的排版属性，其所有的值都在CPU阶段进行计算。所以其数量其实是无限制的。

            TEXT_COORD_VEC start_position;   //起始位置

            TEXT_COORD_TYPE char_height;
            TEXT_COORD_TYPE space_size;
            TEXT_COORD_TYPE full_space_size;
            TEXT_COORD_TYPE tab_size;
            TEXT_COORD_TYPE char_gap;
            TEXT_COORD_TYPE line_gap;
            TEXT_COORD_TYPE line_height;

            uint16_t style_id = 0;          ///<索引 CharStyle SSBO 的样式 ID

            float extra_advance_x = 0.0f;   ///<额外水平间距（像素），用于 bold/outline 扩展
            float extra_advance_y = 0.0f;   ///<额外垂直间距（像素），用于 bold/outline 扩展

            float scale = 1.0f;             ///<缩放因子（1.0=原始大小，2.0=双倍）

            int32_t rotation = 0;            ///<旋转角度 (0/90/180/270)

            // ── 矩形排版约束(由 ParagraphStyle 解析后传入，单位=像素) ──
            TEXT_COORD_TYPE rect_width  = 0;                                        ///<排版矩形宽度(0=不限制)
            TEXT_COORD_TYPE rect_height = 0;                                        ///<排版矩形高度(0=不限制)
            TextWrapMode    wrap_mode   = TextWrapMode::None;                        ///<自动折行模式
            TextOverflowMode overflow_mode = TextOverflowMode::Overflow;             ///<溢出处理模式
            TextVerticalAlign v_align   = TextVerticalAlign::Top;                    ///<垂直对齐
            uint32_t        max_lines   = 0;                                        ///<最大行数(0=不限制)

            auto operator <=> (const TextDrawStyle &)const=default;
        };

        /**
        * 文本度量结果(像素)
        *
        * width/height 是排版后内容实际占用的尺寸，不等同于矩形尺寸：
        * 未设置矩形时即为自由排版尺寸；设置了矩形时仍返回内容尺寸，
        * 便于 GUI 做自适应尺寸、居中和滚动判定。
        */
        struct TextMetrics
        {
            TEXT_COORD_TYPE width       = 0;        ///<内容宽度
            TEXT_COORD_TYPE height      = 0;        ///<内容高度
            int             line_count  = 0;        ///<行数(竖排为列数)
            bool            truncated   = false;    ///<是否因矩形/最大行数被截断
        };//struct TextMetrics

        /**
        * 解析排版风格：把 ParagraphStyle 中与字符高度相关的系数换算成像素。
        * TextRenderPipeline 与 GUI 层共用同一份解析逻辑，避免两处公式漂移。
        */
        void BuildDrawStyle(TextDrawStyle &out_style,
                            const ParagraphStyle &para_style,
                            const TEXT_COORD_VEC &start_pos,
                            const int char_height,
                            const uint16_t style_id = 0,
                            const CharStyle *char_style = nullptr);

        /**
        * 纯测量：不注册字形、不建 UV、不需要 TileFont
        *
        * 使用 FontSource 的字形度量走与 TextLayout 完全相同的折行/对齐/截断规则，
        * 供 GUI 在布局阶段(拿到图集之前)做自适应尺寸计算。
        *
        * @param out_metrics 输出度量结果
        * @param font_source 字体源
        * @param str         要排版的文本(UTF-16)
        * @param style       排版风格(rect_width/rect_height/wrap_mode/overflow_mode/max_lines 均生效)
        * @return 是否计算成功
        */
        bool MeasureTextLayout(TextMetrics &out_metrics,
                               FontSource *font_source,
                               const U16StringView &str,
                               const TextDrawStyle &style);

        /**
        * 解析一条横向装饰线(下划线/删除线)的屏幕矩形(纯函数，不碰图集，便于无 GPU 单测)。
        *
        * @param out_pos   输出线框左上角(与实例 pen 同一坐标空间，屏幕像素)
        * @param out_w     输出线宽(像素)
        * @param out_h     输出线高/粗细(像素)
        * @param vis_left  该行首个可见字符的笔位 X
        * @param vis_right 该行末可见字符的右缘 X(含省略号)
        * @param line_top  该行顶 Y(= 实例 pen_y)
        * @param char_height_scaled 该行缩放后的字高(像素，= ll.char_height)
        * @param thickness_ratio/position_ratio 段落设定比例(<=0 用默认)
        * @param default_thickness_ratio/default_position_ratio 内置默认比例
        * @return 是否值得绘制(线宽<=0 返回 false)
        */
        bool ResolveTextDecoration(TEXT_COORD_VEC &out_pos, uint32_t &out_w, uint32_t &out_h,
                                   int32_t vis_left, int32_t vis_right, int32_t line_top,
                                   int32_t char_height_scaled,
                                   float thickness_ratio, float position_ratio,
                                   float default_thickness_ratio, float default_position_ratio);

        /**
        * 纯色 quad 的 scale 补偿度量(纯函数，供 EmitSolidQuad 与单测共用)。
        *
        * mesh shader 会对 metrics 再乘 cs.scale，故这里写入"未缩放"尺寸，使最终屏幕尺寸 = 期望像素；
        * offset_x 恒 0(左边对齐 pen_x)，offset_y 取基准字高(未缩放)使 quad 顶部落在 pen_y(与 scale 无关)。
        *
        * @param out 输出 CharMetricsInfo(x/y/w/h)
        * @param w_px/h_px 期望的屏幕像素尺寸
        * @param scale 该样式的缩放因子(<=0 视为 1)
        * @param base_char_height 字体基准字高(FontSource::GetCharHeight，未缩放)
        */
        void SolidQuadMetrics(CharMetricsInfo &out,uint32_t w_px,uint32_t h_px,float scale,int base_char_height);
    }//namespace layout
}//namespace hgl::graph
