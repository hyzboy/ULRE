// MeshShaderToolchainTest validates Phase 2 mesh/task toolchain alignment.
//
// Coverage:
// 1) Runtime shader stage name/flag conversions for Task/Mesh.
// 2) Task and Mesh GLSL compilation through GLSLCompiler plugin.
// 3) SPIR-V OpEntryPoint execution model tags for task/mesh outputs.
// 4) Reflection parse path returns stable structures for these stages.

#include "../GLSLCompiler.h"
#include "../SPVParseData.h"

#include <hgl/common/ShaderStageDef.h>
#include <hgl/common/VertexInputDef.h>
#include <hgl/shadergen/GLSLCompilerConfig.h>
#include <hgl/shadergen/device/DeviceProfileTargetVersion.h>

#include <cstdio>
#include <cstdint>
#include <cstring>

using namespace hgl::graph;

static int g_failures = 0;

#define CHECK_TRUE(expr)                                                    \
    do {                                                                    \
        if (!(expr)) {                                                      \
            std::fprintf(stderr, "FAIL (%s:%d): %s\n",                    \
                         __FILE__, __LINE__, #expr);                        \
            ++g_failures;                                                   \
        }                                                                   \
    } while (0)

#define CHECK_EQ(a, b)  CHECK_TRUE((a) == (b))

namespace
{
    constexpr uint32_t kSpvOpEntryPoint = 15u;

    constexpr int32_t kSpvExecutionModelTaskNV = 5267;
    constexpr int32_t kSpvExecutionModelMeshNV = 5268;
    constexpr int32_t kSpvExecutionModelTaskEXT = 5364;
    constexpr int32_t kSpvExecutionModelMeshEXT = 5365;

    static void SetCompilerTargetToVulkan13()
    {
        using namespace hgl::graph::mtl::contract;

        PhysicalDeviceProfileLite profile;
        profile.name = "MeshShaderToolchainTest";
        profile.target_vulkan_version = MakeVkVersion(1, 3);
        profile.target_spv_version = SPV_VERSION_1_6;

        SetShaderCompilerPhysicalDeviceProfile(profile);
    }

    static int32_t FindExecutionModel(const SPVData *spv)
    {
        if (!spv || !spv->spv_data || spv->spv_length <= 5)
            return -1;

        const uint32_t *words = spv->spv_data;
        const uint32_t word_count = spv->spv_length;

        for (uint32_t i = 5; i < word_count;)
        {
            const uint32_t opword = words[i];
            const uint16_t word_span = static_cast<uint16_t>(opword >> 16);
            const uint16_t opcode = static_cast<uint16_t>(opword & 0xffffu);

            if (word_span == 0)
                break;

            if (opcode == kSpvOpEntryPoint)
            {
                if (i + 1u < word_count)
                    return static_cast<int32_t>(words[i + 1u]);

                return -1;
            }

            i += word_span;
        }

        return -1;
    }

    static bool IsTaskExecutionModel(const int32_t model)
    {
        return model == kSpvExecutionModelTaskNV
            || model == kSpvExecutionModelTaskEXT;
    }

    static bool IsMeshExecutionModel(const int32_t model)
    {
        return model == kSpvExecutionModelMeshNV
            || model == kSpvExecutionModelMeshEXT;
    }

    static SPVData *CompileChecked(const uint32_t stage, const char *source, const char *label)
    {
        SPVData *spv = CompileShader(stage, source);

        CHECK_TRUE(spv != nullptr);
        if (!spv)
            return nullptr;

        if (!spv->result)
        {
            std::fprintf(stderr,
                         "Compile failed for %s stage. Log:\n%s\n",
                         label,
                         spv->log ? spv->log : "<null>");
        }

        CHECK_TRUE(spv->result);
        CHECK_TRUE(spv->spv_data != nullptr);
        CHECK_TRUE(spv->spv_length > 0);

        return spv;
    }

    static void CheckParseDataSanity(SPVParseData *parse_data)
    {
        CHECK_TRUE(parse_data != nullptr);
        if (!parse_data)
            return;

        CHECK_TRUE(parse_data->stage_io.input.count == 0u);
        CHECK_TRUE(parse_data->stage_io.output.count == 0u);
        CHECK_TRUE(parse_data->push_constant.count == 0u);
        CHECK_TRUE(parse_data->subpass_input.count == 0u);

        for (uint32_t i = 0; i < VK_DESCRIPTOR_TYPE_COUNT; ++i)
            CHECK_TRUE(parse_data->resource[i].count == 0u);
    }
}

static void test_task_mesh_stage_name_mapping()
{
    const uint32_t task_flag = static_cast<uint32_t>(ShaderStage::Task);
    const uint32_t mesh_flag = static_cast<uint32_t>(ShaderStage::Mesh);

    CHECK_EQ(GetShaderStageFlagBits("task"), task_flag);
    CHECK_EQ(GetShaderStageFlagBits("mesh"), mesh_flag);
    CHECK_EQ(GetShaderStageFlagBits("TASK"), task_flag);
    CHECK_EQ(GetShaderStageFlagBits("MESH"), mesh_flag);

    const char *task_name = GetShaderStageName((VkShaderStageFlagBits)task_flag);
    const char *mesh_name = GetShaderStageName((VkShaderStageFlagBits)mesh_flag);

    CHECK_TRUE(task_name != nullptr);
    CHECK_TRUE(mesh_name != nullptr);

    if (task_name)
        CHECK_TRUE(std::strcmp(task_name, "Task") == 0);
    if (mesh_name)
        CHECK_TRUE(std::strcmp(mesh_name, "Mesh") == 0);
}

static void test_task_shader_compile_and_stage_tag()
{
    static const char *kTaskShaderSource = R"GLSL(
#version 460
#extension GL_EXT_mesh_shader : require
layout(local_size_x = 1) in;

void main()
{
    EmitMeshTasksEXT(1u, 1u, 1u);
}
)GLSL";

    SPVData *spv = CompileChecked(static_cast<uint32_t>(ShaderStage::Task),
                                  kTaskShaderSource,
                                  "Task");
    if (!spv)
        return;

    const int32_t exec_model = FindExecutionModel(spv);
    CHECK_TRUE(IsTaskExecutionModel(exec_model));

    SPVParseData *parse_data = ParseShaderSPV(spv);
    CheckParseDataSanity(parse_data);

    if (parse_data)
        FreeShaderSPVParseData(parse_data);

    FreeSPVData(spv);
}

static void test_mesh_shader_compile_and_stage_tag()
{
    static const char *kMeshShaderSource = R"GLSL(
#version 460
#extension GL_EXT_mesh_shader : require
layout(local_size_x = 1) in;
layout(triangles, max_vertices = 3, max_primitives = 1) out;

void main()
{
    SetMeshOutputsEXT(3u, 1u);

    gl_MeshVerticesEXT[0].gl_Position = vec4(-1.0, -1.0, 0.0, 1.0);
    gl_MeshVerticesEXT[1].gl_Position = vec4( 1.0, -1.0, 0.0, 1.0);
    gl_MeshVerticesEXT[2].gl_Position = vec4( 0.0,  1.0, 0.0, 1.0);

    gl_PrimitiveTriangleIndicesEXT[0] = uvec3(0u, 1u, 2u);
}
)GLSL";

    SPVData *spv = CompileChecked(static_cast<uint32_t>(ShaderStage::Mesh),
                                  kMeshShaderSource,
                                  "Mesh");
    if (!spv)
        return;

    const int32_t exec_model = FindExecutionModel(spv);
    CHECK_TRUE(IsMeshExecutionModel(exec_model));

    SPVParseData *parse_data = ParseShaderSPV(spv);
    CheckParseDataSanity(parse_data);

    if (parse_data)
        FreeShaderSPVParseData(parse_data);

    FreeSPVData(spv);
}

int main()
{
    if (!InitShaderCompiler())
    {
        std::fprintf(stderr, "InitShaderCompiler failed.\n");
        return 1;
    }

    SetCompilerTargetToVulkan13();

    test_task_mesh_stage_name_mapping();
    test_task_shader_compile_and_stage_tag();
    test_mesh_shader_compile_and_stage_tag();

    CloseShaderCompiler();

    if (g_failures > 0)
    {
        std::fprintf(stderr, "\n%d test(s) FAILED.\n", g_failures);
        return 1;
    }

    std::fprintf(stdout, "All MeshShaderToolchain tests passed.\n");
    return 0;
}
