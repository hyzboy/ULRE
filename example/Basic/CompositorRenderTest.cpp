// Step 5.10 — 端到端验证：Compositor 生成的 SPV → VkShaderModule → VkPipeline
//
// 本测试验证全链路：CompositorAssembler 生成 GLSL → GLSLCompiler 编译为 SPV
//   → VulkanDevice::CreateShaderModule → RenderPass::CreatePipeline → 有效 VkPipeline
//
// 如果 GLSLCompiler DLL 不可用，则测试跳过编译阶段，仅显示窗口。

#include<hgl/framework/WorkManager.h>
#include<hgl/shadergen/CompositorAssembler.h>
#include<hgl/shadergen/ShaderCompilerProfileAPI.h>
#include<hgl/mtl/new/NewDescriptorSetLayoutFactory.h>
#include<hgl/vk/VKShaderModule.h>
#include<hgl/vk/VKRenderPass.h>
#include<hgl/vk/pipeline/VKPipelineData.h>
#include<hgl/vk/pipeline/VKInlinePipeline.h>
#include<hgl/vk/VKRenderTarget.h>
#include<hgl/graph/render/RenderContext.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/VertexDataBufferManager.h>
#include<iostream>

using namespace hgl;
using namespace hgl::graph;

// GLSLCompiler 接口
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

class CompositorRenderTest : public WorkObject
{
    Pipeline *compositor_pipeline       = nullptr;
    NewPipelineLayoutData *layout_data  = nullptr;
    ShaderModule *vs_module             = nullptr;
    ShaderModule *fs_module             = nullptr;

    VertexDataBufferManager *vdbm       = nullptr;
    BlockAllocator::UserNode *vtx_node  = nullptr;
    BlockAllocator::UserNode *idx_node  = nullptr;

    bool test_passed = false;

    bool InitCompositorPipeline()
    {
        auto *device = GetDevice();
        if (!device)
        {
            std::cerr << "FAIL: No VulkanDevice available.\n";
            return false;
        }

        // ======== Phase 1: Generate GLSL ========
        std::cout << "=== Phase 1: CompositorAssembler::Assemble() ===\n";

        CompositorAssembler assembler("ShaderLibrary");
        auto assembled = assembler.Assemble(
            SurfaceType::Standard,
            BlendMode::Opaque,
            PassType::ForwardOpaque,
            QualityTier::Medium,
            PlatformBackend::PC
        );

        if (!assembled.success)
        {
            std::cerr << "FAIL: Assemble failed: " << assembled.error_message << "\n";
            return false;
        }
        std::cout << "OK: GLSL generated (VS: " << assembled.vertex_glsl.size()
                  << " bytes, FS: " << assembled.fragment_glsl.size() << " bytes)\n";

        // ======== Phase 2: Compile to SPV ========
        std::cout << "\n=== Phase 2: GLSL -> SPV ===\n";

        if (!InitShaderCompiler())
        {
            std::cerr << "SKIP: GLSLCompiler DLL not available.\n";
            return false;
        }

        AddShaderIncludePath("ShaderLibrary");

        SPVData *vs_spv = CompileShader(VK_SHADER_STAGE_VERTEX_BIT, assembled.vertex_glsl.c_str());
        if (!vs_spv || !vs_spv->result)
        {
            std::cerr << "FAIL: VS compile failed.\n";
            if (vs_spv && vs_spv->log) std::cerr << "  Log: " << vs_spv->log << "\n";
            if (vs_spv) FreeSPVData(vs_spv);
            CloseShaderCompiler();
            return false;
        }
        std::cout << "OK: VS compiled to " << vs_spv->spv_length << " SPIR-V words.\n";

        SPVData *fs_spv = CompileShader(VK_SHADER_STAGE_FRAGMENT_BIT, assembled.fragment_glsl.c_str());
        if (!fs_spv || !fs_spv->result)
        {
            std::cerr << "FAIL: FS compile failed.\n";
            if (fs_spv && fs_spv->log) std::cerr << "  Log: " << fs_spv->log << "\n";
            if (fs_spv) FreeSPVData(fs_spv);
            FreeSPVData(vs_spv);
            CloseShaderCompiler();
            return false;
        }
        std::cout << "OK: FS compiled to " << fs_spv->spv_length << " SPIR-V words.\n";

        CloseShaderCompiler();

        // ======== Phase 3: SPV → VkShaderModule ========
        std::cout << "\n=== Phase 3: SPV -> VkShaderModule ===\n";

        vs_module = device->CreateShaderModule(
            (VkShaderStageFlagBits)VK_SHADER_STAGE_VERTEX_BIT,
            vs_spv->spv_data,
            vs_spv->spv_length * sizeof(uint32_t)
        );
        FreeSPVData(vs_spv);

        if (!vs_module)
        {
            std::cerr << "FAIL: CreateShaderModule(VS) failed.\n";
            FreeSPVData(fs_spv);
            return false;
        }
        std::cout << "OK: VS VkShaderModule created.\n";

        fs_module = device->CreateShaderModule(
            (VkShaderStageFlagBits)VK_SHADER_STAGE_FRAGMENT_BIT,
            fs_spv->spv_data,
            fs_spv->spv_length * sizeof(uint32_t)
        );
        FreeSPVData(fs_spv);

        if (!fs_module)
        {
            std::cerr << "FAIL: CreateShaderModule(FS) failed.\n";
            return false;
        }
        std::cout << "OK: FS VkShaderModule created.\n";

        // ======== Phase 4: Pipeline Layout ========
        std::cout << "\n=== Phase 4: Pipeline Layout ===\n";

        layout_data = NewDescriptorSetLayoutFactory::CreateNewPipelineLayout(
            device->GetDevice(),
            SurfaceType::Standard,
            true    // ssbo_platform = true (PC 使用 SSBO 顶点获取)
        );

        if (!layout_data || layout_data->pipeline_layout == VK_NULL_HANDLE)
        {
            std::cerr << "FAIL: CreateNewPipelineLayout failed.\n";
            return false;
        }
        std::cout << "OK: VkPipelineLayout created (4-set layout).\n";

        // ======== Phase 5: Create VkPipeline ========
        std::cout << "\n=== Phase 5: VkPipeline Creation ===\n";

        auto *render_context = GetRenderContext();
        if (!render_context)
        {
            std::cerr << "FAIL: No RenderContext.\n";
            return false;
        }

        auto *render_target = render_context->GetCurrentRenderTarget();
        if (!render_target)
        {
            std::cerr << "FAIL: No RenderTarget.\n";
            return false;
        }

        auto *render_pass = render_target->GetRenderPass();
        if (!render_pass)
        {
            std::cerr << "FAIL: No RenderPass.\n";
            return false;
        }

        // 获取 Solid3D 预设的 PipelineData（深度测试、背面剔除等）
        const PipelineData *solid3d = GetPipelineData(InlinePipeline::Solid3D);
        if (!solid3d)
        {
            std::cerr << "FAIL: GetPipelineData(Solid3D) returned null.\n";
            return false;
        }

        // 组装 ShaderStageCreateInfoList
        ShaderStageCreateInfoList ssci;
        ssci.Add(*vs_module->GetCreateInfo());
        ssci.Add(*fs_module->GetCreateInfo());

        // 创建管线（VIL=nullptr → 空顶点输入，适用于 SSBO 顶点获取）
        compositor_pipeline = render_pass->CreatePipeline(
            "CompositorTest_Standard_ForwardOpaque_Medium_PC",
            ssci,
            layout_data->pipeline_layout,
            nullptr,    // 空顶点输入（SSBO fetch）
            solid3d
        );

        if (!compositor_pipeline)
        {
            std::cerr << "FAIL: RenderPass::CreatePipeline failed!\n";
            std::cerr << "      (Check Vulkan validation for details)\n";
            return false;
        }

        std::cout << "OK: VkPipeline created successfully!\n";
        std::cout << "  Pipeline name: " << compositor_pipeline->GetName().c_str() << "\n";

        return true;
    }

    bool InitSSBOUpload()
    {
        auto *device = GetDevice();

        // ======== Phase 6: SSBO Vertex Data Upload ========
        std::cout << "\n=== Phase 6: SSBO Vertex Data Upload ===\n";

        vdbm = new VertexDataBufferManager(device);
        if (!vdbm->Init(65536, 65536))
        {
            std::cerr << "FAIL: VertexDataBufferManager::Init failed.\n";
            return false;
        }
        std::cout << "OK: VertexDataBufferManager initialized (65536 vertices, 65536 indices).\n";

        // 三角形顶点数据（匹配 GLSL std430 VertexData 布局）
        SSBOVertexData triangle[3] = {};

        // Vertex 0: bottom-left
        triangle[0].position[0] = -0.5f; triangle[0].position[1] = -0.5f; triangle[0].position[2] = 0.0f;
        triangle[0].normal[0]   =  0.0f; triangle[0].normal[1]   =  0.0f; triangle[0].normal[2]   = 1.0f;
        triangle[0].uv0[0]      =  0.0f; triangle[0].uv0[1]      =  0.0f;

        // Vertex 1: bottom-right
        triangle[1].position[0] =  0.5f; triangle[1].position[1] = -0.5f; triangle[1].position[2] = 0.0f;
        triangle[1].normal[0]   =  0.0f; triangle[1].normal[1]   =  0.0f; triangle[1].normal[2]   = 1.0f;
        triangle[1].uv0[0]      =  1.0f; triangle[1].uv0[1]      =  0.0f;

        // Vertex 2: top-center
        triangle[2].position[0] =  0.0f; triangle[2].position[1] =  0.5f; triangle[2].position[2] = 0.0f;
        triangle[2].normal[0]   =  0.0f; triangle[2].normal[1]   =  0.0f; triangle[2].normal[2]   = 1.0f;
        triangle[2].uv0[0]      =  0.5f; triangle[2].uv0[1]      =  1.0f;

        vtx_node = vdbm->AllocateVertexBlock(3);
        if (!vtx_node)
        {
            std::cerr << "FAIL: AllocateVertexBlock(3) returned null.\n";
            return false;
        }
        std::cout << "OK: Allocated vertex block: offset=" << vtx_node->GetStart()
                  << ", count=" << vtx_node->GetCount() << "\n";

        if (!vdbm->UploadVertices(vtx_node, triangle, 3))
        {
            std::cerr << "FAIL: UploadVertices failed.\n";
            return false;
        }
        std::cout << "OK: Triangle vertices uploaded to SSBO staging buffer.\n";

        // 索引数据（可选，验证索引分配）
        uint32_t indices[3] = {0, 1, 2};

        idx_node = vdbm->AllocateIndexBlock(3);
        if (!idx_node)
        {
            std::cerr << "FAIL: AllocateIndexBlock(3) returned null.\n";
            return false;
        }
        std::cout << "OK: Allocated index block: offset=" << idx_node->GetStart()
                  << ", count=" << idx_node->GetCount() << "\n";

        if (!vdbm->UploadIndices(idx_node, indices, 3))
        {
            std::cerr << "FAIL: UploadIndices failed.\n";
            return false;
        }
        std::cout << "OK: Triangle indices uploaded to SSBO staging buffer.\n";

        // Dirty 状态验证
        if (!vdbm->IsDirty())
        {
            std::cerr << "FAIL: SSBO should be dirty after upload.\n";
            return false;
        }
        std::cout << "OK: SSBO dirty state is correct (dirty after upload).\n";

        // Descriptor info 验证
        auto vtx_info = vdbm->GetVertexDescriptorInfo();
        auto idx_info = vdbm->GetIndexDescriptorInfo();

        if (vtx_info.buffer == VK_NULL_HANDLE || vtx_info.range == 0)
        {
            std::cerr << "FAIL: Vertex SSBO descriptor info is invalid.\n";
            return false;
        }
        if (idx_info.buffer == VK_NULL_HANDLE || idx_info.range == 0)
        {
            std::cerr << "FAIL: Index SSBO descriptor info is invalid.\n";
            return false;
        }

        std::cout << "OK: SSBO descriptor buffer info valid.\n";
        std::cout << "  Vertex SSBO size: " << vtx_info.range << " bytes\n";
        std::cout << "  Index  SSBO size: " << idx_info.range << " bytes\n";
        std::cout << "  Expected per-vertex stride: " << sizeof(SSBOVertexData) << " bytes\n";

        std::cout << "\n  NOTE: Actual GPU rendering (vkCmdDraw) requires full UBO/descriptor\n";
        std::cout << "  chain (CameraUBO, L2W_SSBO, MI_Buffer, etc.) — deferred to Stage 7.\n";

        return true;
    }

public:

    bool Init() override
    {
        std::cout << "========================================\n";
        std::cout << " CompositorRenderTest — Step 5.10\n";
        std::cout << " Compositor SPV → VkPipeline 端到端验证\n";
        std::cout << "========================================\n\n";

        test_passed = InitCompositorPipeline();

        if (test_passed)
            test_passed = InitSSBOUpload();

        std::cout << "\n========================================\n";
        if (test_passed)
            std::cout << " RESULT: ALL PHASES PASSED (1-6)\n";
        else
            std::cout << " RESULT: SOME PHASES FAILED (see above)\n";
        std::cout << "========================================\n";

        // 返回 true 使窗口保持打开，方便查看控制台输出
        return true;
    }

    void Tick(double delta) override
    {
        WorkObject::Tick(delta);
    }

    void Render(double delta_time) override
    {
        // 暂不执行实际渲染 — 仅显示空窗口
        // 渲染集成将在 Stage 7 (Forward 材质迁移) 中实现
    }

    ~CompositorRenderTest()
    {
        delete vdbm;
    }
};

int main()
{
    return RunFramework<CompositorRenderTest>(OS_TEXT("CompositorRenderTest"), 800, 600);
}
