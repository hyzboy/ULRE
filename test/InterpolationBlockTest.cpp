// Phase C 插值接口块测试

#include <string>
#include <vector>
#include <utility>
#include <stdio.h>

/// 提取业务代码中的插值变量（简化版：基于字符串匹配）
static std::vector<std::pair<std::string, std::string>> ExtractInterpolatedVariables(const char *code)
{
    std::vector<std::pair<std::string, std::string>> variables; // {name, type}
    
    if (!code || !*code)
        return variables;
    
    std::string code_str(code);
    
    // 检测常见插值变量模式
    struct InterpolationPattern {
        const char *pattern;
        const char *name;
        const char *type;
    };
    
    InterpolationPattern patterns[] = {
        {"Output.Color", "Color", "vec4"},
        {"Output.Normal", "Normal", "vec3"},
        {"Output.Position", "Position", "vec4"},
        {"Output.WorldPosition", "WorldPosition", "vec4"},
        {"Output.TexCoord", "TexCoord", "vec2"},
        {"Output.Tangent", "Tangent", "vec3"},
        {"Output.Bitangent", "Bitangent", "vec3"},
    };
    
    for (const auto &p : patterns)
    {
        if (code_str.find(p.pattern) != std::string::npos)
        {
            // 避免重复添加
            bool exists = false;
            for (const auto &v : variables)
            {
                if (v.first == p.name)
                {
                    exists = true;
                    break;
                }
            }
            if (!exists)
                variables.push_back({p.name, p.type});
        }
    }
    
    return variables;
}

int main()
{
    printf("=== Phase C Interpolation Block Test ===\n\n");

    // 测试 1：VertexColor3D （单个插值变量）
    const char *vertex_color_vs = R"(
vec4 VertexShaderBusiness(const VertexInput vi)
{
    Output.Color = vi.Color;
    return vec4(vi.Position, 1.0);
}
)";

    printf("[Test 1] VertexColor3D VS:\n");
    auto vc_vars = ExtractInterpolatedVariables(vertex_color_vs);
    printf("Found %zu interpolated variables:\n", vc_vars.size());
    for (const auto &v : vc_vars)
    {
        printf("  - %s %s\n", v.second.c_str(), v.first.c_str());
    }
    printf("Expected: 1 variable (Color vec4)\n\n");

    // 测试 2：Gizmo3D （多个插值变量）
    const char *gizmo_vs = R"(
vec4 VertexShaderBusiness(const VertexInput vi)
{
    Output.Normal = vi.Normal;
    Output.Position = vec4(vi.Position, 1.0);
    return Output.Position;
}
)";

    printf("[Test 2] Gizmo3D VS:\n");
    auto gizmo_vars = ExtractInterpolatedVariables(gizmo_vs);
    printf("Found %zu interpolated variables:\n", gizmo_vars.size());
    for (const auto &v : gizmo_vars)
    {
        printf("  - %s %s\n", v.second.c_str(), v.first.c_str());
    }
    printf("Expected: 2 variables (Normal vec3, Position vec4)\n\n");

    // 测试 3：TextureBlinnPhong （纹理坐标）
    const char *texture_vs = R"(
vec4 VertexShaderBusiness(const VertexInput vi)
{
    Output.TexCoord = vi.TexCoord;
    Output.Normal = vi.Normal;
    Output.WorldPosition = vec4(vi.Position, 1.0);
    return Output.WorldPosition;
}
)";

    printf("[Test 3] TextureBlinnPhong VS:\n");
    auto tex_vars = ExtractInterpolatedVariables(texture_vs);
    printf("Found %zu interpolated variables:\n", tex_vars.size());
    for (const auto &v : tex_vars)
    {
        printf("  - %s %s\n", v.second.c_str(), v.first.c_str());
    }
    printf("Expected: 3 variables (TexCoord vec2, Normal vec3, WorldPosition vec4)\n\n");

    // 总结
    printf("=== Test Summary ===\n");
    printf("Test 1: %s\n", vc_vars.size() == 1 ? "✅ PASS" : "❌ FAIL");
    printf("Test 2: %s\n", gizmo_vars.size() == 2 ? "✅ PASS" : "❌ FAIL");
    printf("Test 3: %s\n", tex_vars.size() == 3 ? "✅ PASS" : "❌ FAIL");

    return 0;
}
