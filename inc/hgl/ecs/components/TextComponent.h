#pragma once

#include<hgl/ecs/core/Component.h>
#include<hgl/graph/font/TextLayout.h>
#include<hgl/type/String.h>
#include<cstdint>
#include<utility>

namespace hgl
{
    namespace graph
    {
        class FontSource;
        enum class TextGeometryType : uint8;
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
    class TextComponent : public Component
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
        graph::TextGeometryType geometry_type;      // FixedStyle or StylePerChar

    public:

        explicit TextComponent(const std::string& name = "Text")
            : Component(name)
            , geometry_type(static_cast<graph::TextGeometryType>(0))
        {
        }

        ~TextComponent() override = default;

        const char* GetRenderSystemGroupName() const override { return "Text"; }

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

        graph::TextGeometryType GetGeometryType() const { return geometry_type; }

        void SetGeometryType(graph::TextGeometryType type)
        {
            geometry_type = type;
            TouchChange(static_cast<uint32_t>(TextChange::Layout));
        }
    };//class TextComponent
}//namespace hgl::ecs

