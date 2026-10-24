#pragma once

#include <hgl/type/String.h>

namespace hgl::graph::mtl
{
    /**
     * 引擎自带（非用户资产）的材质定义。
     *
     * 为什么这些定义必须**代码化**而不是留在 ShaderLibrary/material/*.material.toml：
     *
     *   1. 它们是**保底链**（MaterialRecipe.h 的 kFallbackMaterialRules）。保底材质一旦
     *      因为资产缺失 / 被误删 / 写错而解析不出来，错误处理本身就会再失败一次 ——
     *      保底物的可依赖性必须高于它所要兜底的东西。
     *   2. 它们的 ID 被运行时**硬编码**引用（TextRenderPipeline 选 SDF / Bitmap 字体，
     *      ECS 测试断言具体 ID）。这类"引擎内部约定"不该受资产目录内容影响。
     *   3. 它们没有作者可调的自由度：全部字段都由领域契约决定（阶段三之二已把纹理槽
     *      形态与 material_data 类型推入 MaterialDomainContract）。资产文件里剩下的只有
     *      领域 + 顶点语义 + 变体开关，属于"引擎内部知识"，写进 C++ 才算放在正确的地方。
     *
     * 代码化的**边界**：内嵌的仍然是完整 schema-4 源码，注册时走**与 .material.toml
     * 完全相同**的解析 + 领域校验管线（ParseMaterialDefinitionFile）。这样做是为了让
     * MaterialDomainContract 保持唯一真源 —— 若改为在 C++ 里手工拼 MaterialDefinition
     * 结构体，领域契约就得在第二处复述一遍（material_source_module_name / ntb_module_name /
     * vertex_node_config / 纹理槽形态…），那正是本架构要消灭的重复。
     * 代码化消灭的是"对磁盘文件的依赖"，不是"对校验管线的依赖"。
     */
    struct BuiltinMaterialDefinitionSource
    {
        /// 诊断标签（日志用），例如 OS_TEXT("builtin:pure_color")。
        const os_char *source_label;

        /// 注册后必须得到的主键。注册期会断言解析结果与之一致（防止内嵌源码被改错 ID）。
        const char *definition_id;

        /// 完整 schema-4 源码，以 NUL 结尾（长度由 strlen 决定）。
        const char *schema4_source;
    };

    /// 内嵌定义份数。
    int GetBuiltinMaterialDefinitionCount() noexcept;

    /// 取第 index 份（越界返回 nullptr）。
    const BuiltinMaterialDefinitionSource *GetBuiltinMaterialDefinitionSource(int index) noexcept;

    /// 按定义 ID 查内嵌定义（未找到返回 nullptr）。
    const BuiltinMaterialDefinitionSource *FindBuiltinMaterialDefinitionSource(
        const char *definition_id) noexcept;
}
