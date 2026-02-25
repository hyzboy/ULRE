// Phase B 校验逻辑测试

#include <hgl/graph/mtl/ShaderLogic.h>
#include <stdio.h>

using namespace hgl::graph::mtl;

// ═════════════════════════════════════════════════════════════════════════════
// 测试用例 1：合法的最小材质（无任何依赖）
// ═════════════════════════════════════════════════════════════════════════════

constexpr char SIMPLE_VS_LOGIC[] = R"(
vec4 VertexShaderBusiness(const VertexInput vi) {
    return vec4(vi.Position, 1.0);
}
)";

constexpr char SIMPLE_FS_LOGIC[] = R"(
vec4 FragmentShaderBusiness() {
    return vec4(1.0, 0.0, 0.0, 1.0);
}
)";

const MaterialLogicDef VALID_MINIMAL_LOGIC = {
    .vertex = {
        .main_logic = SIMPLE_VS_LOGIC,
        .custom_functions = nullptr,
        .required_resources = nullptr,
        .required_resource_count = 0,
        .required_helpers = nullptr,
        .required_helper_count = 0
    },
    .fragment = {
        .main_logic = SIMPLE_FS_LOGIC,
        .custom_functions = nullptr,
        .required_resources = nullptr,
        .required_resource_count = 0,
        .required_helpers = nullptr,
        .required_helper_count = 0
    },
    .geometry = nullptr,
    .tess_control = nullptr,
    .tess_eval = nullptr
};

// ═════════════════════════════════════════════════════════════════════════════
// 测试用例 2：非法材质（VS main_logic 为空）
// ═════════════════════════════════════════════════════════════════════════════

const MaterialLogicDef INVALID_NO_VS_LOGIC = {
    .vertex = {
        .main_logic = nullptr,  // ❌ 违反约束
        .custom_functions = nullptr,
        .required_resources = nullptr,
        .required_resource_count = 0,
        .required_helpers = nullptr,
        .required_helper_count = 0
    },
    .fragment = {
        .main_logic = SIMPLE_FS_LOGIC,
        .custom_functions = nullptr,
        .required_resources = nullptr,
        .required_resource_count = 0,
        .required_helpers = nullptr,
        .required_helper_count = 0
    },
    .geometry = nullptr,
    .tess_control = nullptr,
    .tess_eval = nullptr
};

// ═════════════════════════════════════════════════════════════════════════════
// 测试用例 3：非法材质（count 与数组指针不匹配）
// ═════════════════════════════════════════════════════════════════════════════

const MaterialLogicDef INVALID_COUNT_MISMATCH = {
    .vertex = {
        .main_logic = SIMPLE_VS_LOGIC,
        .custom_functions = nullptr,
        .required_resources = nullptr,
        .required_resource_count = 2,  // ❌ 数组为 null 但 count > 0
        .required_helpers = nullptr,
        .required_helper_count = 0
    },
    .fragment = {
        .main_logic = SIMPLE_FS_LOGIC,
        .custom_functions = nullptr,
        .required_resources = nullptr,
        .required_resource_count = 0,
        .required_helpers = nullptr,
        .required_helper_count = 0
    },
    .geometry = nullptr,
    .tess_control = nullptr,
    .tess_eval = nullptr
};

// ═════════════════════════════════════════════════════════════════════════════
// 测试用例 4：警告情况（数组非空但 count 为 0）
// ═════════════════════════════════════════════════════════════════════════════

constexpr const char* DUMMY_RESOURCES[] = {"camera", "mtl"};

const MaterialLogicDef WARNING_ARRAY_IGNORED = {
    .vertex = {
        .main_logic = SIMPLE_VS_LOGIC,
        .custom_functions = nullptr,
        .required_resources = DUMMY_RESOURCES,
        .required_resource_count = 0,  // ⚠️ 数组非空但 count 为 0
        .required_helpers = nullptr,
        .required_helper_count = 0
    },
    .fragment = {
        .main_logic = SIMPLE_FS_LOGIC,
        .custom_functions = nullptr,
        .required_resources = nullptr,
        .required_resource_count = 0,
        .required_helpers = nullptr,
        .required_helper_count = 0
    },
    .geometry = nullptr,
    .tess_control = nullptr,
    .tess_eval = nullptr
};

// ═════════════════════════════════════════════════════════════════════════════
// 测试主函数
// ═════════════════════════════════════════════════════════════════════════════

int main() {
    printf("=== Phase B ShaderLogic Validation Test ===\n\n");

    // 测试 1：合法最小材质
    printf("[Test 1] Valid minimal logic:\n");
    bool result1 = ValidateMaterialLogicDef(VALID_MINIMAL_LOGIC);
    printf("Result: %s\n\n", result1 ? "✅ PASS" : "❌ FAIL");

    // 测试 2：非法材质（VS main_logic 为空）
    printf("[Test 2] Invalid - VS main_logic is null:\n");
    bool result2 = ValidateMaterialLogicDef(INVALID_NO_VS_LOGIC);
    printf("Result: %s (expected FAIL)\n\n", result2 ? "✅ PASS" : "❌ FAIL");

    // 测试 3：非法材质（count 与数组不匹配）
    printf("[Test 3] Invalid - count mismatch:\n");
    bool result3 = ValidateMaterialLogicDef(INVALID_COUNT_MISMATCH);
    printf("Result: %s (expected FAIL)\n\n", result3 ? "✅ PASS" : "❌ FAIL");

    // 测试 4：警告情况（数组非空但 count 为 0）
    printf("[Test 4] Warning - array ignored:\n");
    bool result4 = ValidateMaterialLogicDef(WARNING_ARRAY_IGNORED);
    printf("Result: %s (expected PASS with warnings)\n\n", result4 ? "✅ PASS" : "❌ FAIL");

    // 总结
    printf("=== Test Summary ===\n");
    printf("Test 1: %s\n", result1 ? "✅" : "❌");
    printf("Test 2: %s\n", !result2 ? "✅" : "❌");  // 应该 fail
    printf("Test 3: %s\n", !result3 ? "✅" : "❌");  // 应该 fail
    printf("Test 4: %s\n", result4 ? "✅" : "❌");

    return 0;
}
