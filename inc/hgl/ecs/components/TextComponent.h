#pragma once

#include<hgl/ecs/core/entity/ComponentRow.h>
#include<hgl/graph/font/TextLayout.h>
#include<hgl/type/RectScope.h>
#include<hgl/type/String.h>
#include<cstdint>
#include<utility>

namespace hgl
{
    namespace graph
    {
        class FontSource;
    }
}

namespace hgl::ecs
{
    /**
     * TextComponent
     *
     * Stores text content and layout/style data for ECS-driven text rendering.
    * Runtime rendering resources are managed separately by TextRenderPipeline.
     */
    class TextComponent : public ComponentRow
    {
    public:

        enum class TextChange : uint32_t
        {
            Content = 1u << 0,
            Style   = 1u << 1,
            Layout  = 1u << 2,
            Font    = 1u << 3,
        };

    private:

        U16String text;

        graph::layout::ParagraphStyle paragraph_style;
        graph::layout::CharStyle char_style;
        graph::layout::TEXT_COORD_VEC start_position{0, 0};

        graph::FontSource* font_source = nullptr;   // Not owned

    public:

        TextComponent() = default;

        /// 非虚（T2-c：组件行里没有 vptr）；对象住**类型化对象池**，析构由池显式调用
        ~TextComponent() = default;

        // 系统组（`Text`）由**类型表**声明（`kComponentTypeTable[ComponentType::Text].group`）：
        // T2-c 起组归属是类型属性，不再由组件对象上的虚函数声明（虚表已随基类删除）。

        // ── 变更计数 / 变更位 ──
        // T2-c：原 `Component` 基类的 `version` / `change_mask` 当时只有线条与文本在用
        // ⇒ 搬进真正用它的类型；**线条侧已于 2026-10-11 审计删除（只写不读，收集判据是 lines.empty()）**，
        // 文本是唯一活消费者；文本是 2D 专用（未来 2D 单开一套管理），这里保持最小耦合。
        uint64_t version = 0;
        uint32_t change_mask = 0;

        void TouchChange(uint32_t mask) { ++version; change_mask |= mask; }

    public:

        uint32_t GetChangeMask() const { return change_mask; }
        void ClearAllChanges() { change_mask = 0; }

    public:

    public:

        const U16String& GetText() const { return text; }

        void SetText(const U16String& value)
        {
            text = value;
            TouchChange(static_cast<uint32_t>(TextChange::Content));
        }

        void SetText(U16String&& value)
        {
            text = std::move(value);
            TouchChange(static_cast<uint32_t>(TextChange::Content));
        }

        const graph::layout::ParagraphStyle& GetParagraphStyle() const { return paragraph_style; }

        void SetParagraphStyle(const graph::layout::ParagraphStyle& value)
        {
            paragraph_style = value;
            TouchChange(static_cast<uint32_t>(TextChange::Layout));
        }

        const graph::layout::CharStyle& GetCharStyle() const { return char_style; }

        void SetCharStyle(const graph::layout::CharStyle& value)
        {
            char_style = value;
            TouchChange(static_cast<uint32_t>(TextChange::Style));
        }

        const graph::layout::TEXT_COORD_VEC& GetStartPosition() const { return start_position; }

        void SetStartPosition(const graph::layout::TEXT_COORD_VEC& value)
        {
            start_position = value;
            TouchChange(static_cast<uint32_t>(TextChange::Layout));
        }

        graph::FontSource* GetFontSource() const { return font_source; }

        void SetFontSource(graph::FontSource* fs)
        {
            font_source = fs;
            TouchChange(static_cast<uint32_t>(TextChange::Font));
        }

    public: // ── 矩形(RECT)排版 ──────────────────────────────────────────

        /**
        * 设置排版矩形
        *
        * 左/上边写入起始位置，宽/高作为内容区约束交给排版引擎。
        * 宽或高为 0 表示该方向不限制(与旧版自由排版完全等价)。
        * 内边距由控件层处理：传入的应当已经是内容区矩形。
        */
        void SetLayoutRect(const hgl::math::RectScope2i& rect)
        {
            start_position = {rect.GetLeft(), rect.GetTop()};
            paragraph_style.rect_width  = rect.GetWidth();
            paragraph_style.rect_height = rect.GetHeight();
            TouchChange(static_cast<uint32_t>(TextChange::Layout));
        }

        hgl::math::RectScope2i GetLayoutRect() const
        {
            return hgl::math::RectScope2i(start_position.x, start_position.y,
                                          paragraph_style.rect_width, paragraph_style.rect_height);
        }

        /// 自动折行模式(需要先设置矩形宽度)
        void SetTextWrap(graph::layout::TextWrapMode mode)
        {
            paragraph_style.wrap_mode = mode;
            TouchChange(static_cast<uint32_t>(TextChange::Layout));
        }

        graph::layout::TextWrapMode GetTextWrap() const { return paragraph_style.wrap_mode; }

        /// 溢出处理(Clip/Ellipsis 才会按矩形高度与最大行数截断)
        void SetTextOverflow(graph::layout::TextOverflowMode mode)
        {
            paragraph_style.overflow_mode = mode;
            TouchChange(static_cast<uint32_t>(TextChange::Layout));
        }

        graph::layout::TextOverflowMode GetTextOverflow() const { return paragraph_style.overflow_mode; }

        /// 最大行数(0=不限制)
        void SetMaxLines(uint32_t lines)
        {
            paragraph_style.max_lines = lines;
            TouchChange(static_cast<uint32_t>(TextChange::Layout));
        }

        uint32_t GetMaxLines() const { return paragraph_style.max_lines; }

        /// 水平对齐(Left/Center/Right；Fill 按 Left 处理)
        void SetHorizontalAlign(graph::layout::TextAlign align)
        {
            paragraph_style.align = align;
            TouchChange(static_cast<uint32_t>(TextChange::Layout));
        }

        graph::layout::TextAlign GetHorizontalAlign() const { return paragraph_style.align; }

        /// 垂直对齐(需要先设置矩形高度)
        void SetVerticalAlign(graph::layout::TextVerticalAlign align)
        {
            paragraph_style.v_align = align;
            TouchChange(static_cast<uint32_t>(TextChange::Layout));
        }

        graph::layout::TextVerticalAlign GetVerticalAlign() const { return paragraph_style.v_align; }
    };//class TextComponent

    /// T2-c：登记类型号（**CPU-only**：无 GPU 行、不写 `EntityGPU.row[]`）。
    template<> struct ComponentTypeOf<TextComponent> { static constexpr ComponentType value = ComponentType::Text; };
}//namespace hgl::ecs

