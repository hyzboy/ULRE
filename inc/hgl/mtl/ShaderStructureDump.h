#pragma once

/// ShaderStructureDump.h — 编译产物的**结构化快照**（S4 试点）
///
/// 目的：把回归门的断言从「GLSL 文本子串」降维到「结构快照 + golden 比对」。
/// 现状问题：回归门 5,007 行里大量断言形如 `contains(fs, "layout(set=2, binding=0)")`，
/// 生成器任何格式变动都要改断言 → 生成器与测试双向锁死。
///
/// 设计约束：
///   1. **不新增真源**——快照只是既有结构（ShaderBuildContext 的编译期信号 /
///      模块列表 / 有效 varying）的视图。
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

    /// 生成「发射顺序快照」：把两个 stage 的最终 GLSL 压成一条有序的骨架事实流。
    ///
    /// 与 `DumpShaderStructure` 互补——那份只看「有什么」（模块、varying、信号），
    /// 这份只看「按什么顺序拼」：
    ///   - `#version` / `#extension` / `#define` / `#include` / `#if*` 的先后；
    ///   - 顶层 `layout(...)` 限定的先后；
    ///   - 顶层函数签名的先后。
    ///
    /// 之所以需要它：`#include` 顺序与宏定义顺序是 ShaderGen 拼接的**隐式契约**
    /// （宏先于被它影响的声明、`scene_ubo` 先于消费它的模块），而这些顺序在
    /// 源码里只体现为「几行 += 的先后」，重构时极易在无人察觉的情况下互换。
    ///
    /// 粒度选择：只记录骨架行，函数体、注释、空行、缩进一律不入快照；行内连续
    /// 空白折叠成一个空格。于是「把骨架外置成 `.tmpl`」「把拼接收敛到统一出口」
    /// 这类**不改变 token 顺序**的重构，golden 不必重刷——这正是它作为安全网的
    /// 前提：安全网本身不能是每次重构都要改的东西。
    std::string DumpShaderEmissionOrder(const ShaderBuildContext &ctx, const char *label);
}//namespace hgl::graph::mtl
