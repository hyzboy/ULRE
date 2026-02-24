/// test_ComposedShaderGenerator.cpp — 验证合成着色器生成器（真实生成路径）

#include <hgl/graph/mtl/ShaderComposition.h>
#include <hgl/graph/mtl/ShaderComposition_Examples.h>
#include <hgl/type/String.h>
#include <cstdio>
#include <cstring>
#include <unordered_set>

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

static bool ValidateNoDuplicateSetBinding(const hgl::AnsiString &glsl_code, const char *label)
{
    std::unordered_set<uint64_t> binding_set;
    bool all_ok = true;

    const char *cursor = glsl_code.c_str();
    const char *token = "layout(set=";

    printf("\n[重复绑定检查] %s\n", label);

    while ((cursor = std::strstr(cursor, token)) != nullptr)
    {
        unsigned int set_id = 0;
        unsigned int binding_id = 0;

        if (std::sscanf(cursor, "layout(set=%u, binding=%u", &set_id, &binding_id) == 2)
        {
            const uint64_t key = (uint64_t(set_id) << 32) | uint64_t(binding_id);

            if (binding_set.find(key) != binding_set.end())
            {
                printf("  [✗] 发现重复 binding: set=%u, binding=%u\n", set_id, binding_id);
                all_ok = false;
            }
            else
            {
                binding_set.insert(key);
            }
        }

        cursor += 1;
    }

    if (all_ok)
    {
        printf("  [✓] 未发现重复 set/binding\n");
    }

    return all_ok;
}

static bool ValidateStage3Helpers(const hgl::AnsiString &glsl_code, const char *label)
{
    printf("\n[Helper 注入检查] %s\n", label);

    ShaderTextValidation helper_checks[] = {
        {"包含 TransformNormal", "TransformNormal("},
        {"包含 GetCameraPos", "GetCameraPos("},
        {"包含 GetWorldPos", "GetWorldPos("},
    };

    return ValidateGLSL(glsl_code, helper_checks, uint32_t(sizeof(helper_checks) / sizeof(helper_checks[0])));
}

static bool ValidateHelperAliasEmission(const hgl::AnsiString &glsl_code, const char *label)
{
    printf("\n[Helper 别名检查] %s\n", label);

    ShaderTextValidation helper_alias_checks[] = {
        {"包含 GetWorldPosition", "GetWorldPosition("},
        {"包含 GetCameraPosition", "GetCameraPosition("},
    };

    return ValidateGLSL(glsl_code, helper_alias_checks, uint32_t(sizeof(helper_alias_checks) / sizeof(helper_alias_checks[0])));
}

static bool ValidateContainsWorldAndCameraHelpersOnly(const hgl::AnsiString &glsl_code, const char *label)
{
    printf("\n[显式依赖注入检查] %s\n", label);

    const bool has_world = (std::strstr(glsl_code.c_str(), "GetWorldPos(") != nullptr);
    const bool has_camera = (std::strstr(glsl_code.c_str(), "GetCameraPos(") != nullptr);
    const bool has_transform = (std::strstr(glsl_code.c_str(), "TransformNormal(") != nullptr);

    printf("%s 包含 GetWorldPos\n", has_world ? "✓" : "✗");
    printf("%s 包含 GetCameraPos\n", has_camera ? "✓" : "✗");
    printf("%s 不包含 TransformNormal\n", !has_transform ? "✓" : "✗");

    return has_world && has_camera && !has_transform;
}

static bool ValidateHelperAbsence(const hgl::AnsiString &glsl_code, const char *label)
{
    printf("\n[Helper 未注入检查] %s\n", label);

    ShaderTextValidation helper_checks[] = {
        {"不包含 TransformNormal", "TransformNormal("},
        {"不包含 GetCameraPos", "GetCameraPos("},
        {"不包含 GetWorldPos", "GetWorldPos("},
    };

    bool all_ok = true;
    for (uint32_t i = 0; i < uint32_t(sizeof(helper_checks) / sizeof(helper_checks[0])); ++i)
    {
        const bool found = (std::strstr(glsl_code.c_str(), helper_checks[i].keyword) != nullptr);
        const char *status = found ? "✗" : "✓";
        printf("%s %s\n", status, helper_checks[i].description);
        if (found)
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

    hgl::AnsiString vs_code = ComposedShaderGenerator::ComposeVertexShader(EX_BASIC_LIT_COMPOSED, key);
    hgl::AnsiString fs_code = ComposedShaderGenerator::ComposeFragmentShader(EX_BASIC_LIT_COMPOSED, key);

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

    printf("\n[Step 4] Stage 2/3 回归检查\n");

    bool vs_no_dup = ValidateNoDuplicateSetBinding(vs_code, "VS");
    bool fs_no_dup = ValidateNoDuplicateSetBinding(fs_code, "FS");

    // Stage 5：按需注入后，BasicLit（当前业务代码未直接调用 helper）应不注入这些函数
    bool vs_helper_absent_ok = ValidateHelperAbsence(vs_code, "VS(BasicLit)");
    bool fs_helper_absent_ok = ValidateHelperAbsence(fs_code, "FS(BasicLit)");

    // 构造一个显式调用 helper 的材质，验证按需注入生效
    static const char HELPER_DEMAND_VS_BUSINESS[] = R"(
        vec4 VertexShaderBusiness(const VertexInput vi) {
            return vec4(vi.Position, 1.0);
        }
    )";

    static const char HELPER_DEMAND_FS_BUSINESS[] = R"(
        vec4 FragmentShaderBusiness(const VS_Output vso) {
            vec3 world_n = TransformNormal(vso.WorldNormal);
            vec3 view_dir = normalize(GetCameraPosition() - GetWorldPosition());
            float ndv = max(dot(world_n, view_dir), 0.0);
            return vec4(vec3(ndv), 1.0);
        }
    )";

    const VertexShaderBusiness HELPER_DEMAND_VERTEX_BUSINESS { HELPER_DEMAND_VS_BUSINESS };
    const FragmentShaderBusiness HELPER_DEMAND_FRAGMENT_BUSINESS { HELPER_DEMAND_FS_BUSINESS };

    const ComposedMaterialDef HELPER_DEMAND_COMPOSED {
        .name = "HelperDemand",
        .primitive_type = EX_BASIC_LIT_COMPOSED.primitive_type,
        .vertex_entries = EX_BASIC_LIT_VERTEX,
        .vertex_entry_count = 3,
        .descriptor_entries = EX_BASIC_LIT_DESCRIPTORS,
        .descriptor_entry_count = 7,
        .vertex_business = &HELPER_DEMAND_VERTEX_BUSINESS,
        .fragment_business = &HELPER_DEMAND_FRAGMENT_BUSINESS,
        .output_mode = ShaderOutputMode::SingleRTAlphaBlend,
        .enable_lighting = false,
        .mi_glsl_codes = EX_BASIC_LIT_MI_GLSL,
        .mi_struct_bytes = sizeof(float) * 3,
    };

    hgl::AnsiString helper_vs_code = ComposedShaderGenerator::ComposeVertexShader(HELPER_DEMAND_COMPOSED, key);
    hgl::AnsiString helper_fs_code = ComposedShaderGenerator::ComposeFragmentShader(HELPER_DEMAND_COMPOSED, key);

    bool vs_helper_ok = ValidateHelperAbsence(helper_vs_code, "VS(HelperDemand)");
    bool fs_helper_ok = ValidateStage3Helpers(helper_fs_code, "FS(HelperDemand)");
    bool fs_helper_alias_ok = ValidateHelperAliasEmission(helper_fs_code, "FS(HelperDemand)");

    // 显式依赖注入测试：业务代码不调用 helper，但通过 ComposedMaterialDef 显式声明依赖
    static const char EXPLICIT_HELPER_FS_BUSINESS[] = R"(
        vec4 FragmentShaderBusiness(const VS_Output vso) {
            // 这里不直接调用 helper，验证显式依赖是否生效
            return vec4(vso.WorldNormal * 0.5 + vec3(0.5), 1.0);
        }
    )";

    const FragmentShaderBusiness EXPLICIT_HELPER_FRAGMENT_BUSINESS { EXPLICIT_HELPER_FS_BUSINESS };
    const char *EXPLICIT_FS_HELPERS[] = {
        "GetWorldPos",
        "GetCameraPosition", // 别名也应生效
    };

    const ComposedMaterialDef EXPLICIT_HELPER_COMPOSED {
        .name = "ExplicitHelperDemand",
        .primitive_type = EX_BASIC_LIT_COMPOSED.primitive_type,
        .vertex_entries = EX_BASIC_LIT_VERTEX,
        .vertex_entry_count = 3,
        .descriptor_entries = EX_BASIC_LIT_DESCRIPTORS,
        .descriptor_entry_count = 7,
        .vertex_business = &HELPER_DEMAND_VERTEX_BUSINESS,
        .fragment_business = &EXPLICIT_HELPER_FRAGMENT_BUSINESS,
        .output_mode = ShaderOutputMode::SingleRTAlphaBlend,
        .enable_lighting = false,
        .mi_glsl_codes = EX_BASIC_LIT_MI_GLSL,
        .mi_struct_bytes = sizeof(float) * 3,
        .vertex_required_helpers = nullptr,
        .vertex_required_helper_count = 0,
        .fragment_required_helpers = EXPLICIT_FS_HELPERS,
        .fragment_required_helper_count = 2,
    };

    hgl::AnsiString explicit_fs_code = ComposedShaderGenerator::ComposeFragmentShader(EXPLICIT_HELPER_COMPOSED, key);
    bool fs_explicit_helper_ok = ValidateContainsWorldAndCameraHelpersOnly(explicit_fs_code, "FS(ExplicitHelperDemand)");

    printf("\n========================================\n");
    printf("  测试总结\n");
    printf("========================================\n");
    printf("  VS 生成:   %s\n", vs_ok ? "✓ 通过" : "✗ 失败");
    printf("  FS 生成:   %s\n", fs_ok ? "✓ 通过" : "✗ 失败");
    printf("  VS 无重复绑定: %s\n", vs_no_dup ? "✓ 通过" : "✗ 失败");
    printf("  FS 无重复绑定: %s\n", fs_no_dup ? "✓ 通过" : "✗ 失败");
    printf("  VS Helper 未注入(BasicLit): %s\n", vs_helper_absent_ok ? "✓ 通过" : "✗ 失败");
    printf("  FS Helper 未注入(BasicLit): %s\n", fs_helper_absent_ok ? "✓ 通过" : "✗ 失败");
    printf("  VS Helper 未注入(HelperDemand): %s\n", vs_helper_ok ? "✓ 通过" : "✗ 失败");
    printf("  FS Helper 注入(HelperDemand): %s\n", fs_helper_ok ? "✓ 通过" : "✗ 失败");
    printf("  FS Helper 别名输出(HelperDemand): %s\n", fs_helper_alias_ok ? "✓ 通过" : "✗ 失败");
    printf("  FS 显式依赖注入(ExplicitHelperDemand): %s\n", fs_explicit_helper_ok ? "✓ 通过" : "✗ 失败");
    printf("  GLSL 导出: %s\n", (dump_vs_ok && dump_fs_ok) ? "✓ 成功" : "✗ 失败");

    const bool all_ok = vs_ok && fs_ok && vs_no_dup && fs_no_dup
                     && vs_helper_absent_ok && fs_helper_absent_ok
                     && vs_helper_ok && fs_helper_ok
                     && fs_helper_alias_ok
                     && fs_explicit_helper_ok
                     && dump_vs_ok && dump_fs_ok;
    printf("  总体结果: %s\n\n", all_ok ? "✓✓✓ 全部通过" : "✗✗✗ 存在失败");

    if (!all_ok)
    {
        printf("请查看 generated_vs.glsl / generated_fs.glsl 进行定位。\n");
        return 1;
    }

    return 0;
}
