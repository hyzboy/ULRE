#include <hgl/mtl/DomainOptions.h>

#include <cstdio>
#include <cstring>

namespace hgl::graph::mtl
{
    namespace
    {
        // 数字格式必须与历史实现逐字节一致（见头文件约束 2）：
        //   uint  → std::to_string(uint32) → "%u"
        //   float → std::to_string(float)  → 提升 double → "%f" → 六位小数
        // snprintf 在这里是格式化工具，不是日志输出（日志一律走 GLog*）。
        void AppendDefine(AnsiString &out, const char *macro, const uint32 value)
        {
            char line[160];
            snprintf(line, sizeof(line), "#define %s %u\n", macro, value);
            out += line;
        }

        void AppendDefine(AnsiString &out, const char *macro, const float value)
        {
            char line[192];
            snprintf(line, sizeof(line), "#define %s %f\n",
                     macro, static_cast<double>(value));
            out += line;
        }

        // 与历史实现一致：只把 a-z 抬成大写，其余字符原样保留。
        // 不要换成通用 toupper —— 那会依赖 locale。
        bool BuildTextureChannelMacro(
            char *out, const size_t out_size, const char *name)
        {
            static const char Prefix[] = "MTL_TEX_";
            static const char Suffix[] = "_CHANNELS";
            constexpr size_t PrefixLength = sizeof(Prefix) - 1;
            constexpr size_t SuffixLength = sizeof(Suffix) - 1;

            if (!name)
                return false;

            const size_t name_length = strlen(name);
            if (PrefixLength + name_length + SuffixLength + 1 > out_size)
                return false;

            memcpy(out, Prefix, PrefixLength);
            for (size_t index = 0; index < name_length; ++index)
            {
                const char c = name[index];
                out[PrefixLength + index] =
                    (c >= 'a' && c <= 'z') ? static_cast<char>(c - 'a' + 'A') : c;
            }
            memcpy(out + PrefixLength + name_length, Suffix, SuffixLength + 1);
            return true;
        }

        // 槽名最终要拼进宏名，非法字符会在 GLSL 里炸成语法错误。
        // 比"编译期才发现"早一步在发射期报错。
        bool IsValidMacroNameBody(const char *name)
        {
            if (!name || !name[0])
                return false;

            for (const char *p = name; *p; ++p)
            {
                const char c = *p;
                const bool valid = (c >= 'a' && c <= 'z')
                                || (c >= 'A' && c <= 'Z')
                                || (c >= '0' && c <= '9')
                                || c == '_';
                if (!valid)
                    return false;
            }
            return true;
        }
    }// namespace

    DomainOptionsProfile ResolveDomainOptionsProfile(
        const bool depth_purpose,
        const char *material_source_module_name,
        const char *ntb_provider_module_name) noexcept
    {
        if (depth_purpose)
            return DomainOptionsProfile::ShadowCaster;

        if (ntb_provider_module_name && ntb_provider_module_name[0])
            return DomainOptionsProfile::ForwardLit;

        if (material_source_module_name && material_source_module_name[0])
            return DomainOptionsProfile::ForwardUnlit;

        return DomainOptionsProfile::None;
    }

    bool EmitDomainOptions(
        const DomainOptionsInput &input,
        AnsiString &out_options_glsl)
    {
        // 无 define 块的主着色器族（Sky 等）：整段不发射。
        // 这不是遗漏 —— Sky 的固定 include 表既无 ambient 槽也无阴影/alpha 通路，
        // 补上这些宏只会让"宏存在即非零"的 #if defined() 误判。
        if (input.profile == DomainOptionsProfile::None)
        {
            out_options_glsl.Clear();
            return true;
        }

        const bool shadow_pass = (input.profile == DomainOptionsProfile::ShadowCaster);

        // 环境光 provider 是 options 里的**唯一**一行 include（字面路径，见头文件）。
        // ForwardLit 少了它 → 主着色器里 EvalAmbientLight 未定义 → 编译期才炸，
        // 所以这里提前拦住。
        if (input.profile == DomainOptionsProfile::ForwardLit
         && (!input.ambient_include_path || !input.ambient_include_path[0]))
            return false;

        // 局部缓冲 + 末尾整体赋值：任何一条拒绝路径都不会留下半截文本。
        AnsiString text_buffer;

        // ── 覆盖标记 ────────────────────────────────────────────────────────
        // 深度/阴影 pass 的 surface provider 只需 alpha，不产出光照 —— 主着色器对
        // material_surface.glsl 的 EvalSurface 调用由 #ifndef HGL_COVERAGE_ONLY 关掉。
        if (shadow_pass)
            AppendDefine(text_buffer, "HGL_COVERAGE_ONLY", 1u);

        // ── 顶点法线降级 ────────────────────────────────────────────────────
        if (input.face_normal_fallback)
            AppendDefine(text_buffer, "HGL_MATERIAL_FACE_NORMAL_FALLBACK", 1u);

        // ── 纹理通道覆盖 ────────────────────────────────────────────────────
        // 深度/阴影 pass 整段跳过：它的 NTB 槽恒为 ntb_none，没有任何模块读通道数，
        // 发射出去只会误触发 ntb_* 的双通道法线重建分支。
        if (!shadow_pass
         && input.texture_declarations
         && input.texture_declaration_count)
        {
            for (uint32 index = 0; index < input.texture_declaration_count; ++index)
            {
                const MaterialTextureDeclaration &declaration =
                    input.texture_declarations[index];

                // "宏存在即非零" ⇒ 0 通道必须整条跳过（见头文件约束 3）。
                if (declaration.channels == 0)
                    continue;

                if (!IsValidMacroNameBody(declaration.name.c_str()))
                    return false;

                char macro[160];
                if (!BuildTextureChannelMacro(
                        macro, sizeof(macro), declaration.name.c_str()))
                    return false;

                AppendDefine(text_buffer, macro,
                             static_cast<uint32>(declaration.channels));
            }
        }

        // ── 阴影 PCF ────────────────────────────────────────────────────────
        // pcf_shadow.glsl 用 #ifndef 兜底 16，因此本段是"覆盖兜底值"而非必需；
        // 发射出来才能让采样档位成为可调项。深度 pass 自己没有 shadow 槽 ⇒ 不发。
        if (!shadow_pass && input.emit_shadow_pcf)
        {
            AppendDefine(text_buffer, "HGL_SHADOW_PCF_POISSON_TAPS",
                         input.shadow_pcf_poisson_taps);
            AppendDefine(text_buffer, "HGL_SHADOW_NORMAL_OFFSET",
                         input.shadow_normal_offset);
        }

        // ── Alpha ───────────────────────────────────────────────────────────
        // cutoff 只在 alpha_test 打开时才有意义（旧路径同口径）。
        if (input.alpha_test)
        {
            AppendDefine(text_buffer, "HGL_ALPHA_TEST", 1u);
            AppendDefine(text_buffer, "HGL_ALPHA_CUTOFF", input.alpha_cutoff);
        }
        if (input.alpha_dither)
            AppendDefine(text_buffer, "HGL_ALPHA_DITHER", 1u);
        if (input.alpha_output)
            AppendDefine(text_buffer, "HGL_ALPHA_OUTPUT", 1u);

        // ── 环境光 provider ─────────────────────────────────────────────────
        // 放在最后：主着色器里 options.glsl 位于 bindings 之后、其它公共契约之前，
        // 该模块只定义 EvalAmbientLight 一个函数，位置不影响编译产物。
        if (input.profile == DomainOptionsProfile::ForwardLit)
        {
            text_buffer += "#include \"";
            text_buffer += input.ambient_include_path;
            text_buffer += "\"\n";
        }

        out_options_glsl = text_buffer;
        return true;
    }
}// namespace hgl::graph::mtl
