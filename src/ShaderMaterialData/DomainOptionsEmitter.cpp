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

        // ── mesh_options 专用 ───────────────────────────────────────────────

        // 单个 include 路径的合法性。要求是**字面相对路径**：
        // glslang 的 `#include` 不接受宏展开，路径必须原样可解析；引号/换行/反斜杠
        // 会直接把 options 文本撕成两份，所以在发射期就拒绝，而不是等编译。
        bool IsValidModulePath(const char *path)
        {
            if (!path || !path[0])
                return false;
            if (path[0] == '/')
                return false;

            for (const char *p = path; *p; ++p)
            {
                const char c = *p;
                const bool valid = (c >= 'a' && c <= 'z')
                                || (c >= 'A' && c <= 'Z')
                                || (c >= '0' && c <= '9')
                                || c == '_' || c == '/' || c == '.';
                if (!valid)
                    return false;
            }

            // `..` 越出 ShaderLibrary 根目录：允许它等于允许 include 任意文件。
            for (const char *p = path; p[0] && p[1]; ++p)
                if (p[0] == '.' && p[1] == '.')
                    return false;

            const size_t length = strlen(path);
            const char Suffix[] = ".glsl";
            constexpr size_t SuffixLength = sizeof(Suffix) - 1;
            return length > SuffixLength
                && strcmp(path + length - SuffixLength, Suffix) == 0;
        }

        // Stage 1 解码块是**多行**文本：每一非空行都必须是 `#include "<path>"`。
        // 这条校验的存在理由：该块由另一层（VertexABIBuilder）产出，本层只透传；
        // 一旦上游哪天改成"顺手拼点别的东西"，here 会是第一个报警点——否则症状
        // 是主着色器里多出一行非法 GLSL，报错落在 glslang 而看不出产地。
        bool IsValidIncludeBlock(const char *block)
        {
            const char *cursor = block;
            while (*cursor)
            {
                if (*cursor == '\r')
                    return false;

                const char *line_end = cursor;
                while (*line_end && *line_end != '\n')
                    ++line_end;

                const size_t line_length = static_cast<size_t>(line_end - cursor);
                // `#include "` (10) + 至少一个字符的路径 + `"` ⇒ 至少 12。
                if (line_length < 12)
                    return false;
                if (strncmp(cursor, "#include \"", 10) != 0)
                    return false;
                if (cursor[line_length - 1] != '"')
                    return false;

                cursor = (*line_end == '\n') ? (line_end + 1) : line_end;
            }
            return true;
        }

        void AppendInclude(AnsiString &out, const char *module_path)
        {
            out += "#include \"";
            out += module_path;
            out += "\"\n";
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
        if (input.vertex_normal_present)
            AppendDefine(text_buffer, "HGL_MATERIAL_HAS_VERTEX_NORMAL", 1u);

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

        // ── 材质定义自带宏 ──────────────────────────────────────────────────
        // 定义在 `.definition.toml` 的 `defines` 里，旧路径由 BuildCompileDefineDocument
        // 逐条写成 `#define <name> 1`（值恒为 1，不是 key=value 形式）。
        if (input.definition_defines)
        {
            for (uint32 index = 0; index < input.definition_define_count; ++index)
            {
                const char *name = input.definition_defines[index];
                if (!name || !name[0])
                    continue;

                if (!IsValidMacroNameBody(name))
                    return false;

                text_buffer += "#define ";
                text_buffer += name;
                text_buffer += " 1\n";
            }
        }

        // ── 环境光 provider ─────────────────────────────────────────────────
        // 放在最后：主着色器里 frag_options.glsl 位于 bindings 之后、其它公共契约之前，
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

    // ═══════════════════════════════════════════════════════════════════════════
    // mesh_options.glsl
    // ═══════════════════════════════════════════════════════════════════════════
    //
    // 发射顺序是契约（见头文件约束 1）：
    //   Stage 1（全部属性解码） → Stage 2（位置映射） → Stage 3（投影/视口）
    // 三段的依赖方向是单向的：s1 声明 Position/UV/Color/Normal…，s2 消费 Position
    // 产出 GetLocalPos()，s3 消费 GetLocalPos() 产出 GetClipPos()/GetWorldPos()。
    bool EmitMeshOptions(
        const MeshOptionsInput &input,
        AnsiString &out_mesh_options_glsl)
    {
        const bool has_decode = input.vertex_decode_includes
                             && input.vertex_decode_includes[0];
        const bool has_stage2 = input.position_mapping_module
                             && input.position_mapping_module[0];
        const bool has_stage3 = input.stage3_module
                             && input.stage3_module[0];

        // s2/s3 成对出现（见头文件约束 2）。只给一半 ⇒ GetClipPos(GetLocalPos())
        // 少一个函数，主着色器编不过，而错落点离这里很远。
        if (has_stage2 != has_stage3)
            return false;

        // Stage 1 是硬前提：每个 `s2_*` 都声明了
        // `@ulre require GeometryAttribute Position …`，它消费的是 Stage 1 解出的
        // `Position`。没有解码块就等于没有 Position，s2/s3 会在 glslang 里报未声明
        // 标识符——不如在发射期就拒绝。
        if (!has_decode)
            return false;

        if (!IsValidIncludeBlock(input.vertex_decode_includes))
            return false;
        if (has_stage2 && !IsValidModulePath(input.position_mapping_module))
            return false;
        if (has_stage3 && !IsValidModulePath(input.stage3_module))
            return false;

        // 局部缓冲 + 末尾整体赋值：任何一条拒绝路径都不留半截文本。
        AnsiString text_buffer;

        text_buffer += input.vertex_decode_includes;

        // 上游块不保证以 '\n' 收尾，而紧接着要拼下一行 include ——
        // 少了这个换行会把两行 include 粘成一行（GLSL 里是语法错误）。
        const int length = text_buffer.Length();
        if (length > 0 && text_buffer.c_str()[length - 1] != '\n')
            text_buffer += "\n";

        if (has_stage2)
            AppendInclude(text_buffer, input.position_mapping_module);
        if (has_stage3)
            AppendInclude(text_buffer, input.stage3_module);

        out_mesh_options_glsl = text_buffer;
        return true;
    }
}// namespace hgl::graph::mtl
