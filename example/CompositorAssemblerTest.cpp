// CompositorAssembler + GLSL 编译测试
// 测试 Surface Function + Compositor Template 的 GLSL 生成与 SPV 编译全链路

#include <hgl/shadergen/CompositorAssembler.h>
#include <hgl/shadergen/GLSLCompilerConfig.h>
#include <iostream>
#include <cstdio>

using namespace hgl::graph;

// GLSLCompiler 接口 (from GLSLCompiler.h)
namespace hgl::graph
{
    bool     InitShaderCompiler();
    void     CloseShaderCompiler();
    struct SPVData { bool result; char *log; char *debug_log; uint32_t *spv_data; uint32_t spv_length; };
    SPVData *CompileShader(const uint32_t type, const char *source);
    void     FreeSPVData(SPVData *spv_data);
}

#ifndef VK_SHADER_STAGE_VERTEX_BIT
#define VK_SHADER_STAGE_VERTEX_BIT   0x00000001
#endif
#ifndef VK_SHADER_STAGE_FRAGMENT_BIT
#define VK_SHADER_STAGE_FRAGMENT_BIT 0x00000010
#endif

static void PrintSeparator(const char *title)
{
    std::printf("\n==== %s ====\n\n", title);
}

static bool TestAssembleGLSL(CompositorAssembler &assembler)
{
    PrintSeparator("Step 1: CompositorAssembler::Assemble()");

    auto result = assembler.Assemble(
        SurfaceType::Standard,
        RenderAlphaMode::Opaque,
        PassType::ForwardOpaque
    );

    if (!result.success)
    {
        std::fprintf(stderr, "FAIL: Assemble failed: %s\n", result.error_message.c_str());
        return false;
    }

    std::printf("OK: Assemble succeeded.\n");

    // 打印生成的 VS
    PrintSeparator("Generated Vertex Shader");
    std::printf("%s\n", result.vertex_glsl.c_str());

    // 打印生成的 FS
    PrintSeparator("Generated Fragment Shader");
    std::printf("%s\n", result.fragment_glsl.c_str());

    // 检查残留的 #include（信息性打印，glslang 会在编译阶段解析它们）
    if (result.vertex_glsl.find("#include") != std::string::npos)
        std::printf("INFO: VS contains #include directives (will be resolved by glslang).\n");
    if (result.fragment_glsl.find("#include") != std::string::npos)
        std::printf("INFO: FS contains #include directives (will be resolved by glslang).\n");
    return true;
}

static bool TestCompileSPV(CompositorAssembler &assembler, const std::string &shader_lib)
{
    PrintSeparator("Step 2: GLSL -> SPV Compile");

    auto result = assembler.Assemble(
        SurfaceType::Standard,
        RenderAlphaMode::Opaque,
        PassType::ForwardOpaque
    );

    if (!result.success)
    {
        std::fprintf(stderr, "FAIL: Assemble failed: %s\n", result.error_message.c_str());
        return false;
    }

    // 初始化 GLSL 编译器
    if (!InitShaderCompiler())
    {
        std::fprintf(stderr, "SKIP: GLSLCompiler plugin not available — skipping SPV compile test.\n");
        std::fprintf(stderr, "      (This is expected if GLSLCompiler DLL is not in the working directory)\n");
        return true;  // 非致命
    }

    // 设置 #include 搜索路径（以 ShaderLibrary 为根，glslang 会在此目录下查找）
    // 注意：需要 DLL 已支持 GL_GOOGLE_include_directive
    AddShaderIncludePath(shader_lib.c_str());

    // 先编译一个最简单的 shader 确认 DLL 基本工作正常
    {
        const char *trivial_vs =
            "#version 450\nvoid main() { gl_Position = vec4(0); }\n";
        std::printf("Sanity check: compiling trivial VS...\n");
        SPVData *t = CompileShader(VK_SHADER_STAGE_VERTEX_BIT, trivial_vs);
        if (!t)
            std::printf("  Sanity: returned null!\n");
        else
        {
            std::printf("  Sanity: result=%d, spv_length=%u\n", (int)t->result, t->spv_length);
            if (t->log && t->log[0]) std::printf("  Sanity log: %s\n", t->log);
            FreeSPVData(t);
        }
    }

    // 测试 #include 是否生效
    {
        const char *include_test =
            "#version 450\n"
            "#include \"common/surface_interface.glsl\"\n"
            "void main() { SurfaceInput si; gl_Position = vec4(si.worldPos, 1.0); }\n";
        std::printf("Include test: compiling VS with #include...\n");
        SPVData *t = CompileShader(VK_SHADER_STAGE_VERTEX_BIT, include_test);
        if (!t)
            std::printf("  Include test: returned null! (possible crash in DLL)\n");
        else
        {
            std::printf("  Include test: result=%d, spv_length=%u\n", (int)t->result, t->spv_length);
            if (t->log && t->log[0]) std::printf("  Include log: %s\n", t->log);
            FreeSPVData(t);
        }
    }

    // 编译 VS
    std::printf("Compiling VS...\n");
    SPVData *vs_spv = CompileShader(VK_SHADER_STAGE_VERTEX_BIT, result.vertex_glsl.c_str());
    if (!vs_spv || !vs_spv->result)
    {
        std::printf("FAIL: VS compile failed (spv_data=%p).\n", (void*)vs_spv);
        if (vs_spv)
        {
            std::printf("  result=%d, log=%p, debug_log=%p\n", (int)vs_spv->result,
                        (void*)vs_spv->log, (void*)vs_spv->debug_log);
            if (vs_spv->log && vs_spv->log[0])
                std::printf("  Log: %s\n", vs_spv->log);
            if (vs_spv->debug_log && vs_spv->debug_log[0])
                std::printf("  Debug: %s\n", vs_spv->debug_log);
        }
        if (vs_spv) FreeSPVData(vs_spv);
        CloseShaderCompiler();
        return false;
    }
    std::printf("OK: VS compiled to %u SPIR-V words.\n", vs_spv->spv_length);
    FreeSPVData(vs_spv);

    // 编译 FS
    std::printf("Compiling FS...\n");
    SPVData *fs_spv = CompileShader(VK_SHADER_STAGE_FRAGMENT_BIT, result.fragment_glsl.c_str());
    if (!fs_spv || !fs_spv->result)
    {
        std::printf("FAIL: FS compile failed.\n");
        if (fs_spv && fs_spv->log)
            std::printf("  Log: %s\n", fs_spv->log);
        if (fs_spv && fs_spv->debug_log)
            std::printf("  Debug: %s\n", fs_spv->debug_log);
        if (fs_spv) FreeSPVData(fs_spv);
        CloseShaderCompiler();
        return false;
    }
    std::printf("OK: FS compiled to %u SPIR-V words.\n", fs_spv->spv_length);
    FreeSPVData(fs_spv);

    CloseShaderCompiler();
    return true;
}

int main(int argc, char *argv[])
{
    // ShaderLibrary 路径：默认从工作目录出发
    std::string shader_lib = "ShaderLibrary";
    if (argc > 1)
        shader_lib = argv[1];

    std::printf("ShaderLibrary path: %s\n", shader_lib.c_str());

    CompositorAssembler assembler(shader_lib);

    int failures = 0;

    if (!TestAssembleGLSL(assembler))
        ++failures;

    if (!TestCompileSPV(assembler, shader_lib))
        ++failures;

    PrintSeparator("Summary");
    if (failures == 0)
        std::printf("ALL TESTS PASSED.\n");
    else
        std::printf("%d test(s) FAILED.\n", failures);

    return failures;
}
