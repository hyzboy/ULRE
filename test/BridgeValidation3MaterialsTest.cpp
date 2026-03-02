#include <hgl/shadergen/ShaderComposition.h>
#include <hgl/shadergen/ShaderLogic.h>
#include <hgl/type/String.h>

#include "ShaderGen/3d/S_PureColor3D.h"
#include "ShaderGen/3d/S_PureColor3D_Logic.h"
#include "ShaderGen/3d/S_VertexColor3D.h"
#include "ShaderGen/3d/S_VertexColor3D_Logic.h"
#include "ShaderGen/3d/S_Gizmo3D.h"

#include <cstdio>
#include <cstring>

using namespace hgl::graph::mtl;

struct BridgeCheckResult {
    const char *name;
    bool logic_valid = false;
    bool bridge_ok = false;
    bool no_missing_resource = false;
    bool vs_generated = false;
    bool fs_generated = false;
    bool semantic_ok = false;
};

static bool ContainsKeyword(const char *text, const char *keyword) {
    return text && keyword && (std::strstr(text, keyword) != nullptr);
}

static BridgeCheckResult RunBridgeCheck(
    const char *name,
    const ComposedMaterialDef &base_def,
    const MaterialLogicDef &logic,
    const char* const* vs_semantic_keywords,
    const uint32_t vs_semantic_keyword_count,
    const char* const* fs_semantic_keywords,
    const uint32_t fs_semantic_keyword_count,
    const ShaderPermutationKey &key)
{
    BridgeCheckResult result;
    result.name = name;

    result.logic_valid = ValidateMaterialLogicDef(logic);

    ComposedMaterialBuildFromLogicResult bridge_result;
    result.bridge_ok = BuildComposedMaterialDefFromLogic(base_def, logic, bridge_result);
    result.no_missing_resource = bridge_result.diagnostics.missing_resources.empty();

    if (result.bridge_ok) {
        const std::string vs_code = ComposedShaderGenerator::ComposeVertexShader(bridge_result.def, key);
        const std::string fs_code = ComposedShaderGenerator::ComposeFragmentShader(bridge_result.def, key);

        result.vs_generated = (!vs_code.empty()) && ContainsKeyword(vs_code.c_str(), "VertexShaderBusiness");
        result.fs_generated = (!fs_code.empty()) && ContainsKeyword(fs_code.c_str(), "FragmentShaderBusiness");

        bool vs_semantic_ok = true;
        for (uint32_t i = 0; i < vs_semantic_keyword_count; ++i) {
            if (!ContainsKeyword(vs_code.c_str(), vs_semantic_keywords[i])) {
                printf("  [Semantic][%s][VS] Missing keyword: %s\n", name, vs_semantic_keywords[i]);
                vs_semantic_ok = false;
            }
        }

        bool fs_semantic_ok = true;
        for (uint32_t i = 0; i < fs_semantic_keyword_count; ++i) {
            if (!ContainsKeyword(fs_code.c_str(), fs_semantic_keywords[i])) {
                printf("  [Semantic][%s][FS] Missing keyword: %s\n", name, fs_semantic_keywords[i]);
                fs_semantic_ok = false;
            }
        }

        result.semantic_ok = vs_semantic_ok && fs_semantic_ok;
    }

    return result;
}

int main() {
    printf("=== Phase C Bridge Validation (PureColor / VertexColor / Gizmo) ===\n\n");

    ShaderPermutationKey key{};

    static const char* PURE_COLOR_VS_SEMANTIC[] = {
        "vec4(vi.Position, 1.0)",
    };
    static const char* PURE_COLOR_FS_SEMANTIC[] = {
        "MaterialInstance mi = GetMI()",
        "return mi.Color",
    };

    static const char* VERTEX_COLOR_VS_SEMANTIC[] = {
        "Output.Color = vi.Color",
        "vec4(vi.Position, 1.0)",
    };
    static const char* VERTEX_COLOR_FS_SEMANTIC[] = {
        "return Input.Color",
    };

    static const char* GIZMO_VS_SEMANTIC[] = {
        "Output.Normal = GetNormal(vi.Normal)",
        "Output.Position = GetLocalToWorld() * vec4(vi.Position, 1.0)",
    };
    static const char* GIZMO_FS_SEMANTIC[] = {
        "SUN_DIRECTION",
        "MaterialInstance mi = GetMI()",
    };

    const BridgeCheckResult pure_color = RunBridgeCheck(
        "PureColor3D",
        PURE_COLOR_3D_COMPOSED_DEF,
        PURE_COLOR_3D_LOGIC,
        PURE_COLOR_VS_SEMANTIC,
        uint32_t(sizeof(PURE_COLOR_VS_SEMANTIC) / sizeof(PURE_COLOR_VS_SEMANTIC[0])),
        PURE_COLOR_FS_SEMANTIC,
        uint32_t(sizeof(PURE_COLOR_FS_SEMANTIC) / sizeof(PURE_COLOR_FS_SEMANTIC[0])),
        key);

    const BridgeCheckResult vertex_color = RunBridgeCheck(
        "VertexColor3D",
        VERTEX_COLOR_3D_COMPOSED_DEF,
        VERTEX_COLOR_3D_LOGIC,
        VERTEX_COLOR_VS_SEMANTIC,
        uint32_t(sizeof(VERTEX_COLOR_VS_SEMANTIC) / sizeof(VERTEX_COLOR_VS_SEMANTIC[0])),
        VERTEX_COLOR_FS_SEMANTIC,
        uint32_t(sizeof(VERTEX_COLOR_FS_SEMANTIC) / sizeof(VERTEX_COLOR_FS_SEMANTIC[0])),
        key);

    const BridgeCheckResult gizmo = RunBridgeCheck(
        "Gizmo3D",
        GIZMO_3D_COMPOSED_DEF,
        GIZMO_3D_LOGIC,
        GIZMO_VS_SEMANTIC,
        uint32_t(sizeof(GIZMO_VS_SEMANTIC) / sizeof(GIZMO_VS_SEMANTIC[0])),
        GIZMO_FS_SEMANTIC,
        uint32_t(sizeof(GIZMO_FS_SEMANTIC) / sizeof(GIZMO_FS_SEMANTIC[0])),
        key);

    const BridgeCheckResult all[] = {pure_color, vertex_color, gizmo};

    bool ok = true;
    for (const auto &r : all) {
        const bool pass = r.logic_valid
                       && r.bridge_ok
                       && r.no_missing_resource
                       && r.vs_generated
                       && r.fs_generated
                       && r.semantic_ok;

        printf("[%s]\n", r.name);
        printf("  ValidateMaterialLogicDef: %s\n", r.logic_valid ? "PASS" : "FAIL");
        printf("  BuildComposedMaterialDefFromLogic: %s\n", r.bridge_ok ? "PASS" : "FAIL");
        printf("  Missing resources: %s\n", r.no_missing_resource ? "PASS" : "FAIL");
        printf("  VS generated: %s\n", r.vs_generated ? "PASS" : "FAIL");
        printf("  FS generated: %s\n", r.fs_generated ? "PASS" : "FAIL");
        printf("  Semantic assertions: %s\n", r.semantic_ok ? "PASS" : "FAIL");
        printf("  Result: %s\n\n", pass ? "PASS" : "FAIL");

        ok = ok && pass;
    }

    printf("=== Bridge Validation Summary: %s ===\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}
