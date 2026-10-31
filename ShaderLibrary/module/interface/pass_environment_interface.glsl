// @ulre begin
// @ulre name pass_environment_interface
// @ulre kind Shared
// @ulre priority 0
// @ulre uses surface_interface
// @ulre end
// Pass Environment Interface — “渲染环境”与“表面领域”之间的唯一接缝。
//
// ────────────────────────────────────────────────────────────────────────────
// 为什么需要这一层契约
// ────────────────────────────────────────────────────────────────────────────
// 领域 shader（StandardOpaque / Foliage / Terrain / UI2D / HumanSkin）只负责回答
// 一个问题：**“这个片元的表面长什么样？”**（baseColor / normal / metallic /
// roughness / ao / emissive / alpha）。
//
// 它*不*回答“这个片元最终是什么颜色”，因为那个答案取决于**当前 Pass 的渲染环境**：
//   · ForwardLit  → 需要阳光 + CSM 阴影 + IBL 天光 + 直接光 BRDF
//   · GBuffer     → 不做任何光照，只把 SurfaceOutput 打包进多个 RT
//   · Depth/Shadow→ 只有 Alpha-test 剔除有意义，颜色完全丢弃
//   · 纯无光（UI/天空）→ 直接把 SurfaceOutput 当最终色
//
// 把“环境结论”写死进领域 shader，就等于为每个 (领域 × Pass) 组合复制一份源码，
// 这正是我们要消灭的组合爆炸。因此领域 shader 的**最后一行**永远是：
//
//     return EvalPassEnvironment(env);
//
// 由 Pass 决定注入哪个模块来提供 `EvalPassEnvironment`。领域 shader 不认识
// 阳光、阴影、IBL、GI 中的任何一个。
//
// ────────────────────────────────────────────────────────────────────────────
// 谁来提供实现（现状映射）
// ────────────────────────────────────────────────────────────────────────────
// 本头文件只定义契约，实现由 `output_policy` 槽的模块提供，目前对应：
//   · `module/provider/output_policy/output_policy_forward.glsl` —— Forward Lit 环境的输入装配
//   · `module/provider/output_policy/output_policy_flat.glsl`    —— 无光/平光环境
// 具体光照算法仍由 `lighting/*.glsl` 提供（`forward_pbr` / `forward_flat` ...），
// 即“环境装配”与“算法实现”本身就是两层，本契约不吞掉后者。
//
// ⚠ 不要再往 `SurfaceOutput` 里加“与光照有关的字段”。
//   `emissive` 属于表面自发光（物理上是表面属性），可以留；
//   “受光强度”“阴影可见度”“IBL 贡献”之类一旦进 `SurfaceOutput`，
//   这套解耦立刻失效——请改为加进 `PassEnvironmentInput` 之外的实现侧。
//
// ⚠ Depth-only / Shadow 环境不需要颜色输出；此时实现应忽略颜色、仅用
//   `surfaceOut.alpha` 决定 discard，返回值的 RGB 分量无意义。
#ifndef PASS_ENVIRONMENT_INTERFACE_GLSL
#define PASS_ENVIRONMENT_INTERFACE_GLSL

#include "module/interface/surface_interface.glsl"

struct PassEnvironmentInput
{
    // 场景/几何侧输入（世界位置、世界法线、UV、视向、屏幕坐标……）
    // 字段名刻意叫 `geom` 而不是 `surface`：GLSL 结构体只能用**位置**构造，
    // 若这里叫 `surface`，调用点 `PassEnvironmentInput(si, surface, idx)` 里的
    // 第二个实参（真正的 SurfaceOutput）与字段名同音不同义，极易穿错顺序。
    SurfaceInput geom;
    // 领域 shader 产出的纯表面描述；环境实现是它的唯一消费者
    SurfaceOutput surface;
    // 本 draw 的材质数据行号（per-draw 行表索引），供需要接收侧阴影参数/
    // 自发光强度的环境实现取数；不需要它的环境可忽略。
    uint dataIndex;
};

// A pass-selected output_policy module implements:
//   vec4 EvalPassEnvironment(PassEnvironmentInput env)

#endif // PASS_ENVIRONMENT_INTERFACE_GLSL
