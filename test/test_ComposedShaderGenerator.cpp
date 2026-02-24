/// test_ComposedShaderGenerator.cpp — 验证合成着色器生成器（真实生成路径）

#include <hgl/graph/mtl/ShaderComposition.h>
#include <hgl/graph/mtl/ShaderComposition_Examples.h>
#include <hgl/type/String.h>
#include <cstdio>
#include <cstring>

using namespace hgl::graph::mtl;

struct ShaderTextValidation {
    const char *description;
    const char *keyword;
    bool found = false;
};

static bool DumpShaderTextFile(const char *filename, const char *text)
{
    FILE *fp = fopen(filename, "wb");
    if (!fp)
    {
        printf("  [✗] 无法写入文件: %s\n", filename);
        return false;
    }

    const size_t len = strlen(text);
    const size_t written = fwrite(text, 1, len, fp);
    fclose(fp);

    if (written != len)
    {
        printf("  [✗] 文件写入不完整: %s (%u/%u)\n", filename, (unsigned)written, (unsigned)len);
        return false;
    }

    printf("  [✓] 已输出 GLSL: %s (%u bytes)\n", filename, (unsigned)len);
    return true;
}

static bool ValidateGLSL(const hgl::AnsiString &glsl_code, ShaderTextValidation *validations, uint32_t count)
{
    printf("\n[验证生成的 GLSL 代码]\n");
    printf("代码长度：%u 字符\n\n", glsl_code.Length());

    bool all_ok = true;
    for (uint32_t i = 0; i < count; i++)
    {
        validations[i].found = (std::strstr(glsl_code.c_str(), validations[i].keyword) != nullptr);
        const char *status = validations[i].found ? "✓" : "✗";
        printf("%s %s\n", status, validations[i].description);
        if (!validations[i].found)
            all_ok = false;
    }

    return all_ok;
}

int main()
{
    printf("========================================\n");
    printf("  ComposedShaderGenerator 验证测试\n");
    printf("========================================\n\n");

    printf("[Step 1] 生成真实 VS/FS GLSL（通过公开 API）\n");

    ShaderPermutationKey key{};

    hgl::AnsiString vs_code = ComposedShaderGenerator::ComposeVertexShader(BASIC_LIT_COMPOSED, key);
    hgl::AnsiString fs_code = ComposedShaderGenerator::ComposeFragmentShader(BASIC_LIT_COMPOSED, key);

    printf("  VS 长度: %u\n", vs_code.Length());
    printf("  FS 长度: %u\n", fs_code.Length());

    printf("\n[Step 2] 导出 GLSL 文件（失败时用于排查）\n");

    bool dump_vs_ok = DumpShaderTextFile("generated_vs.glsl", vs_code.c_str());
    bool dump_fs_ok = DumpShaderTextFile("generated_fs.glsl", fs_code.c_str());

    printf("\n[Step 3] 关键结构检查\n");

    ShaderTextValidation vs_validations[] = {
        {"包含 #version", "#version"},
        {"包含 VertexInput", "struct VertexInput"},
        {"包含 VS_Output", "struct VS_Output"},
        {"包含 GetLocalToWorld", "GetLocalToWorld"},
        {"包含 GetNormal", "GetNormal"},
        {"包含 main", "void main()"},
    };

    ShaderTextValidation fs_validations[] = {
        {"包含 #version", "#version"},
        {"包含 VS_Output", "struct VS_Output"},
        {"包含 ComposeFinalOutput", "ComposeFinalOutput"},
        {"包含 FragmentShaderBusiness", "FragmentShaderBusiness"},
        {"包含 main", "void main()"},
    };

    bool vs_ok = ValidateGLSL(vs_code, vs_validations, uint32_t(sizeof(vs_validations) / sizeof(vs_validations[0])));
    bool fs_ok = ValidateGLSL(fs_code, fs_validations, uint32_t(sizeof(fs_validations) / sizeof(fs_validations[0])));

    printf("\n========================================\n");
    printf("  测试总结\n");
    printf("========================================\n");
    printf("  VS 生成:   %s\n", vs_ok ? "✓ 通过" : "✗ 失败");
    printf("  FS 生成:   %s\n", fs_ok ? "✓ 通过" : "✗ 失败");
    printf("  GLSL 导出: %s\n", (dump_vs_ok && dump_fs_ok) ? "✓ 成功" : "✗ 失败");

    const bool all_ok = vs_ok && fs_ok && dump_vs_ok && dump_fs_ok;
    printf("  总体结果: %s\n\n", all_ok ? "✓✓✓ 全部通过" : "✗✗✗ 存在失败");

    if (!all_ok)
    {
        printf("请查看 generated_vs.glsl / generated_fs.glsl 进行定位。\n");
        return 1;
    }

    return 0;
}
