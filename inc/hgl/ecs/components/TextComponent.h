#pragma once

#include<hgl/ecs/core/ComponentRow.h>
#include<hgl/graph/font/TextLayout.h>
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
    };//class TextComponent

    /// T2-c：登记类型号（**CPU-only**：无 GPU 行、不写 `EntityGPU.row[]`）。
    template<> struct ComponentTypeOf<TextComponent> { static constexpr ComponentType value = ComponentType::Text; };
}//namespace hgl::ecs

