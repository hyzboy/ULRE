// MeshShaderTemplate.h — mesh 生成器 GLSL 模板加载（S3）
//
// 动机：mesh 生成器原先把 GLSL 以 `ms += "...\n"` 形式内嵌在 C++ 头文件里
//（CharQuad 103 行 / LineQuad 86 行 …），不可 lint、不可高亮、不能单独喂 glslang、
// 改一行要重编 C++。S3 把**无条件静态主体**外移到 ShaderLibrary/mesh/*.glsl.tmpl，
// C++ 只负责「加载模板 + 填槽 + 追加条件片段」。
//
// 模板语法**刻意极简：只有 {{slot}} 字面替换，没有条件、没有循环、没有嵌套**。
// 一旦支持条件，模板就变成第二套语言，复杂度全量回流（这是 S3 的设计约束，
// 见 doc/shadergen-refactoring-methodology.md 的"模板引擎的诱惑"）。
// 条件性内容（如 varying 写入块）仍由 C++ 按 stage interface 决定后追加。
//
// 模板文件内容 = 生成文本本身（无文件头注释）——保证与改造前**逐字节一致**；
// 文件用途说明写在本头与 doc/，不写进模板文件。

#pragma once

#include <string>

namespace hgl::graph::mtl
{
    /// 加载 ShaderLibrary/mesh/<filename> 模板。
    /// - 首次加载后**进程内缓存**（生成期单线程：材质编译在主线程）
    /// - CRLF → LF 归一化（git 检出行尾策略不影响生成结果）
    /// - 缺文件返回空串并 GLogError；调用方须发射 `#error` 使 glslang 显式失败
    const std::string &GetMeshShaderTemplate(const char *filename);

    /// 把 text 中所有 {{slot}} 替换为 value（字面替换，不解析嵌套）
    void ApplyMeshTemplateSlot(std::string &text, const char *slot, const std::string &value);

    /// 加载并追加模板到 out；缺文件时追加 `#error` 行（显式失败，不静默产出半个 shader）
    void AppendMeshShaderTemplate(std::string &out, const char *filename);
}//namespace hgl::graph::mtl
