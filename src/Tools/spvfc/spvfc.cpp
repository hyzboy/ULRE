/*
 * spvfc.cpp  —  Shader Flat File Compiler
 *
 * Compiles GLSL or HLSL shader source files to the .spvf format (raw SPIR-V
 * binary bundled with SPIRV-Cross reflection data in a single contiguous
 * buffer) and prints a human-readable reflection summary to stdout.
 *
 * Usage: spvfc [options] <shader_file> [<shader_file2> ...]
 *
 * Options:
 *   -o <file>       Output .spvf path  (single input only)
 *   -e <name>       Entry-point name   [default: main]
 *   -I <dir>        Include root: 1st = shader library root, optional 2nd = the
 *                   root used for `generated/` includes (also -I<dir> form)
 *   -D <macro>      Add preprocessor define (also -D<macro>=value form)
 *   --hlsl          Treat all inputs as HLSL
 *   --vulkan <ver>  Target Vulkan version: 1.0 1.1 1.2 1.3 1.4 (default: 1.4)
 *   --spv <ver>     Target SPIR-V version: 1.0 .. 1.6 (default: 1.6)
 *   --no-output     Compile + reflect but do not write .spvf file
 *   --no-reflect    Suppress reflection text output
 *   --quiet         Suppress informational messages (errors still printed)
 *
 * Examples:
 *   spvfc mesh.vert
 *   spvfc -I shaders/include pbr.frag
 *   spvfc --hlsl -e PSMain -o pbr.spvf pbr.hlsl
 *   spvfc --vulkan 1.2 --spv 1.5 --no-output --no-reflect shader.comp
 *   spvfc -DENABLE_SHADOWS=1 -DLIGHT_COUNT=4 scene.frag
 */

/* 整合后的编译/反射核心：模块内直调命名空间 API，不再有插件 DLL 与
 * extern "C" 函数表。SPVParseFlat.h 由 ULRE.ShaderCompiler 的 PUBLIC
 * include 目录传播，提供 ShaderFlatView / ShaderFlatLoad / SF* 常量。 */
#include <ShaderCompilerCore.h>
#include <SPVParseFlat.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace core = hgl::graph::core;
using hgl::graph::SPVData;

/* =========================================================================
 * Helper: readable names for shader stage flags
 * ========================================================================= */

static const char *stage_name(uint32_t s)
{
    switch (s) {
        case 0x00000001: return "vertex";
        case 0x00000002: return "tessellation_control";
        case 0x00000004: return "tessellation_evaluation";
        case 0x00000008: return "geometry";
        case 0x00000010: return "fragment";
        case 0x00000020: return "compute";
        case 0x00000040: return "task (NV)";
        case 0x00000080: return "mesh (NV)";
        case 0x00000100: return "raygen (KHR)";
        case 0x00000200: return "any_hit (KHR)";
        case 0x00000400: return "closest_hit (KHR)";
        case 0x00000800: return "miss (KHR)";
        case 0x00001000: return "intersection (KHR)";
        case 0x00002000: return "callable (KHR)";
        default:         return "unknown";
    }
}

/* =========================================================================
 * Helper: GLSL-like type string for an SFAttribute
 * ========================================================================= */

static void format_attr_type(char *buf, size_t bufsz,
                              uint8_t basetype, uint8_t vec_size, uint8_t col_count)
{
    const char *pfx =
        (basetype == SF_BASETYPE_INT)    ? "i" :
        (basetype == SF_BASETYPE_UINT)   ? "u" :
        (basetype == SF_BASETYPE_BOOL)   ? "b" :
        (basetype == SF_BASETYPE_DOUBLE) ? "d" : "";

    if (col_count > 1) {
        /* matrix type */
        if (vec_size == col_count)
            snprintf(buf, bufsz, "%smat%u",    pfx, (unsigned)vec_size);
        else
            snprintf(buf, bufsz, "%smat%ux%u", pfx,
                     (unsigned)col_count, (unsigned)vec_size);
    } else if (vec_size > 1) {
        /* vector type */
        snprintf(buf, bufsz, "%svec%u", pfx, (unsigned)vec_size);
    } else {
        /* scalar type */
        switch (basetype) {
            case SF_BASETYPE_FLOAT:  snprintf(buf, bufsz, "float");  break;
            case SF_BASETYPE_DOUBLE: snprintf(buf, bufsz, "double"); break;
            case SF_BASETYPE_INT:    snprintf(buf, bufsz, "int");    break;
            case SF_BASETYPE_UINT:   snprintf(buf, bufsz, "uint");   break;
            case SF_BASETYPE_BOOL:   snprintf(buf, bufsz, "bool");   break;
            default:                 snprintf(buf, bufsz, "?%u", (unsigned)basetype); break;
        }
    }
}

/* =========================================================================
 * Helper: readable name for a VkDescriptorType index
 * ========================================================================= */

static const char *desc_type_name(uint8_t dt)
{
    switch (dt) {
        case SF_DESCRIPTOR_TYPE_SAMPLER:                return "sampler";
        case SF_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER: return "combined_image_sampler";
        case SF_DESCRIPTOR_TYPE_SAMPLED_IMAGE:          return "sampled_image";
        case SF_DESCRIPTOR_TYPE_STORAGE_IMAGE:          return "storage_image";
        case SF_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:   return "uniform_texel_buffer";
        case SF_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:   return "storage_texel_buffer";
        case SF_DESCRIPTOR_TYPE_UNIFORM_BUFFER:         return "uniform_buffer";
        case SF_DESCRIPTOR_TYPE_STORAGE_BUFFER:         return "storage_buffer";
        case SF_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC: return "uniform_buffer_dynamic";
        case SF_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC: return "storage_buffer_dynamic";
        case SF_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:       return "input_attachment";
        default:                                        return "?";
    }
}

/* =========================================================================
 * Print human-readable reflection summary for a ShaderFlatView
 * ========================================================================= */

static void print_reflection(const char *input_path, const ShaderFlatView *v)
{
    char type_buf[32];
    bool any_res = false;

    printf("\n");
    printf("=== Reflection: %s ===\n", input_path);
    printf("Stage      : %s (0x%08X)\n", stage_name(v->shader_stage), v->shader_stage);
    printf("Entry      : %s\n", v->entrypoint);
    printf("SPIR-V     : %u bytes (%u words)\n\n",
           v->spv_word_count * 4u, v->spv_word_count);

    /* Stage inputs */
    printf("Stage Inputs (%u):\n", v->input_count);
    if (v->input_count == 0) {
        printf("  (none)\n");
    } else {
        for (uint32_t i = 0; i < v->input_count; i++) {
            const SFAttribute *a = &v->inputs[i];
            format_attr_type(type_buf, sizeof(type_buf),
                             a->basetype, a->vec_size, a->col_count);
            printf("  loc=%-3u  %-14s  %s\n",
                   (unsigned)a->location, type_buf, a->name);
        }
    }

    /* Stage outputs */
    printf("\nStage Outputs (%u):\n", v->output_count);
    if (v->output_count == 0) {
        printf("  (none)\n");
    } else {
        for (uint32_t i = 0; i < v->output_count; i++) {
            const SFAttribute *a = &v->outputs[i];
            format_attr_type(type_buf, sizeof(type_buf),
                             a->basetype, a->vec_size, a->col_count);
            printf("  loc=%-3u  %-14s  %s\n",
                   (unsigned)a->location, type_buf, a->name);
        }
    }

    /* Descriptor resources — grouped by type */
    for (uint32_t dt = 0; dt < SF_DESCRIPTOR_TYPE_COUNT; dt++) {
        if (v->resource_count[dt] == 0) continue;
        any_res = true;
        printf("\n%s (%u):\n", desc_type_name((uint8_t)dt), v->resource_count[dt]);
        for (uint32_t i = 0; i < v->resource_count[dt]; i++) {
            const SFDescriptor *d = &v->resources[dt][i];
            if (d->array_size > 0)
                printf("  set=%-2u  binding=%-3u  %s[%u]\n",
                       (unsigned)d->set, (unsigned)d->binding,
                       d->name, (unsigned)d->array_size);
            else
                printf("  set=%-2u  binding=%-3u  %s\n",
                       (unsigned)d->set, (unsigned)d->binding, d->name);
        }
    }
    if (!any_res)
        printf("\nDescriptor Resources: (none)\n");

    /* Push constants */
    printf("\nPush Constants (%u):\n", v->push_constant_count);
    if (v->push_constant_count == 0) {
        printf("  (none)\n");
    } else {
        for (uint32_t i = 0; i < v->push_constant_count; i++) {
            const SFPushConstant *pc = &v->push_constants[i];
            printf("  offset=%-4u  size=%-4u  %s\n",
                   pc->offset, pc->size, pc->name);
        }
    }

    /* Subpass inputs (only printed when present) */
    if (v->subpass_input_count > 0) {
        printf("\nSubpass Inputs (%u):\n", v->subpass_input_count);
        for (uint32_t i = 0; i < v->subpass_input_count; i++) {
            const SFSubpassInput *si = &v->subpass_inputs[i];
            printf("  attachment=%-2u  binding=%-3u  %s\n",
                   (unsigned)si->input_attachment_index,
                   (unsigned)si->binding, si->name);
        }
    }

    printf("\n");
}

/* =========================================================================
 * Version string → glslang constant helpers
 * ========================================================================= */

/* Return the glslang encoding, or 0 when the version is not supported.  An
 * unknown value is a hard error: silently falling back used to let the CLI
 * advertise a target that the compiler did not actually honour. */

static uint32_t parse_vulkan_ver(const char *s)
{
    if (strcmp(s, "1.0") == 0) return (1u << 22);
    if (strcmp(s, "1.1") == 0) return (1u << 22) | (1u << 12);
    if (strcmp(s, "1.2") == 0) return (1u << 22) | (2u << 12);
    if (strcmp(s, "1.3") == 0) return (1u << 22) | (3u << 12);
    if (strcmp(s, "1.4") == 0) return (1u << 22) | (4u << 12);
    fprintf(stderr, "[spvfc] Unsupported Vulkan version '%s' (supported: 1.0 1.1 1.2 1.3 1.4)\n", s);
    return 0;
}

static uint32_t parse_spv_ver(const char *s)
{
    if (strcmp(s, "1.0") == 0) return (1u << 16);
    if (strcmp(s, "1.1") == 0) return (1u << 16) | (1u << 8);
    if (strcmp(s, "1.2") == 0) return (1u << 16) | (2u << 8);
    if (strcmp(s, "1.3") == 0) return (1u << 16) | (3u << 8);
    if (strcmp(s, "1.4") == 0) return (1u << 16) | (4u << 8);
    if (strcmp(s, "1.5") == 0) return (1u << 16) | (5u << 8);
    if (strcmp(s, "1.6") == 0) return (1u << 16) | (6u << 8);
    fprintf(stderr, "[spvfc] Unsupported SPIR-V version '%s' (supported: 1.0 .. 1.6)\n", s);
    return 0;
}

/* =========================================================================
 * CLI option bag
 * ========================================================================= */

struct Options {
    std::vector<std::string> inputs;
    std::string              output;                   /* -o              */
    std::string              entrypoint  = "main";     /* -e              */
    std::vector<std::string> include_dirs;             /* [0] library, [1] generated/ */
    std::string              preamble;                 /* built from -D   */
    bool                     hlsl        = false;
    bool                     no_output   = false;
    bool                     no_reflect  = false;
    bool                     quiet       = false;
    uint32_t                 vulkan_ver  = (1u << 22) | (4u << 12); /* Vulkan 1.4 */
    uint32_t                 spv_ver     = (1u << 16) | (6u << 8);  /* SPIR-V 1.6 */
};

/* =========================================================================
 * Per-file pipeline: compile → reflect → write .spvf → print summary
 * ========================================================================= */

static int process_file(const std::string &input,
                        const std::string &output,
                        const Options     &opts)
{
    /* 1. Detect shader stage from file extension */
    const size_t      dot = input.rfind('.');
    const std::string ext = (dot != std::string::npos) ? input.substr(dot + 1) : "";
    const uint32_t    stage = core::GetShaderStageFlagByExtName(ext.c_str());

    if (stage == 0) {
        fprintf(stderr,
                "[spvfc] Cannot determine shader stage for '%s'"
                " (unknown extension '%s').\n"
                "        Rename the file with a known extension"
                " (vert frag comp geom tesc tese task mesh"
                " rgen rahit rchit rmiss rint rcall)\n",
                input.c_str(), ext.c_str());
        return 1;
    }

    if (!opts.quiet)
        printf("[spvfc] Compiling : %s\n", input.c_str());

    /* 2. Build CompileInfo */
    std::vector<const char *> inc_ptrs;
    for (const auto &d : opts.include_dirs)
        inc_ptrs.push_back(d.c_str());

    core::CompileInfo ci;
    ci.shader_type    = opts.hlsl ? core::ShaderLanguageType::HLSL : core::ShaderLanguageType::GLSL;
    ci.entrypoint     = opts.entrypoint.c_str();
    ci.includes_count = static_cast<uint32_t>(inc_ptrs.size());
    ci.includes       = inc_ptrs.empty() ? nullptr : inc_ptrs.data();
    ci.preamble       = opts.preamble.empty() ? nullptr : opts.preamble.c_str();
    ci.vulkan_version = opts.vulkan_ver;
    ci.spv_version    = opts.spv_ver;

    /* 3. Compile to SPIR-V */
    SPVData *spv = core::CompileFromPath(stage, input.c_str(), &ci);
    if (!spv || !spv->result) {
        fprintf(stderr, "[spvfc] Compile error in '%s':\n%s\n",
                input.c_str(), (spv && spv->log) ? spv->log : "(no detail)");
        core::FreeSPVData(spv);
        return 1;
    }

    /* 4. Reflect */
    core::SPVParseData *parsed = core::ParseSPV(spv);

    /* 5. Pack SPIR-V + reflection into flat binary */
    uint32_t  flat_size = 0;
    uint8_t  *flat      = core::CreateShaderFlat(stage, opts.entrypoint.c_str(),
                                                 spv, parsed, &flat_size);
    if (!flat) {
        fprintf(stderr, "[spvfc] CreateShaderFlat failed for '%s'\n", input.c_str());
        core::FreeSPVParse(parsed);
        core::FreeSPVData(spv);
        return 1;
    }

    /* 6. Print reflection */
    if (!opts.no_reflect) {
        ShaderFlatView view;
        if (ShaderFlatLoad(flat, flat_size, &view))
            print_reflection(input.c_str(), &view);
        else
            fprintf(stderr, "[spvfc] Warning: ShaderFlatLoad failed (internal error)\n");
    }

    /* 7. Write .spvf */
    int result = 0;
    if (!opts.no_output) {
        FILE *f = fopen(output.c_str(), "wb");
        if (!f) {
            fprintf(stderr, "[spvfc] Cannot open '%s' for writing\n", output.c_str());
            result = 1;
        } else {
            fwrite(flat, 1, flat_size, f);
            fclose(f);
            if (!opts.quiet)
                printf("[spvfc] Written   : %s (%u bytes)\n",
                       output.c_str(), flat_size);
        }
    }

    core::FreeShaderFlat(flat);
    core::FreeSPVParse(parsed);
    core::FreeSPVData(spv);
    return result;
}

/* =========================================================================
 * Usage
 * ========================================================================= */

static void usage(const char *prog)
{
    printf(
        "Usage: %s [options] <shader_file> [<shader_file2> ...]\n"
        "\n"
        "Compiles GLSL/HLSL shaders to .spvf (SPIR-V + reflection data)\n"
        "and prints a human-readable reflection summary.\n"
        "\n"
        "Options:\n"
        "  -o <file>       Output .spvf path (only valid with a single input)\n"
        "  -e <name>       Entry-point name  [default: main]\n"
        "  -I <dir>        Include root (1st = shader library, optional 2nd = generated/)\n"
        "  -D <macro>      Add preprocessor define  (-D<macro>=value also OK)\n"
        "  --hlsl          Treat all inputs as HLSL source\n"
        "  --vulkan <ver>  Target Vulkan version: 1.0  1.1  1.2  1.3  1.4 (default: 1.4)\n"
        "  --spv    <ver>  Target SPIR-V version: 1.0  1.1 .. 1.6 (default: 1.6)\n"
        "  --no-output     Compile and reflect but do not write .spvf files\n"
        "  --no-reflect    Suppress reflection text output\n"
        "  --quiet         Suppress informational messages (errors still shown)\n"
        "\n"
        "Output file: each input gets <input>.spvf unless -o is specified.\n"
        "\n"
        "Examples:\n"
        "  %s mesh.vert\n"
        "  %s -I shaders/include  scene.frag\n"
        "  %s --hlsl -e PSMain -o pbr.spvf  pbr.hlsl\n"
        "  %s -DENABLE_SHADOWS  --vulkan 1.2 --spv 1.5  lighting.frag\n"
        "  %s --no-output --no-reflect  *.comp\n",
        prog, prog, prog, prog, prog, prog);
}

/* =========================================================================
 * main
 * ========================================================================= */

int main(int argc, char *argv[])
{
    const char *prog = argc > 0 ? argv[0] : "spvfc";

    if (argc < 2) {
        usage(prog);
        return 0;
    }

    Options opts;
    int errors = 0;

    /* ---- argument parsing ---- */
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];

        if (a[0] != '-') {
            opts.inputs.push_back(a);
            continue;
        }

        /* -o <output> */
        if (strcmp(a, "-o") == 0 && i + 1 < argc)
            { opts.output = argv[++i]; continue; }

        /* -e <entrypoint> */
        if (strcmp(a, "-e") == 0 && i + 1 < argc)
            { opts.entrypoint = argv[++i]; continue; }

        /* -I <dir>  or  -I<dir> */
        if (strcmp(a, "-I") == 0 && i + 1 < argc)
            { opts.include_dirs.push_back(argv[++i]); continue; }
        if (strncmp(a, "-I", 2) == 0 && a[2] != '\0')
            { opts.include_dirs.push_back(a + 2); continue; }

        /* -D <macro>  or  -D<macro>[=val] */
        if (strcmp(a, "-D") == 0 && i + 1 < argc)
            { opts.preamble += std::string("#define ") + argv[++i] + "\n"; continue; }
        if (strncmp(a, "-D", 2) == 0 && a[2] != '\0')
            { opts.preamble += std::string("#define ") + (a + 2) + "\n"; continue; }

        /* flags */
        if (strcmp(a, "--hlsl")      == 0) { opts.hlsl       = true;  continue; }
        if (strcmp(a, "--no-output") == 0) { opts.no_output  = true;  continue; }
        if (strcmp(a, "--no-reflect")== 0) { opts.no_reflect = true;  continue; }
        if (strcmp(a, "--quiet")     == 0) { opts.quiet      = true;  continue; }

        /* --vulkan <ver> */
        if (strcmp(a, "--vulkan") == 0 && i + 1 < argc)
            { opts.vulkan_ver = parse_vulkan_ver(argv[++i]);
              if (opts.vulkan_ver == 0) return 1;
              continue; }

        /* --spv <ver> */
        if (strcmp(a, "--spv") == 0 && i + 1 < argc)
            { opts.spv_ver = parse_spv_ver(argv[++i]);
              if (opts.spv_ver == 0) return 1;
              continue; }

        fprintf(stderr, "[spvfc] Unknown option: %s\n\n", a);
        usage(prog);
        return 1;
    }

    if (opts.include_dirs.size() > 2) {
        fprintf(stderr, "[spvfc] At most two -I roots are allowed "
                        "(1st = shader library root, 2nd = generated/ root).\n");
        return 1;
    }

    if (opts.inputs.empty()) {
        fprintf(stderr, "[spvfc] No input files specified.\n\n");
        usage(prog);
        return 1;
    }

    if (!opts.output.empty() && opts.inputs.size() > 1) {
        fprintf(stderr, "[spvfc] -o may only be used with a single input file.\n");
        return 1;
    }

    /* ---- initialize compiler (glslang process-level init, no plugin load) ---- */
    if (!core::Init()) {
        fprintf(stderr, "[spvfc] Failed to initialize shader compiler.\n");
        return 1;
    }

    /* ---- process each file ---- */
    for (const auto &input : opts.inputs) {
        const std::string out =
            opts.output.empty() ? (input + ".spvf") : opts.output;
        errors += process_file(input, out, opts);
    }

    core::Shutdown();
    return (errors > 0) ? 1 : 0;
}
