// ShaderConfig.h — 「编译期配置」的唯一真源与唯一发射器。
//
// ────────────────────────────────────────────────────────────────────────────
// 这一层要解决什么
// ────────────────────────────────────────────────────────────────────────────
// 固定代码着色器架构的最终形态是：**Shader 源码完全静态**，可变的东西只剩下一组
// 编译期宏。于是「这组宏到底有哪些、按什么顺序、什么格式」必须收敛到一处，
// 而不是散落在各个 composer 的 `defines += ...` 拼接语句里。
//
// 本文件定义：
//   · `ShaderConfig`            —— 类型化的配置字段（谁也不能再手写字符串宏）
//   · `EmitShaderConfigDefines` —— 唯一的规范化发射器
//   · `ComputeShaderConfigHash` —— 供缓存/门禁使用的稳定摘要
//
// 因此 `ShaderConfig` 就是用户语境里的 `Generated_DomainConfig`：
// 它的发射结果被填进固定模板的 `{{defines}}` 槽，位置在**所有 `#include` 之前**，
// 所以下游模块的 `#ifndef` 能取到这里的值。
//
// ⚠ 为什么发射成文本而不是落盘一个物理 `.h` 文件
//   · GLSL 的 `#include` 解析依赖 GLSLCompiler 的搜索路径；落盘会引入
//     「生成目录生命周期 / 并发写 / 陈旧文件」三类新问题，而收益只是形式上的。
//   · 单遍编译（读模板 → 填槽 → 编译）不产生中间文件，失败时也不留垃圾。
//   概念上它仍然是「一个只含开关宏的头」，只是以源码块形式内联。
//
// ⚠ 发射顺序是行为的一部分
//   · `MTL_TEX_<NAME>_CHANNELS` 必须在 NTB 等模块的 `#include` 之前出现
//     （`ntb_tangent_vbo_normalmap` 依赖它判断 BC5 法线）。
//   · 阴影 PCF 宏必须早于 `pcf_shadow.glsl` 的 `#ifndef` 兜底。
//   改动字段顺序 = 改动宏顺序 = 可能改动生成的 GLSL。门禁
//   `D2.shader-config-emit` 用固定的规范文本钉死这一点。

#ifndef HGL_MTL_SHADER_CONFIG_INCLUDE
#define HGL_MTL_SHADER_CONFIG_INCLUDE

#include <hgl/type/DataType.h>
#include <hgl/type/String.h>
#include <hgl/type/ValueArray.h>
#include <string>

namespace hgl::graph::mtl
{
    /// 模板族。"None" 表示该模板没有 Define 块（Sky 及其它未外置模板），
    /// 此时**整段配置都不发射**——包括纹理通道、阴影、alpha 宏。
    /// 这是既有行为，不要"顺手补上"。
    /// 四种 profile 互斥：宏的集合由模板族唯一决定，不看字段语义。
    enum class ShaderConfigProfile : unsigned char
    {
        None = 0,
        ForwardLit,     ///< 光照模板：material source / scene lighting 恒为 1
        ForwardUnlit,   ///< 非光照模板：material source 按模块存在性赋值，scene lighting 恒 0
        /// 深度/阴影模板：没有颜色附件、没有光照，provider 开关一个都不发射，
        /// 只发 `HGL_COVERAGE_ONLY 1`（+ 共享的 alpha 三宏）。
        ShadowCaster,
    };

    /// 材质 TOML 里 `channels = n` 的规范化结果（n == 0 的声明不进来）。
    /// 槽顺序 == 材质引用行顺序，因此这里也必须保序——不要换成 Map。
    ///
    /// 名字用定长缓冲而非 `AnsiString`：`hgl::ValueArray` 要求
    /// trivially copyable（POD），而槽名在实际材质里都是极短的标识符。
    /// 超长由 `SetShaderConfigTextureChannelName` 报错，不做静默截断。
    struct ShaderConfigTextureChannel
    {
        static constexpr int MaxNameBytes = 64;

        char          name[MaxNameBytes] = {};
        unsigned char channels = 0;

        // `hgl::ValueArray` 的部分成员（去重添加 / 比较）需要 operator==。
        bool operator==(const ShaderConfigTextureChannel &other) const
        {
            return channels == other.channels && strcmp(name, other.name) == 0;
        }
        bool operator!=(const ShaderConfigTextureChannel &other) const
        {
            return !(*this == other);
        }
    };

    /// 写入槽名。超长返回 false（宁可编译失败，也不要静默改成别的宏名）。
    bool SetShaderConfigTextureChannelName(ShaderConfigTextureChannel &slot, const char *name);

    struct ShaderConfig
    {
        ShaderConfigProfile profile = ShaderConfigProfile::None;

        // ── provider 开关（仅 profile != None 时发射）────────────────────────
        // NTB / AO 的开关位已随「消灭主着色器条件编译」一起删除：
        // 两者现在是所有相关模板的必需槽，换实现 = 换槽模块（空对象补位），
        // 不再是编译期开关。
        bool use_material_source_provider = false;
        bool use_scene_lighting = false;

        // ── 阴影 PCF（仅当模板真带 shadow_provider 槽）──────────────────────
        bool emit_shadow_pcf = false;
        hgl::uint32 shadow_pcf_poisson_taps = 16;
        hgl::uint32 shadow_normal_offset = 1;

        // ── Alpha（三选多，互相独立）────────────────────────────────────────
        bool alpha_test = false;
        float alpha_cutoff = 0.5f;
        bool alpha_dither = false;
        bool alpha_output = false;

        // ── 纹理通道覆盖（按材质声明顺序）───────────────────────────────────
        hgl::ValueArray<ShaderConfigTextureChannel> texture_channels;
    };

    /// 规范化发射：把配置写成一段 `#define` 文本（每行以 '\n' 结尾）。
    /// 配置为空（profile == None）时不会写入任何内容。
    void EmitShaderConfigDefines(const ShaderConfig &config, std::string &out);

    /// 配置摘要。基于规范化文本计算，因此「顺序不同 = 摘要不同」，
    /// 可直接用于着色器缓存键与门禁基线。
    hgl::uint64 ComputeShaderConfigHash(const ShaderConfig &config);
}// namespace hgl::graph::mtl

#endif//HGL_MTL_SHADER_CONFIG_INCLUDE
