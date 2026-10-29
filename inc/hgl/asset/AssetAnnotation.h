#pragma once

/**
 * 资产对象 / 属性反射注解
 *
 * 设计口径（2026-10-29 定稿）：与 `inc/hgl/graph/BufferAnnotation.h` 同族，但面向
 * **资产层对象**（几何规约、图元规约、材质配方、场景编排……），而不是 GPU 缓冲结构。
 *
 * 三条规则：
 *   1. **C++ 声明是唯一真源**。注解决定「这个结构可序列化」，字段清单、类型、顺序、
 *      类内缺省值全部由 Clang AST 自动提取，因此**不需要逐字段标注**。
 *   2. **打在结构上 = 整个结构可序列化**。派生产物（不得手改）：
 *        · TOML 读写             → `inc/hgl/asset/generated/<Group>Toml.gen.h`
 *        · FlatBuffers schema    → `inc/hgl/asset/generated/<group>_schema.fbs`
 *        · FlatBuffers 编解码    → `inc/hgl/asset/generated/<Group>Flat.gen.h`
 *        · 属性元数据（GUI/文档）→ `inc/hgl/asset/generated/<Group>Meta.gen.h`
 *   3. **`HGL_PROPERTY(...)` 只是可选增强**：不给也能序列化，给了才带编辑器/文档元数据。
 *
 * 生成器：`src/Tools/AssetGen/asset_gen.py`（详见该目录 README）。
 *
 * ── 用法 ────────────────────────────────────────────────────────────────────
 * ```cpp
 * #include <hgl/asset/AssetAnnotation.h>
 *
 * struct HGL_ASSET_STRUCT BuiltinSphereParams
 * {
 *     uint32_t slices = 32;                                       // 自动序列化
 *
 *     HGL_PROPERTY(EditAnywhere, Category = "Shape", Range(1, 512),
 *                  DisplayName = "Slices", Tooltip = "经线分段数")
 *     uint32_t rings = 16;
 *
 *     HGL_PROPERTY(Transient)                                     // 不进 TOML / FBS
 *     uint32_t cached_hash = 0;
 * };
 * ```
 *
 * ── 属性标签（`HGL_PROPERTY(...)` 里逗号分隔）──────────────────────────────
 * | 标签 | 含义 |
 * |---|---|
 * | `EditAnywhere`      | 可编辑（任何实例） |
 * | `EditDefaultsOnly`  | 仅可在原型/资产上编辑 |
 * | `VisibleAnywhere`   | 只读展示 |
 * | `ReadOnly`          | 编辑器只读 |
 * | `EditOnly`          | 仅编辑器使用，发行版可剔除（须放在 `#if HGL_ASSET_EDITOR` 内） |
 * | `Transient`         | 不参与序列化（派生缓存、运行时句柄） |
 * | `Category = "X"`    | 检查器分组名 |
 * | `DisplayName = "X"` | 检查器显示名（缺省用字段名） |
 * | `Tooltip = "X"`     | 悬浮说明（缺省取 `///<` 尾注释） |
 * | `Range(min, max)`   | 软范围（滑块上下限） |
 * | `Clamp(min, max)`   | 硬范围（超出即拒绝） |
 * | `Step = v`          | 拖动步长 |
 * | `AssetRef = "geo"`  | 该字符串字段是资产引用（`geo`/`primitive`/`recipe`/`tex`/`pack`） |
 * | `Key = "k"`         | TOML 键名覆盖（缺省用字段名） |
 *
 * ── 非 Clang 编译器 ─────────────────────────────────────────────────────────
 * 全部退化为空宏，零警告、零 ABI 影响（与 `CMScript/inc/hgl/Reflect.h` 同策略）。
 * 因此 MSVC 构建**不需要** Clang 参与：派生产物是提交进仓库的普通头文件。
 */

#if defined(__clang__)
    #define HGL_ASSET_STRUCT    [[clang::annotate("hgl::asset_struct")]]
    #define HGL_ASSET_CLASS     [[clang::annotate("hgl::asset_class")]]
    #define HGL_PROPERTY(...)   [[clang::annotate("hgl::prop," #__VA_ARGS__)]]
#else
    #define HGL_ASSET_STRUCT
    #define HGL_ASSET_CLASS
    #define HGL_PROPERTY(...)
#endif
