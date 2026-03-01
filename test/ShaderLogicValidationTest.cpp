// Phase B 校验逻辑测试

#include <hgl/shadergen/ShaderLogic.h>
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
    VertexShaderLogic{{
        SIMPLE_VS_LOGIC,
        nullptr,
        nullptr,
        0,
        nullptr,
        0
    }},
    FragmentShaderLogic{{
        SIMPLE_FS_LOGIC,
        nullptr,
        nullptr,
        0,
        nullptr,
        0
    }},
    nullptr,
    nullptr,
    nullptr
};

// ═════════════════════════════════════════════════════════════════════════════
// 测试用例 2：非法材质（VS main_logic 为空）
// ═════════════════════════════════════════════════════════════════════════════

const MaterialLogicDef INVALID_NO_VS_LOGIC = {
    VertexShaderLogic{{
        nullptr,
        nullptr,
        nullptr,
        0,
        nullptr,
        0
    }},
    FragmentShaderLogic{{
        SIMPLE_FS_LOGIC,
        nullptr,
        nullptr,
        0,
        nullptr,
        0
    }},
    nullptr,
    nullptr,
    nullptr
};

// ═════════════════════════════════════════════════════════════════════════════
// 测试用例 3：非法材质（count 与数组指针不匹配）
// ═════════════════════════════════════════════════════════════════════════════

const MaterialLogicDef INVALID_COUNT_MISMATCH = {
    VertexShaderLogic{{
        SIMPLE_VS_LOGIC,
        nullptr,
        nullptr,
        2,
        nullptr,
        0
    }},
    FragmentShaderLogic{{
        SIMPLE_FS_LOGIC,
        nullptr,
        nullptr,
        0,
        nullptr,
        0
    }},
    nullptr,
    nullptr,
    nullptr
};

// ═════════════════════════════════════════════════════════════════════════════
// 测试用例 4：警告情况（数组非空但 count 为 0）
// ═════════════════════════════════════════════════════════════════════════════

constexpr const char* DUMMY_RESOURCES[] = {"camera", "mtl"};

const MaterialLogicDef WARNING_ARRAY_IGNORED = {
    VertexShaderLogic{{
        SIMPLE_VS_LOGIC,
        nullptr,
        DUMMY_RESOURCES,
        0,
        nullptr,
        0
    }},
    FragmentShaderLogic{{
        SIMPLE_FS_LOGIC,
        nullptr,
        nullptr,
        0,
        nullptr,
        0
    }},
    nullptr,
    nullptr,
    nullptr
};

// ═════════════════════════════════════════════════════════════════════════════
// 测试主函数
// ═════════════════════════════════════════════════════════════════════════════

int main() {
    printf("=== Phase B ShaderLogic Validation Test ===\n\n");

    // 测试 1：合法最小材质
    printf("[Test 1] Valid minimal logic:\n");
    bool result1 = ValidateMaterialLogicDef(VALID_MINIMAL_LOGIC);
    bool pass1 = (result1 == true);
    printf("Result: %s\n\n", pass1 ? "✅ PASS" : "❌ FAIL");

    // 测试 2：非法材质（VS main_logic 为空）
    printf("[Test 2] Invalid - VS main_logic is null:\n");
    bool result2 = ValidateMaterialLogicDef(INVALID_NO_VS_LOGIC);
    bool pass2 = (result2 == false);
    printf("Result: %s (expected reject)\n\n", pass2 ? "✅ PASS" : "❌ FAIL");

    // 测试 3：非法材质（count 与数组不匹配）
    printf("[Test 3] Invalid - count mismatch:\n");
    bool result3 = ValidateMaterialLogicDef(INVALID_COUNT_MISMATCH);
    bool pass3 = (result3 == false);
    printf("Result: %s (expected reject)\n\n", pass3 ? "✅ PASS" : "❌ FAIL");

    // 测试 4：警告情况（数组非空但 count 为 0）
    printf("[Test 4] Warning - array ignored:\n");
    bool result4 = ValidateMaterialLogicDef(WARNING_ARRAY_IGNORED);
    bool pass4 = (result4 == true);
    printf("Result: %s (expected PASS with warnings)\n\n", pass4 ? "✅ PASS" : "❌ FAIL");

    // 总结
    printf("=== Test Summary ===\n");
    printf("Test 1: %s\n", pass1 ? "✅" : "❌");
    printf("Test 2: %s\n", pass2 ? "✅" : "❌");
    printf("Test 3: %s\n", pass3 ? "✅" : "❌");
    printf("Test 4: %s\n", pass4 ? "✅" : "❌");

    return (pass1 && pass2 && pass3 && pass4) ? 0 : 1;
}
