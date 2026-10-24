#include <hgl/mtl/ShaderConfig.h>
#include <hgl/util/hash/FNV1a.h>

namespace hgl::graph::mtl
{
    namespace
    {
        void AppendDefine(std::string &out, const char *macro, const char *value)
        {
            out += "#define ";
            out += macro;
            out += ' ';
            out += value;
            out += '\n';
        }

        void AppendDefine(std::string &out, const char *macro, const uint32 value)
        {
            out += "#define ";
            out += macro;
            out += ' ';
            out += std::to_string(value);
            out += '\n';
        }

        void AppendDefine(std::string &out, const char *macro, const float value)
        {
            // 必须与历史实现逐字节一致：历史上是 std::to_string(float)，
            // 隐式提升为 double 后按 "%f" 输出六位小数（如 0.500000）。
            out += "#define ";
            out += macro;
            out += ' ';
            out += std::to_string(value);
            out += '\n';
        }

        // 与历史实现一致：只把 a-z 抬成大写，其余字符原样保留。
        // 材质槽名是下划线小写标识符，因此等价于"全大写"，但不要换成
        // 通用 toupper —— 那会依赖 locale。
        std::string UpperSlotName(const char *name)
        {
            std::string macro;
            for (const char *p = name; p && *p; ++p)
                macro += (*p >= 'a' && *p <= 'z')
                    ? static_cast<char>(*p - 'a' + 'A') : *p;
            return macro;
        }
    }// namespace

    bool SetShaderConfigTextureChannelName(ShaderConfigTextureChannel &slot, const char *name)
    {
        if (!name)
            return false;

        const size_t length = strlen(name);
        if (length + 1 > static_cast<size_t>(ShaderConfigTextureChannel::MaxNameBytes))
            return false;

        memcpy(slot.name, name, length + 1);
        return true;
    }

    void EmitShaderConfigDefines(const ShaderConfig &config, std::string &out)
    {
        // 无 Define 块的模板族（Sky 等）：整段配置都不发射。
        // 这不是遗漏，而是既有行为：Sky 模板既没有 provider 槽，也没有
        // 阴影/alpha 通路，补上这些宏只会让 "存在即非零" 的 #if defined() 判断误判。
        if (config.profile == ShaderConfigProfile::None)
            return;

        // ── provider 开关 ────────────────────────────────────────────────
        // ForwardLit 恒为 1；ForwardUnlit 按模块存在性赋值。因此这里按 profile
        // 发射，而**不是**按 ShaderConfig 里的布尔字段——后者仅供摘要/检查使用。
        //
        // NTB / AO 两个开关宏已随「消灭主着色器条件编译」删除：两者现在是所有
        // 相关模板的必需槽，缺省由空对象模块（ntb/ntb_none.glsl、ao/identity.glsl）
        // 补位，着色器里没有任何 #if 读它们。
        if (config.profile == ShaderConfigProfile::ForwardLit)
        {
            AppendDefine(out, "HGL_USE_MATERIAL_SOURCE_PROVIDER", 1u);
            AppendDefine(out, "HGL_USE_SCENE_LIGHTING", 1u);
        }
        else
        {
            AppendDefine(out, "HGL_USE_MATERIAL_SOURCE_PROVIDER",
                         config.use_material_source_provider ? 1u : 0u);
            AppendDefine(out, "HGL_USE_SCENE_LIGHTING", 0u);
        }

        // ── 纹理通道覆盖 ────────────────────────────────────────────────
        // 宏名形如 MTL_TEX_NORMAL_CHANNELS=2；"宏存在即非零" 让下游可以
        // 用 #if defined(MTL_TEX_NORMAL_CHANNELS) 判断，因此 channels==0
        // 的声明必须整条跳过（不能发射成 0）。
        for (int index = 0; index < config.texture_channels.GetCount(); ++index)
        {
            const ShaderConfigTextureChannel &channel = config.texture_channels[index];
            if (channel.channels == 0)
                continue;

            std::string macro = "MTL_TEX_";
            macro += UpperSlotName(channel.name);
            macro += "_CHANNELS";
            AppendDefine(out, macro.c_str(), static_cast<uint32>(channel.channels));
        }

        // ── 阴影 PCF ────────────────────────────────────────────────────
        // 只有模板真的带 shadow_provider 槽时才发射；该块落在模板的
        // {{defines}}（早于 {{module_includes}}），pcf_shadow.glsl 的
        // #ifndef 兜底值取这里的值。
        if (config.emit_shadow_pcf)
        {
            AppendDefine(out, "HGL_SHADOW_PCF_POISSON_TAPS",
                         config.shadow_pcf_poisson_taps);
            AppendDefine(out, "HGL_SHADOW_NORMAL_OFFSET",
                         config.shadow_normal_offset);
        }

        // ── Alpha ───────────────────────────────────────────────────────
        if (config.alpha_test)
        {
            AppendDefine(out, "HGL_ALPHA_TEST", 1u);
            AppendDefine(out, "HGL_ALPHA_CUTOFF", config.alpha_cutoff);
        }
        if (config.alpha_dither)
            AppendDefine(out, "HGL_ALPHA_DITHER", 1u);
        if (config.alpha_output)
            AppendDefine(out, "HGL_ALPHA_OUTPUT", 1u);
    }

    uint64 ComputeShaderConfigHash(const ShaderConfig &config)
    {
        std::string text;
        EmitShaderConfigDefines(config, text);

        hgl::hash::FNV1aHasher64 hasher;
        hasher << text;
        return hasher;
    }
}// namespace hgl::graph::mtl
