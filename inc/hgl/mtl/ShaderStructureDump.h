#pragma once

/// ShaderStructureDump.h — 编译产物的**结构化快照**（S4 试点）
///
/// 目的：把回归门的断言从「GLSL 文本子串」降维到「结构快照 + golden 比对」。
/// 现状问题：回归门 5,007 行里大量断言形如 `contains(fs, "layout(set=2, binding=0)")`，
/// 生成器任何格式变动都要改断言 → 生成器与测试双向锁死。
///
/// 设计约束：
///   1. **不新增真源**——快照只是既有结构（ShaderResourceSchema /
///      DescriptorSetLayoutAllocator / ShaderBuildContext）的视图。
///   2. **不含 hash 值与 GLSL 文本**——hash 随任何文本变动而变，写进 golden 会让
///      golden 每次都要重刷，失去意义；只记录「结构」（谁存在、在哪个 set/binding、
///      给哪些 stage、是否 required）。
///   3. **顺序稳定**——资源按 (set, binding, name) 排序输出，不依赖内部容器顺序。
///
/// 输出为行式文本（每行一个事实），便于 golden 文件逐行 diff 与人工审阅。

#include <hgl/mtl/ShaderBuildContext.h>
#include <string>

namespace hgl::graph::mtl
{
    /// 生成结构快照。label 由调用方给出（通常 "<definition_id> purpose=... pass=..."）。
    std::string DumpShaderStructure(const ShaderBuildContext &ctx, const char *label);
}//namespace hgl::graph::mtl
