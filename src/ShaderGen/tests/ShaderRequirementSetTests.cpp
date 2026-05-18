// ShaderRequirementSetTests — Phase 1/2 单元测试
//
// 验证 ShaderRequirementSet 的核心行为：
//   1. ParseFromGLSLSource — 顶部注解解析
//   2. 去重（同一语义重复 require 只保留首次）
//   3. Requires() 查询
//   4. binding 分配顺序（桶内按插入顺序 0,1,2,...）
//   5. MergeFrom — 合并两个集合时去重正确
//   6. @sfm:no-require — 不产生任何 requirement
//   7. 注解在 #ifndef 之后 — 无法被解析（顶部扫描截止规则）

#include <hgl/shadergen/ShaderRequirementSet.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// 最小测试框架
// ---------------------------------------------------------------------------

static int g_pass = 0;
static int g_fail = 0;

static void check(bool cond, const char *expr, const char *file, int line)
{
    if (cond)
    {
        ++g_pass;
    }
    else
    {
        ++g_fail;
        std::fprintf(stderr, "FAIL  %s:%d  %s\n", file, line, expr);
    }
}

#define CHECK(cond) check((cond), #cond, __FILE__, __LINE__)

// ---------------------------------------------------------------------------
// 1. ParseFromGLSLSource — 解析顶部注解
// ---------------------------------------------------------------------------

static void Test_ParseTopAnnotations()
{
    const char *src =
        "// @sfm:require  UBO camera\n"
        "// @sfm:require  SSBO transform_id\n"
        "// @sfm:require  SSBO transform_data\n"
        "#ifndef GUARD\n"
        "#define GUARD\n"
        "void main(){}\n"
        "#endif\n";

    hgl::graph::ShaderRequirementSet rs;
    rs.ParseFromGLSLSource(src);

    CHECK(rs.Requires("camera"));
    CHECK(rs.Requires("transform_id"));
    CHECK(rs.Requires("transform_data"));
    CHECK(!rs.Requires("viewport"));
}

// ---------------------------------------------------------------------------
// 2. 去重
// ---------------------------------------------------------------------------

static void Test_Deduplication()
{
    const char *src =
        "// @sfm:require  UBO camera\n"
        "// @sfm:require  UBO camera\n"   // 重复
        "// @sfm:require  UBO camera\n"   // 重复
        "\n";

    hgl::graph::ShaderRequirementSet rs;
    rs.ParseFromGLSLSource(src);

    CHECK(rs.Requires("camera"));

    // camera 属于 PerFrame set；桶内应只有 1 条
    const auto &bucket = rs.GetRequirements(hgl::graph::DescriptorSetType::PerFrame);
    int camera_count = 0;
    for (const auto &req : bucket)
        if (req.sem_name && std::strcmp(req.sem_name, "camera") == 0) ++camera_count;
    CHECK(camera_count == 1);
}

// ---------------------------------------------------------------------------
// 3. @sfm:no-require — 不产生任何需求
// ---------------------------------------------------------------------------

static void Test_NoRequire()
{
    const char *src =
        "// @sfm:no-require\n"
        "#ifndef GUARD\n"
        "void main(){}\n"
        "#endif\n";

    hgl::graph::ShaderRequirementSet rs;
    rs.ParseFromGLSLSource(src);

    CHECK(rs.Empty());
    CHECK(!rs.Requires("camera"));
    CHECK(!rs.Requires("viewport"));
}

// ---------------------------------------------------------------------------
// 4. 注解在 #ifndef 之后 — 扫描截止规则
// ---------------------------------------------------------------------------

static void Test_AnnotationAfterGuard_NotParsed()
{
    // @sfm 行在 #ifndef 之后，ParseFromGLSLSource 应在遇到 '#' 开头行时停止
    const char *src =
        "#ifndef GUARD\n"
        "#define GUARD\n"
        "// @sfm:require  UBO camera\n"   // 在 guard 之后，不应被解析
        "void main(){}\n"
        "#endif\n";

    hgl::graph::ShaderRequirementSet rs;
    rs.ParseFromGLSLSource(src);

    CHECK(!rs.Requires("camera"));
    CHECK(rs.Empty());
}

// ---------------------------------------------------------------------------
// 5. binding 分配顺序（同一 set 桶内按插入顺序 0,1,2,...）
// ---------------------------------------------------------------------------

static void Test_BindingOrder()
{
    // transform_id 和 transform_data 都在 PerObject set
    const char *src =
        "// @sfm:require  SSBO transform_id\n"
        "// @sfm:require  SSBO transform_data\n"
        "\n";

    hgl::graph::ShaderRequirementSet rs;
    rs.ParseFromGLSLSource(src);

    CHECK(rs.Requires("transform_id"));
    CHECK(rs.Requires("transform_data"));

    const auto bindings = rs.GetVkBindings(hgl::graph::DescriptorSetType::PerObject);
    CHECK(bindings.size() >= 2);
    if (bindings.size() >= 2)
    {
        CHECK(bindings[0].binding == 0);   // transform_id 先声明 → binding 0
        CHECK(bindings[1].binding == 1);   // transform_data 后声明 → binding 1
    }
}

// ---------------------------------------------------------------------------
// 6. MergeFrom — 合并两个集合，跨源去重
// ---------------------------------------------------------------------------

static void Test_MergeFrom()
{
    const char *src_vs =
        "// @sfm:require  UBO camera\n"
        "// @sfm:require  SSBO transform_id\n"
        "\n";

    const char *src_fs =
        "// @sfm:require  UBO camera\n"   // 已在 VS 中 → 合并后仍只有一条
        "// @sfm:require  UBO viewport\n"
        "\n";

    hgl::graph::ShaderRequirementSet rs_vs, rs_fs;
    rs_vs.ParseFromGLSLSource(src_vs);
    rs_fs.ParseFromGLSLSource(src_fs);

    rs_vs.MergeFrom(rs_fs);

    CHECK(rs_vs.Requires("camera"));
    CHECK(rs_vs.Requires("transform_id"));
    CHECK(rs_vs.Requires("viewport"));

    // camera 去重：PerFrame 桶内仍只有 1 条
    const auto &pf = rs_vs.GetRequirements(hgl::graph::DescriptorSetType::PerFrame);
    int camera_count = 0;
    for (const auto &req : pf)
        if (req.sem_name && std::strcmp(req.sem_name, "camera") == 0) ++camera_count;
    CHECK(camera_count == 1);
}

// ---------------------------------------------------------------------------
// 7. 混合注释与空行不影响解析
// ---------------------------------------------------------------------------

static void Test_InterleavedBlankLines()
{
    const char *src =
        "\n"
        "// some comment\n"
        "// @sfm:require  UBO viewport\n"
        "\n"                              // 空行在注释块内，继续扫描
        "// @sfm:require  UBO camera\n"
        "\n";

    hgl::graph::ShaderRequirementSet rs;
    rs.ParseFromGLSLSource(src);

    CHECK(rs.Requires("viewport"));
    CHECK(rs.Requires("camera"));
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main()
{
    std::printf("=== ShaderRequirementSetTests ===\n");

    Test_ParseTopAnnotations();
    Test_Deduplication();
    Test_NoRequire();
    Test_AnnotationAfterGuard_NotParsed();
    Test_BindingOrder();
    Test_MergeFrom();
    Test_InterleavedBlankLines();

    std::printf("\nResults: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
