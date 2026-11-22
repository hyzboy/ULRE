#pragma once

#include<hgl/CoreType.h>
#include<hgl/type/EnumUtil.h>
#include<hgl/type/String.h>
#include<hgl/mtl/MaterialRecipe.h>

// ═══════════════════════════════════════════════════════════════════════════════
// DebugViewMode — 引擎内嵌调试视图通道
// ═══════════════════════════════════════════════════════════════════════════════
//
// ## 它解决什么
//
// 排查「画面不对」时，只有最终颜色图等于把「几何 × 材质 × 光照 × 阴影」全乘在一起，
// 任何一环出错都表现为"画面奇怪"。本模式让引擎用**同一批几何、同一批材质行**再画
// 一遍，但把片元输出换成中间量（世界法线 / 基础色 / 表面参数…），从而得到可量化
// 分析的通道图。
//
// ## 实现口径（改这里之前请先读 doc/debug-analysis-and-view-modes.md）
//
// 调试视图复用主帧的 GPU 剔除结果，在独立离屏目标上绘制：
//
//   · 成像由 `MaterialDomain::DebugAttribute` 领域承担（一份固定的
//     `ShaderLibrary/domain/debug_attribute/debug_attribute.frag`）；
//   · 通道选择走配方的 `compile_defines`（`HGL_DEBUG_VIEW_*`），因此不同通道
//     天然是不同的 program 身份 —— 切回原通道不会串味；
//   · 世界级调度见 `hgl/ecs/support/debug/DebugViewPass.h`，不修改作者配方。
//
// 世界坐标默认由同帧深度与 inverse_vp 反投影。`WorldPosition` 是可选的
// RGBA32F 输出，用于按需直接采样和与深度反投影交叉验证；其实际误差取决于
// shader/interpolation 精度及距离，不承诺固定物理误差。
namespace hgl::graph::mtl
{
    enum class DebugViewMode : uint8
    {
        Off = 0,        ///< 关闭离屏调试视图
        WorldNormal,    ///< 插值顶点世界法线；不是三角形面法线或法线贴图结果
        BaseColor,      ///< PBR 行的 base_color（含 base_color 贴图）
        Surface,        ///< (roughness, metallic, ao)
        Emissive,       ///< 自发光（含 emissive 贴图）
        UV0,            ///< fract(uv0) 映射到 RGB
        WorldPosition,  ///< fragment world position，RGBA32F，alpha 为有效 mask
        ShadingNormal,  ///< StandardOpaque 的法线贴图扰动后世界空间法线

        ENUM_CLASS_RANGE(Off, ShadingNormal)
    };

    constexpr uint32 DebugViewModeCount = static_cast<uint32>(DebugViewMode::RANGE_SIZE);

    /// 稳定名字（配置文件 / JSON-RPC / MCP 参数共用这一份）。
    /// 稳定字符串包括 world_normal、shading_normal 和 world_position。
    const char *GetDebugViewModeName(DebugViewMode mode) noexcept;

    /// 严格解析；未知名返回 false（调用方负责报错，不做静默回退）。
    bool ParseDebugViewMode(const char *name, DebugViewMode &out_mode) noexcept;

    /// 该模式在 `generated/frag_options.glsl` 里发射的宏名；`Off` ⇒ nullptr。
    const char *GetDebugViewCompileDefine(DebugViewMode mode) noexcept;

    /// 是否真的需要切换材质（Off ⇒ false）。
    inline bool IsDebugViewActive(const DebugViewMode mode) noexcept
    {
        return mode != DebugViewMode::Off;
    }

    // ── 调试配方合成 ────────────────────────────────────────────────────────────
    //
    // 把「一份正常材质配方」变成「同一份材质行、不同解读方式」的调试配方：
    //
    //   · domain 换成 DebugAttribute（领域契约决定顶点语义与片元模板）；
    //   · `base_recipe` 的材质行绑定（material_ssbo_binding）**原样保留** ——
    //     调试通道读的就是这份数据（PBRSurface 行与 StandardOpaque 逐字节同构）；
    //   · 贴图绑定**按 StandardOpaque 槽表过滤后保留** —— 这样「纹理没绑上」在
    //     base_color 通道里表现为纯白（bindless fallback 1.0），而不是整块消失；
    //   · `compile_defines` 只放本通道的那一个宏（覆盖 base 的定义，避免叠加）。
    //
    // 返回 false = 输入非法（mode 为 Off / base_recipe 为空 / 目标领域无契约）。
    bool BuildDebugViewRecipe(DebugViewMode mode,
                              const MaterialRecipe *base_recipe,
                              MaterialRecipe &out_recipe);
}//namespace hgl::graph::mtl
