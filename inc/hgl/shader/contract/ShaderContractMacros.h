/// @file ShaderContractMacros.h
/// C++ ↔ GLSL 数值契约的**注解宏**。
///
/// 本目录（`inc/hgl/shader/contract/`）里的枚举是「同一批数字在 C++ 与 GLSL 两侧
/// 必须逐一相等」的唯一真源（Single Source of Truth）：
///
///   * GLSL 侧派生物  —— `ShaderLibrary/contract/*.glsl`（由 `src/Tools/ShaderConstGen` 生成）
///   * C++ 校验侧派生物 —— `inc/hgl/shader/generated/ShaderConstantsParity.gen.h`
///                        （编译期 `static_assert` 与遗留常量的相等性）
///
/// 生成器的**发现**依据就是下面两个注解宏（与 `HGL_ASSET_STRUCT` 同一思路）：
/// 注解只表达「这个枚举/常量的值要导出给着色器」，具体数值、项序、项名全部由
/// Clang AST 直接读出，**不需要逐项标注**。命名/格式化策略（GLSL 宏名前后缀、
/// 数值后缀）在 `src/Tools/ShaderConstGen/shader_const_gen.toml` 里声明。
///
/// 约束（违反即生成器报错）：
///   1. 被注解的枚举必须是 `enum class` 且带显式底层类型（GLSL 侧只支持整数）。
///   2. 每一项必须显式赋值 —— 隐式递增不算契约，改一行会静默挪动后续所有项。
///   3. 本目录的头文件**不得**依赖引擎其它模块（只允许 `<cstdint>` / `<cstddef>`），
///      生成器把本目录当作自包含的 includable 单元来跑 Clang。

#pragma once

/// 标记「该 `enum class` 的枚举值需导出到 GLSL」。
///
/// 用法（宏位于 `class` 与枚举名之间，展开为空）：
///     enum class HGL_SHADER_ENUM SamplerPresetId : uint32_t { ... };
#define HGL_SHADER_ENUM

/// 标记「该 `static constexpr` 常量需导出到 GLSL」。
///
/// 与 `HGL_SHADER_ENUM` 相比这是次要通道：`VarDecl` 的求值结果不在 Clang AST JSON
/// 里，生成器只能解析字面量初始值，故**仅接受整数字面量 / 十六进制字面量**
/// （`= 16`、`= 0xFFFFFFFFu`）。能表达成枚举的一律优先用枚举。
#define HGL_SHADER_CONST
