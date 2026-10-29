#pragma once

/**
 * GPU 缓冲结构注解（UBO / SSBO）
 *
 * 设计口径（2026-10-29 定稿）：
 *   UBO / SSBO 结构体是**整块纯数据**，因此标注只加在**结构体一级**，
 *   **不需要**逐字段标注 —— 字段名、类型、顺序、偏移、C++ 缺省初始值
 *   全部由 Clang AST + 记录布局（`-fdump-record-layouts`）自动提取。
 *
 * 唯一真源：C++ 结构定义。派生产物（不得手改）：
 *   · GLSL 结构 / `buffer_reference` 声明  →  `ShaderLibrary/<kind>/<Struct>.glsl`
 *   · TOML 读写                             →  `inc/hgl/graph/generated/` (Toml.gen.h)
 *   · FlatBuffers schema                    →  `inc/hgl/graph/generated/` (.fbs)
 *
 * 生成器：`src/Tools/BufferGen/buffer_gen.py`（详见该目录 README）。
 *
 * ── 注解取值（生成器按字符串前缀解析）────────────────────────────────────────
 *   hgl::ubo         UBO 参数块，std140 布局
 *   hgl::ssbo        SSBO 行 / 数组元素，std430(scalar) 布局
 *   hgl::struct      嵌套 GPU 结构（被上面两者引用，自身不成块）
 *   hgl::cpu_only    位于 ubo/ ssbo/ 目录但**不上 GPU**（CPU 侧状态 / 聚合体）
 *
 * 修饰键（逗号分隔，跟在主键后）：
 *   bda=1            以 `buffer_reference` + BDA 地址访问（无绑定无集）
 *   access=readonly  GLSL 侧生成 `readonly buffer`
 *   align=N          显式 `buffer_reference_align=N` / 行对齐断言
 *
 * ── 非 Clang 编译器 ──────────────────────────────────────────────────────────
 * 全部退化为空宏，零警告、零 ABI 影响（与 `CMScript/inc/hgl/Reflect.h` 同策略）。
 */

#if defined(__clang__)
    #define HGL_UBO             [[clang::annotate("hgl::ubo")]]
    #define HGL_UBO_BDA         [[clang::annotate("hgl::ubo,bda=1,access=readonly")]]
    #define HGL_SSBO            [[clang::annotate("hgl::ssbo")]]
    #define HGL_SSBO_BDA        [[clang::annotate("hgl::ssbo,bda=1,access=readonly")]]
    #define HGL_SSBO_ROW        [[clang::annotate("hgl::ssbo,bda=1,access=readwrite")]]
    #define HGL_GPU_STRUCT      [[clang::annotate("hgl::struct")]]
    #define HGL_CPU_ONLY        [[clang::annotate("hgl::cpu_only")]]
#else
    #define HGL_UBO
    #define HGL_UBO_BDA
    #define HGL_SSBO
    #define HGL_SSBO_BDA
    #define HGL_SSBO_ROW
    #define HGL_GPU_STRUCT
    #define HGL_CPU_ONLY
#endif
