// DomainOptions 回归门
//
// 与 src/Tools/ShaderTooling/ 下的工具不同，本 target **只链接 ULRE.ShaderMaterialData**：
// 它断言的是「frag_options.glsl 的发射与生成器无关」——即使 ShaderGen 被物理删除，
// 这条用例也必须照常构建、照常通过。
//
// 用例覆盖：四种 profile 的逐字节文本、纯度（只含 #define / #include）、
// 数字格式（float 六位小数）、channels==0 跳过、非法槽名拒绝、
// ForwardLit 缺 ambient 路径拒绝、profile 路由与渲染状态映射；
// 以及 mesh_options.glsl 的逐字节文本与纯度（严格只含 #include 模块行）。

#include <hgl/mtl/DomainOptions.h>

#include <hgl/filesystem/FileSystem.h>
#include <hgl/filesystem/Path.h>
#include <hgl/io/FileOutputStream.h>
#include <hgl/log/Log.h>
#include <hgl/log/Logger.h>

#include <cstdio>
#include <cstring>

using namespace hgl;
using namespace hgl::graph::mtl;

namespace
{
    int g_pass = 0;
    int g_fail = 0;

    void Check(const bool success, const char *name)
    {
        if (success)
        {
            ++g_pass;
            GLogInfo(u8"[PASS] %s", name);
            return;
        }

        ++g_fail;
        GLogError(u8"[FAIL] %s", name);
    }

    void CheckText(
        const char *name,
        const char *expected,
        const DomainOptionsInput &input)
    {
        AnsiString text;
        if (!EmitDomainOptions(input, text))
        {
            g_fail++;
            GLogError(u8"[FAIL] %s (EmitDomainOptions returned false)", name);
            return;
        }

        if (text == expected)
        {
            g_pass++;
            GLogInfo(u8"[PASS] %s", name);
            return;
        }

        g_fail++;
        GLogError(u8"[FAIL] %s", name);
        GLogError(u8"  expected:\n%s", expected);
        GLogError(u8"  actual:\n%s", text.c_str());
    }

    // frag_options.glsl 的文本同时是 program 身份的一部分，所以它必须只有一种形态：
    // 每行一条 `#define ` 或一行 `#include "`，无注释、无空行、无 CR、以 '\n' 收尾。
    bool IsPureOptionsText(const AnsiString &text)
    {
        const char *raw = text.c_str();
        if (!raw || !raw[0])
            return true;

        const int length = static_cast<int>(strlen(raw));
        if (raw[length - 1] != '\n')
            return false;

        int line_start = 0;
        for (int index = 0; index < length; ++index)
        {
            const char c = raw[index];
            if (c == '\r')
                return false;
            if (c != '\n')
                continue;

            if (index == line_start)
                return false;

            const char *line = raw + line_start;
            const int line_length = index - line_start;
            const bool is_define = (line_length > 8)
                && strncmp(line, "#define ", 8) == 0;
            const bool is_include = (line_length > 12)
                && strncmp(line, "#include \"", 10) == 0;
            if (!is_define && !is_include)
                return false;

            line_start = index + 1;
        }

        return line_start == length;
    }

    // mesh_options.glsl 的纯度：每行必须严格是一条 `#include "`，
    // 无任何 #define、无注释、无空行、无 CR、以 '\n' 收尾。
    // （特殊拓扑如 LineQuad 的缺省实现在其自身的 body 头部自持，不污染通用 options）
    bool IsPureMeshOptionsText(const AnsiString &text)
    {
        const char *raw = text.c_str();
        if (!raw || !raw[0])
            return true;

        const int length = static_cast<int>(strlen(raw));
        if (raw[length - 1] != '\n')
            return false;

        int line_start = 0;
        for (int index = 0; index < length; ++index)
        {
            const char c = raw[index];
            if (c == '\r')
                return false;
            if (c != '\n')
                continue;

            if (index == line_start)
                return false;

            const char *line = raw + line_start;
            const int line_length = index - line_start;
            if (line_length <= 12 || strncmp(line, "#include \"", 10) != 0)
                return false;

            line_start = index + 1;
        }

        return line_start == length;
    }

    void RunProfileRouting()
    {
        Check(ResolveDomainOptionsProfile(true, "pbr_surface_source",
                                          "ntb_tangent_vbo_normalmap")
                  == DomainOptionsProfile::ShadowCaster,
              "route: depth pass wins over everything else");
        Check(ResolveDomainOptionsProfile(false, "pbr_surface_source",
                                          "ntb_tangent_vbo_normalmap")
                  == DomainOptionsProfile::ForwardLit,
              "route: NTB provider present -> ForwardLit");
        Check(ResolveDomainOptionsProfile(false, "unlit_source", nullptr)
                  == DomainOptionsProfile::ForwardUnlit,
              "route: material source without NTB -> ForwardUnlit");
        Check(ResolveDomainOptionsProfile(false, nullptr, nullptr)
                  == DomainOptionsProfile::None,
              "route: neither -> None (Sky)");
        Check(ResolveDomainOptionsProfile(false, "", "")
                  == DomainOptionsProfile::None,
              "route: empty names behave like nullptr");
        Check(ResolveDomainOptionsProfile(true, nullptr, nullptr)
                  == DomainOptionsProfile::ShadowCaster,
              "route: depth pass with no modules is still ShadowCaster");
    }

    void RunRenderStateMapping()
    {
        ResolvedMaterialRenderState state;
        state.alpha_test = true;
        state.alpha_cutoff = 0.25f;
        state.dither = true;
        state.pipeline_config.alpha_blend = true;

        DomainOptionsInput input;
        ApplyRenderStateToDomainOptions(input, state);

        Check(input.alpha_test && input.alpha_dither && input.alpha_output,
              "state: alpha_test / dither / alpha_blend all mapped");
        Check(input.alpha_cutoff == 0.25f, "state: alpha_cutoff mapped");

        ResolvedMaterialRenderState opaque;
        DomainOptionsInput opaque_input;
        ApplyRenderStateToDomainOptions(opaque_input, opaque);
        Check(!opaque_input.alpha_test && !opaque_input.alpha_dither
                  && !opaque_input.alpha_output,
              "state: default state maps to all-false (opaque)");
    }

    void RunStandardOpaqueForward()
    {
        MaterialTextureDeclaration declarations[3];
        declarations[0].name = "normal";
        declarations[0].channels = 2;
        declarations[1].name = "base_color";
        declarations[1].channels = 0;   // 0 = 由纹素格式决定 ⇒ 整条跳过
        declarations[2].name = "opacity_mask";
        declarations[2].channels = 1;

        DomainOptionsInput input;
        input.profile = DomainOptionsProfile::ForwardLit;
        input.ambient_include_path = "lighting/indirect_sky_ambient.glsl";
        input.emit_shadow_pcf = true;
        input.shadow_pcf_poisson_taps = 16;
        input.shadow_normal_offset = 1;
        input.texture_declarations = declarations;
        input.texture_declaration_count = 3;

        CheckText("forward: standard opaque PBR",
                  "#define MTL_TEX_NORMAL_CHANNELS 2\n"
                  "#define MTL_TEX_OPACITY_MASK_CHANNELS 1\n"
                  "#define HGL_SHADOW_PCF_POISSON_TAPS 16\n"
                  "#define HGL_SHADOW_NORMAL_OFFSET 1\n"
                  "#include \"lighting/indirect_sky_ambient.glsl\"\n",
                  input);

        DomainOptionsInput masked = input;
        masked.alpha_test = true;
        masked.alpha_cutoff = 0.5f;
        masked.alpha_dither = true;
        masked.alpha_output = true;

        // 六位小数是硬约束：std::to_string(float) → "%f" → 0.500000
        CheckText("forward: alpha test 0.5 emits six decimals",
                  "#define MTL_TEX_NORMAL_CHANNELS 2\n"
                  "#define MTL_TEX_OPACITY_MASK_CHANNELS 1\n"
                  "#define HGL_SHADOW_PCF_POISSON_TAPS 16\n"
                  "#define HGL_SHADOW_NORMAL_OFFSET 1\n"
                  "#define HGL_ALPHA_TEST 1\n"
                  "#define HGL_ALPHA_CUTOFF 0.500000\n"
                  "#define HGL_ALPHA_DITHER 1\n"
                  "#define HGL_ALPHA_OUTPUT 1\n"
                  "#include \"lighting/indirect_sky_ambient.glsl\"\n",
                  masked);

        DomainOptionsInput fallback = input;
        fallback.emit_shadow_pcf = false;
        fallback.face_normal_fallback = true;
        fallback.texture_declarations = nullptr;
        fallback.texture_declaration_count = 0;

        CheckText("forward: face-normal fallback without shadow PCF",
                  "#define HGL_MATERIAL_FACE_NORMAL_FALLBACK 1\n"
                  "#include \"lighting/indirect_sky_ambient.glsl\"\n",
                  fallback);

        // 换 ambient provider（环境层选择）⇒ 只有 include 行变化
        DomainOptionsInput ibl = fallback;
        ibl.ambient_include_path = "lighting/indirect_ibl.glsl";
        CheckText("forward: ambient provider swap only changes include line",
                  "#define HGL_MATERIAL_FACE_NORMAL_FALLBACK 1\n"
                  "#include \"lighting/indirect_ibl.glsl\"\n",
                  ibl);

        // ForwardLit 缺 ambient 路径 ⇒ 必须拒绝（否则编译期才炸）
        DomainOptionsInput missing_ambient = input;
        missing_ambient.ambient_include_path = nullptr;
        AnsiString rejected;
        Check(!EmitDomainOptions(missing_ambient, rejected)
                  && rejected.IsEmpty(),
              "forward: missing ambient path is rejected, output untouched");

        DomainOptionsInput empty_ambient = input;
        empty_ambient.ambient_include_path = "";
        AnsiString rejected_empty;
        Check(!EmitDomainOptions(empty_ambient, rejected_empty),
              "forward: empty ambient path is rejected");
    }

    void RunStandardOpaqueShadow()
    {
        MaterialTextureDeclaration declarations[1];
        declarations[0].name = "normal";
        declarations[0].channels = 2;

        DomainOptionsInput opaque;
        opaque.profile = DomainOptionsProfile::ShadowCaster;
        // 通道声明存在也必须被忽略：深度 pass 的 NTB 槽恒为 ntb_none
        opaque.texture_declarations = declarations;
        opaque.texture_declaration_count = 1;
        opaque.emit_shadow_pcf = true;

        CheckText("shadow: opaque caster only emits coverage flag",
                  "#define HGL_COVERAGE_ONLY 1\n",
                  opaque);

        DomainOptionsInput masked = opaque;
        masked.alpha_test = true;
        masked.alpha_cutoff = 0.5f;

        CheckText("shadow: masked caster adds alpha test/cutoff",
                  "#define HGL_COVERAGE_ONLY 1\n"
                  "#define HGL_ALPHA_TEST 1\n"
                  "#define HGL_ALPHA_CUTOFF 0.500000\n",
                  masked);

        DomainOptionsInput dither = opaque;
        dither.alpha_dither = true;
        CheckText("shadow: dither caster emits only HGL_ALPHA_DITHER",
                  "#define HGL_COVERAGE_ONLY 1\n"
                  "#define HGL_ALPHA_DITHER 1\n",
                  dither);
    }

    void RunSky()
    {
        DomainOptionsInput sky;
        sky.profile = DomainOptionsProfile::None;
        // 即使是 None，也要容忍调用方塞进来的无意义字段
        sky.ambient_include_path = "lighting/indirect_ibl.glsl";
        sky.emit_shadow_pcf = true;
        sky.alpha_test = true;

        CheckText("sky: None profile emits nothing at all", "", sky);
    }

    void RunForwardUnlit()
    {
        DomainOptionsInput unlit;
        unlit.profile = DomainOptionsProfile::ForwardUnlit;
        unlit.alpha_test = true;
        unlit.alpha_cutoff = 0.5f;

        CheckText("unlit: no ambient include, alpha test only",
                  "#define HGL_ALPHA_TEST 1\n"
                  "#define HGL_ALPHA_CUTOFF 0.500000\n",
                  unlit);
    }

    void RunRejections()
    {
        MaterialTextureDeclaration bad[1];
        bad[0].name = "bad-name";     // 连字符进不了宏名
        bad[0].channels = 2;

        DomainOptionsInput input;
        input.profile = DomainOptionsProfile::ForwardUnlit;
        input.texture_declarations = bad;
        input.texture_declaration_count = 1;

        AnsiString text;
        Check(!EmitDomainOptions(input, text),
              "reject: illegal texture slot name");

        MaterialTextureDeclaration empty_name[1];
        empty_name[0].channels = 2;
        DomainOptionsInput empty_input = input;
        empty_input.texture_declarations = empty_name;
        AnsiString empty_text;
        Check(!EmitDomainOptions(empty_input, empty_text),
              "reject: empty texture slot name");
    }

    void RunPurityAndDeterminism()
    {
        MaterialTextureDeclaration declarations[2];
        declarations[0].name = "normal";
        declarations[0].channels = 2;
        declarations[1].name = "base_color";
        declarations[1].channels = 0;

        DomainOptionsInput input;
        input.profile = DomainOptionsProfile::ForwardLit;
        input.ambient_include_path = "lighting/indirect_sky_ambient.glsl";
        input.emit_shadow_pcf = true;
        input.alpha_test = true;
        input.alpha_dither = true;
        input.alpha_output = true;
        input.face_normal_fallback = true;
        input.texture_declarations = declarations;
        input.texture_declaration_count = 2;

        AnsiString first;
        AnsiString second;
        Check(EmitDomainOptions(input, first) && EmitDomainOptions(input, second)
                  && first == second,
              "purity: emission is deterministic");

        Check(IsPureOptionsText(first),
              "purity: only #define / #include lines, no blank lines, LF endings");

        const char *expected_macros[] = {
            "HGL_MATERIAL_FACE_NORMAL_FALLBACK",
            "HGL_ALPHA_TEST",
            "HGL_ALPHA_CUTOFF",
            "HGL_ALPHA_DITHER",
            "HGL_ALPHA_OUTPUT",
            "HGL_SHADOW_PCF_POISSON_TAPS",
            "HGL_SHADOW_NORMAL_OFFSET",
            "MTL_TEX_NORMAL_CHANNELS",
        };
        bool all_present = true;
        for (const char *macro : expected_macros)
        {
            const AnsiString needle = AnsiString("#define ") + macro + " ";
            if (!strstr(first.c_str(), needle.c_str()))
            {
                all_present = false;
                GLogError(u8"  missing macro: %s", macro);
            }
        }
        Check(all_present, "purity: every expected macro is emitted");
        Check(strstr(first.c_str(), "HGL_USE_MATERIAL_SOURCE_PROVIDER") == nullptr
                  && strstr(first.c_str(), "HGL_USE_SCENE_LIGHTING") == nullptr,
              "purity: dead HGL_USE_* macros are not emitted");
    }

    // ═══════════════════════════════════════════════════════════════════════════
    // mesh_options.glsl
    // ═══════════════════════════════════════════════════════════════════════════

    void CheckMeshText(
        const char *name,
        const AnsiString &expected,
        const MeshOptionsInput &input)
    {
        AnsiString text;
        if (!EmitMeshOptions(input, text))
        {
            g_fail++;
            GLogError(u8"[FAIL] %s (EmitMeshOptions returned false)", name);
            return;
        }

        if (text == expected)
        {
            g_pass++;
            GLogInfo(u8"[PASS] %s", name);
            return;
        }

        g_fail++;
        GLogError(u8"[FAIL] %s", name);
        GLogError(u8"  expected:\n%s", expected.c_str());
        GLogError(u8"  actual:\n%s", text.c_str());
    }

    // 负例还要断言"拒绝时不留半截文本"：out 必须与调用前逐字节一致。
    void CheckMeshRejected(const char *name, const MeshOptionsInput &input)
    {
        AnsiString text("SENTINEL");
        if (EmitMeshOptions(input, text))
        {
            g_fail++;
            GLogError(u8"[FAIL] %s (expected rejection, got: %s)", name, text.c_str());
            return;
        }

        if (text != "SENTINEL")
        {
            g_fail++;
            GLogError(u8"[FAIL] %s (rejected but clobbered output)", name);
            return;
        }

        g_pass++;
        GLogInfo(u8"[PASS] %s", name);
    }

    void RunMeshOptions()
    {
        // ── 正例：三种位置形态 × 典型属性组合 ────────────────────────────────
        {
            MeshOptionsInput input;
            input.vertex_decode_includes =
                "#include \"vertex/s1_color.glsl\"\n"
                "#include \"vertex/s1_position_vec3.glsl\"\n";
            input.position_mapping_module = "vertex/s2_passthrough3d.glsl";
            input.stage3_module = "vertex/s3_world_camera_vp.glsl";

            CheckMeshText("meshopts: 3D passthrough + vertex color",
                          "#include \"vertex/s1_color.glsl\"\n"
                          "#include \"vertex/s1_position_vec3.glsl\"\n"
                          "#include \"vertex/s2_passthrough3d.glsl\"\n"
                          "#include \"vertex/s3_world_camera_vp.glsl\"\n",
                          input);
        }

        {
            MeshOptionsInput input;
            input.vertex_decode_includes =
                "#include \"vertex/s1_color.glsl\"\n"
                "#include \"vertex/s1_position_vec2.glsl\"\n";
            input.position_mapping_module = "vertex/s2_ndc_lift.glsl";
            input.stage3_module = "vertex/s3_l2w_only.glsl";

            CheckMeshText("meshopts: 2D vec2 NDC lift + L2W only",
                          "#include \"vertex/s1_color.glsl\"\n"
                          "#include \"vertex/s1_position_vec2.glsl\"\n"
                          "#include \"vertex/s2_ndc_lift.glsl\"\n"
                          "#include \"vertex/s3_l2w_only.glsl\"\n",
                          input);
        }

        {
            // DrawTriangle：int16 打包顶点 + 像素坐标 → 局部坐标 → 正交视口
            MeshOptionsInput input;
            input.vertex_decode_includes =
                "#include \"vertex/s1_color.glsl\"\n"
                "#include \"vertex/s1_position_vec2i.glsl\"\n";
            input.position_mapping_module = "vertex/s2_pixel_to_local.glsl";
            input.stage3_module = "vertex/s3_ortho_viewport.glsl";

            CheckMeshText("meshopts: vec2i pixel-to-local + ortho viewport",
                          "#include \"vertex/s1_color.glsl\"\n"
                          "#include \"vertex/s1_position_vec2i.glsl\"\n"
                          "#include \"vertex/s2_pixel_to_local.glsl\"\n"
                          "#include \"vertex/s3_ortho_viewport.glsl\"\n",
                          input);
        }

        {
            // SimpleSphere：RG16F UV + RG8 八面体法线 —— 六边形过滤器
            MeshOptionsInput input;
            input.vertex_decode_includes =
                "#include \"vertex/s1_uv_rg16f.glsl\"\n"
                "#include \"vertex/s1_ntb_rg8.glsl\"\n"
                "#include \"vertex/s1_position_vec3.glsl\"\n";
            input.position_mapping_module = "vertex/s2_passthrough3d.glsl";
            input.stage3_module = "vertex/s3_world_camera_vp.glsl";

            CheckMeshText("meshopts: RG16F uv + RG8 octahedral normal",
                          "#include \"vertex/s1_uv_rg16f.glsl\"\n"
                          "#include \"vertex/s1_ntb_rg8.glsl\"\n"
                          "#include \"vertex/s1_position_vec3.glsl\"\n"
                          "#include \"vertex/s2_passthrough3d.glsl\"\n"
                          "#include \"vertex/s3_world_camera_vp.glsl\"\n",
                          input);
        }

        {
            // VertexPalette：调色板索引 + 骨骼变换索引 + 逐实例尺寸
            MeshOptionsInput input;
            input.vertex_decode_includes =
                "#include \"vertex/s1_palette_index.glsl\"\n"
                "#include \"vertex/s1_transform_id.glsl\"\n"
                "#include \"vertex/s1_size.glsl\"\n"
                "#include \"vertex/s1_position_vec3.glsl\"\n";
            input.position_mapping_module = "vertex/s2_passthrough3d.glsl";
            input.stage3_module = "vertex/s3_world_camera_vp.glsl";

            CheckMeshText("meshopts: palette + transform id + size chain",
                          "#include \"vertex/s1_palette_index.glsl\"\n"
                          "#include \"vertex/s1_transform_id.glsl\"\n"
                          "#include \"vertex/s1_size.glsl\"\n"
                          "#include \"vertex/s1_position_vec3.glsl\"\n"
                          "#include \"vertex/s2_passthrough3d.glsl\"\n"
                          "#include \"vertex/s3_world_camera_vp.glsl\"\n",
                          input);
        }

        {
            MeshOptionsInput input;
            input.vertex_decode_includes =
                "#include \"vertex/s1_luminance.glsl\"\n"
                "#include \"vertex/s1_position_vec3.glsl\"\n";
            input.position_mapping_module = "vertex/s2_passthrough3d.glsl";
            input.stage3_module = "vertex/s3_world_camera_vp.glsl";

            CheckMeshText("meshopts: luminance attribute",
                          "#include \"vertex/s1_luminance.glsl\"\n"
                          "#include \"vertex/s1_position_vec3.glsl\"\n"
                          "#include \"vertex/s2_passthrough3d.glsl\"\n"
                          "#include \"vertex/s3_world_camera_vp.glsl\"\n",
                          input);
        }

        {
            // Text2D CharQuad：s1_text_char_quad.glsl 自带整条 SSBO/变换链路，
            // 所以 s2/s3 皆空是唯一合法的"无成对"形态。
            MeshOptionsInput input;
            input.vertex_decode_includes =
                "#include \"vertex/s1_text_char_quad.glsl\"\n";

            CheckMeshText("meshopts: char quad is self-contained (no s2/s3)",
                          "#include \"vertex/s1_text_char_quad.glsl\"\n",
                          input);
        }

        {
            // 上游解码块不以 '\n' 收尾时，必须补一个——否则两行 include 粘成一行。
            MeshOptionsInput input;
            input.vertex_decode_includes = "#include \"vertex/s1_color.glsl\"";
            input.position_mapping_module = "vertex/s2_ndc_lift.glsl";
            input.stage3_module = "vertex/s3_l2w_only.glsl";

            CheckMeshText("meshopts: missing trailing newline is repaired",
                          "#include \"vertex/s1_color.glsl\"\n"
                          "#include \"vertex/s2_ndc_lift.glsl\"\n"
                          "#include \"vertex/s3_l2w_only.glsl\"\n",
                          input);
        }

        // ── 纯度与确定性 ─────────────────────────────────────────────────────
        {
            MeshOptionsInput input;
            input.vertex_decode_includes =
                "#include \"vertex/s1_uv.glsl\"\n"
                "#include \"vertex/s1_ntb.glsl\"\n"
                "#include \"vertex/s1_position_vec3.glsl\"\n";
            input.position_mapping_module = "vertex/s2_passthrough3d.glsl";
            input.stage3_module = "vertex/s3_world_camera_vp.glsl";

            AnsiString first;
            AnsiString second;
            Check(EmitMeshOptions(input, first) && EmitMeshOptions(input, second)
                      && first == second,
                  "meshopts: emission is deterministic");
            Check(IsPureMeshOptionsText(first),
                  "meshopts: only #include lines, no blank lines, LF endings");
        }

        // ── 负例 ─────────────────────────────────────────────────────────────
        {
            MeshOptionsInput input;
            CheckMeshRejected("meshopts: reject all-empty input", input);
        }
        {
            MeshOptionsInput input;
            input.vertex_decode_includes = "#include \"vertex/s1_color.glsl\"\n";
            input.position_mapping_module = "vertex/s2_ndc_lift.glsl";
            CheckMeshRejected("meshopts: reject s2 without s3", input);
        }
        {
            MeshOptionsInput input;
            input.vertex_decode_includes = "#include \"vertex/s1_color.glsl\"\n";
            input.stage3_module = "vertex/s3_l2w_only.glsl";
            CheckMeshRejected("meshopts: reject s3 without s2", input);
        }
        {
            // s2/s3 存在但解码块缺失：GetLocalPos() 拿不到 Position。
            MeshOptionsInput input;
            input.position_mapping_module = "vertex/s2_passthrough3d.glsl";
            input.stage3_module = "vertex/s3_world_camera_vp.glsl";
            CheckMeshRejected("meshopts: reject no stage-1 decode block", input);
        }
        {
            MeshOptionsInput input;
            input.vertex_decode_includes = "#include \"vertex/s1_color.glsl\"\n";
            input.position_mapping_module = "vertex/s2_ndc_lift.glsl";
            input.stage3_module = "vertex/s3_l2w_only";
            CheckMeshRejected("meshopts: reject module path without .glsl", input);
        }
        {
            MeshOptionsInput input;
            input.vertex_decode_includes = "#include \"vertex/s1_color.glsl\"\n";
            input.position_mapping_module = "vertex/s2_ndc_lift.glsl";
            input.stage3_module = "vertex/../s3_l2w_only.glsl";
            CheckMeshRejected("meshopts: reject parent-directory escape", input);
        }
        {
            MeshOptionsInput input;
            input.vertex_decode_includes = "#include \"vertex/s1_color.glsl\"\n";
            input.position_mapping_module = "vertex/s2_ndc_lift.glsl";
            input.stage3_module = "/abs/s3_l2w_only.glsl";
            CheckMeshRejected("meshopts: reject absolute module path", input);
        }
        {
            // 引号会把 options 文本撕成两份 ⇒ 发射期拒绝。
            MeshOptionsInput input;
            input.vertex_decode_includes = "#include \"vertex/s1_color.glsl\"\n";
            input.position_mapping_module = "vertex/s2_ndc_lift.glsl";
            input.stage3_module = "vertex/s3\"_l2w_only.glsl";
            CheckMeshRejected("meshopts: reject quote in module path", input);
        }
        {
            MeshOptionsInput input;
            input.vertex_decode_includes = "vertex/s1_color.glsl\n";
            input.position_mapping_module = "vertex/s2_ndc_lift.glsl";
            input.stage3_module = "vertex/s3_l2w_only.glsl";
            CheckMeshRejected("meshopts: reject non-#include line in decode block", input);
        }
        {
            MeshOptionsInput input;
            input.vertex_decode_includes =
                "#include \"vertex/s1_color.glsl\"\n"
                "\n"
                "#include \"vertex/s1_position_vec2.glsl\"\n";
            input.position_mapping_module = "vertex/s2_ndc_lift.glsl";
            input.stage3_module = "vertex/s3_l2w_only.glsl";
            CheckMeshRejected("meshopts: reject blank line in decode block", input);
        }
        {
            // CR 会随文本进入 program 身份 ⇒ 与片段侧同一套口径，一律拒绝。
            MeshOptionsInput input;
            input.vertex_decode_includes =
                "#include \"vertex/s1_color.glsl\"\r\n";
            input.position_mapping_module = "vertex/s2_ndc_lift.glsl";
            input.stage3_module = "vertex/s3_l2w_only.glsl";
            CheckMeshRejected("meshopts: reject CR in decode block", input);
        }
    }

    // ── --write-options ────────────────────────────────────────────────────
    // 给 scripts/ci/verify-domain-master-shaders.ps1 产出**由发射器生成**的
    // frag_options.glsl，让等价性验收直接消费生产代码，而不是脚本里手抄的宏表。
    // 目录布局与脚本约定一致： <root>/<variant>/generated/frag_options.glsl
    struct OptionsVariant
    {
        const char *dir_name;
        DomainOptionsProfile profile;
        bool alpha_test;
    };

    // AnsiString 为空时 c_str() 可能是 nullptr，不能用 strlen 取长度。
    bool WriteOptionsFile(const OSString &path, const AnsiString &text)
    {
        io::FileOutputStream *stream = io::CreateFileOutputStream(path);
        if (!stream)
            return false;

        const int length = text.Length();
        const bool written =
            (length <= 0) ? true : stream->Write(text.c_str(), length) == length;
        delete stream;
        return written;
    }

    int WriteOptionsVariants(const char *root_dir)
    {
        static const OptionsVariant Variants[] = {
            {"fwd",           DomainOptionsProfile::ForwardLit,   false},
            {"fwd-alpha",     DomainOptionsProfile::ForwardLit,   true },
            {"shadow",        DomainOptionsProfile::ShadowCaster, false},
            {"shadow-masked", DomainOptionsProfile::ShadowCaster, true },
            {"sky",           DomainOptionsProfile::None,         false},
        };

        MaterialTextureDeclaration normal_map;
        normal_map.name = "normal";
        normal_map.channels = 2;

        const filesystem::Path root(ToOSString(root_dir));
        int failures = 0;

        for (const OptionsVariant &variant : Variants)
        {
            DomainOptionsInput input;
            input.profile = variant.profile;

            if (variant.profile == DomainOptionsProfile::ForwardLit)
            {
                input.ambient_include_path = "lighting/indirect_sky_ambient.glsl";
                input.texture_declarations = &normal_map;
                input.texture_declaration_count = 1;
                input.emit_shadow_pcf = true;
            }
            input.alpha_test = variant.alpha_test;

            AnsiString text;
            if (!EmitDomainOptions(input, text))
            {
                GLogError(u8"[FAIL] --write-options: %s 发射失败", variant.dir_name);
                ++failures;
                continue;
            }

            const filesystem::Path dir =
                root / ToOSString(variant.dir_name) / ToOSString("generated");
            filesystem::MakePath(dir.ToOSString());

            if (!WriteOptionsFile((dir / ToOSString("frag_options.glsl")).ToOSString(), text))
            {
                GLogError(u8"[FAIL] --write-options: %s 写入失败", variant.dir_name);
                ++failures;
                continue;
            }

            GLogInfo(u8"[write-options] %s (%d bytes)",
                     variant.dir_name, text.Length());
        }

        return failures == 0 ? 0 : 1;
    }
}// namespace

int main(int argc, char **argv)
{
    const char *write_options_dir = nullptr;
    for (int index = 1; index < argc; ++index)
    {
        if (strcmp(argv[index], "--write-options") == 0 && index + 1 < argc)
            write_options_dir = argv[++index];
    }

    if (!logger::InitLogger(OS_TEXT("DomainOptionsRegression")))
        return 20;

    if (write_options_dir)
        return WriteOptionsVariants(write_options_dir);

    RunProfileRouting();
    RunRenderStateMapping();
    RunStandardOpaqueForward();
    RunStandardOpaqueShadow();
    RunSky();
    RunForwardUnlit();
    RunRejections();
    RunPurityAndDeterminism();
    RunMeshOptions();

    GLogInfo(u8"[DomainOptionsRegression] PASS=%d FAIL=%d", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
