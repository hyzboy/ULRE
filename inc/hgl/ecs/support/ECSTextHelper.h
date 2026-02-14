#pragma once

#include<hgl/ecs/components/TextComponent.h>
#include<hgl/type/String.h>

namespace hgl::ecs
{
    class ECSContext;

    /**
     * ECSTextHelper - 简化的ECS文本提交API
     *
     * 用户只需提交文本内容和排版参数，无需管理TextRender/TextGeometry/Primitive
     */
    class ECSTextHelper
    {
    public:

        /**
         * 提交一条文本到ECS
         * @param world ECS上下文
         * @param text 要显示的文本
         * @param font_source 字体源（使用CreateFontSource等创建）
         * @param start_pos 起始位置
         * @param char_style 字符样式（可选，使用默认样式）
         * @param para_style 段落样式（可选，使用默认样式）
         * @return 新建的TextComponent，可用于后续更新
         */
        static std::shared_ptr<TextComponent> SubmitText(
            ECSContext* world,
            const U16String& text,
            hgl::graph::FontSource* font_source,
            const hgl::graph::layout::TEXT_COORD_VEC& start_pos = {0, 0},
            const hgl::graph::layout::CharStyle& char_style = hgl::graph::layout::CharStyle{},
            const hgl::graph::layout::ParagraphStyle& para_style = hgl::graph::layout::ParagraphStyle{});

        /**
         * 提交多条文本到ECS
         * @param world ECS上下文
         * @param texts 要显示的文本列表
         * @param positions 每条文本的起始位置
         * @param font_source 字体源
         * @param char_style 字符样式（所有文本共用）
         * @param para_style 段落样式（所有文本共用）
         */
        static void SubmitTexts(
            ECSContext* world,
            const std::vector<U16String>& texts,
            const std::vector<hgl::graph::layout::TEXT_COORD_VEC>& positions,
            hgl::graph::FontSource* font_source,
            const hgl::graph::layout::CharStyle& char_style = hgl::graph::layout::CharStyle{},
            const hgl::graph::layout::ParagraphStyle& para_style = hgl::graph::layout::ParagraphStyle{});

        /**
         * 更新已提交的文本内容
         * @param text_comp 要更新的TextComponent
         * @param new_text 新的文本内容
         */
        static void UpdateText(
            const std::shared_ptr<TextComponent>& text_comp,
            const U16String& new_text);

        /**
         * 更新已提交的文本样式
         * @param text_comp 要更新的TextComponent
         * @param char_style 新的字符样式
         */
        static void UpdateStyle(
            const std::shared_ptr<TextComponent>& text_comp,
            const hgl::graph::layout::CharStyle& char_style);
    };
}//namespace hgl::ecs

