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

#include<hgl/mtl/new/DescriptorSetBindings.h>
#include<hgl/vk/VKShaderModule.h>
#include<hgl/vk/VKRenderPass.h>
#include<hgl/vk/pipeline/VKPipelineData.h>
#include<hgl/vk/pipeline/VKInlinePipeline.h>
#include<hgl/vk/VKRenderTarget.h>
#include<hgl/vk/VKBuffer.h>
#include<hgl/graph/render/RenderContext.h>
#include<hgl/graph/core/GraphicsContext.h>
#include<hgl/graph/VertexDataBufferManager.h>
#include<hgl/graph/CameraInfo.h>
#include<iostream>

using namespace hgl;
using namespace hgl::graph;

// GLSLCompiler 接口
namespace hgl::graph
{
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
    // --- Standard (Lit) pipeline ---
    Pipeline *compositor_pipeline       = nullptr;
    NewPipelineLayoutData *layout_data  = nullptr;
    ShaderModule *vs_module             = nullptr;
    ShaderModule *fs_module             = nullptr;

    // --- Unlit pipeline ---
    Pipeline *unlit_pipeline            = nullptr;
    NewPipelineLayoutData *unlit_layout = nullptr;
    ShaderModule *unlit_vs_module       = nullptr;
    ShaderModule *unlit_fs_module       = nullptr;

    VertexDataBufferManager *vdbm       = nullptr;
    BlockAllocator::UserNode *vtx_node  = nullptr;
    BlockAllocator::UserNode *idx_node  = nullptr;

    // --- Render resources (Phase 12+) ---
    DeviceBuffer *camera_ubo            = nullptr;    // CameraInfo UBO (Set 0, binding 0)
    DeviceBuffer *l2w_ssbo              = nullptr;    // L2W SSBO (Set 1, binding 0)
    DeviceBuffer *mi_ssbo               = nullptr;    // MI SSBO (Set 2, binding 0)
    VkDescriptorPool test_desc_pool     = VK_NULL_HANDLE;
    VkDescriptorSet  ds_per_scene       = VK_NULL_HANDLE;
    VkDescriptorSet  ds_per_view        = VK_NULL_HANDLE;
    VkDescriptorSet  ds_per_material    = VK_NULL_HANDLE;
    VkDescriptorSet  ds_per_draw        = VK_NULL_HANDLE;
    bool render_ready                   = false;

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

        AddShaderIncludePath("ShaderLibrary");

        SPVData *vs_spv = CompileShader(VK_SHADER_STAGE_VERTEX_BIT, assembled.vertex_glsl.c_str());
        if (!vs_spv || !vs_spv->result)
        {
            std::cerr << "FAIL: VS compile failed.\n";
            if (vs_spv && vs_spv->log) std::cerr << "  Log: " << vs_spv->log << "\n";
            if (vs_spv) FreeSPVData(vs_spv);

            return false;
        }
        std::cout << "OK: VS compiled to " << vs_spv->spv_length << " bytes (" << vs_spv->spv_length / sizeof(uint32_t) << " words).\n";

        SPVData *fs_spv = CompileShader(VK_SHADER_STAGE_FRAGMENT_BIT, assembled.fragment_glsl.c_str());
        if (!fs_spv || !fs_spv->result)
        {
            std::cerr << "FAIL: FS compile failed.\n";
            if (fs_spv && fs_spv->log) std::cerr << "  Log: " << fs_spv->log << "\n";
            if (fs_spv) FreeSPVData(fs_spv);
            FreeSPVData(vs_spv);

            return false;
        }
        std::cout << "OK: FS compiled to " << fs_spv->spv_length << " bytes (" << fs_spv->spv_length / sizeof(uint32_t) << " words).\n";

        // ======== Phase 3: SPV → VkShaderModule ========
        std::cout << "\n=== Phase 3: SPV -> VkShaderModule ===\n";

        vs_module = device->CreateShaderModule(
            (VkShaderStageFlagBits)VK_SHADER_STAGE_VERTEX_BIT,
            vs_spv->spv_data,
            vs_spv->spv_length
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
            fs_spv->spv_length
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

    bool InitUnlitCompositorPipeline()
    {
        auto *device = GetDevice();
        if (!device)
        {
            std::cerr << "FAIL: No VulkanDevice available.\n";
            return false;
        }

        // ======== Phase 7: Generate Unlit GLSL ========
        std::cout << "\n=== Phase 7: Unlit CompositorAssembler::Assemble() ===\n";

        CompositorAssembler assembler("ShaderLibrary");
        auto assembled = assembler.Assemble(
            SurfaceType::Unlit,
            BlendMode::Opaque,
            PassType::ForwardOpaque,
            QualityTier::Medium,
            PlatformBackend::PC
        );

        if (!assembled.success)
        {
            std::cerr << "FAIL: Unlit Assemble failed: " << assembled.error_message << "\n";
            return false;
        }
        std::cout << "OK: Unlit GLSL generated (VS: " << assembled.vertex_glsl.size()
                  << " bytes, FS: " << assembled.fragment_glsl.size() << " bytes)\n";

        // ======== Phase 8: Compile Unlit to SPV ========
        std::cout << "\n=== Phase 8: Unlit GLSL -> SPV ===\n";

        SPVData *vs_spv = CompileShader(VK_SHADER_STAGE_VERTEX_BIT, assembled.vertex_glsl.c_str());
        if (!vs_spv || !vs_spv->result)
        {
            std::cerr << "FAIL: Unlit VS compile failed.\n";
            if (vs_spv && vs_spv->log) std::cerr << "  Log: " << vs_spv->log << "\n";
            if (vs_spv) FreeSPVData(vs_spv);
            return false;
        }
        std::cout << "OK: Unlit VS compiled to " << vs_spv->spv_length << " bytes ("
                  << vs_spv->spv_length / sizeof(uint32_t) << " words).\n";

        SPVData *fs_spv = CompileShader(VK_SHADER_STAGE_FRAGMENT_BIT, assembled.fragment_glsl.c_str());
        if (!fs_spv || !fs_spv->result)
        {
            std::cerr << "FAIL: Unlit FS compile failed.\n";
            if (fs_spv && fs_spv->log) std::cerr << "  Log: " << fs_spv->log << "\n";
            if (fs_spv) FreeSPVData(fs_spv);
            FreeSPVData(vs_spv);
            return false;
        }
        std::cout << "OK: Unlit FS compiled to " << fs_spv->spv_length << " bytes ("
                  << fs_spv->spv_length / sizeof(uint32_t) << " words).\n";

        // ======== Phase 9: SPV → VkShaderModule ========
        std::cout << "\n=== Phase 9: Unlit SPV -> VkShaderModule ===\n";

        unlit_vs_module = device->CreateShaderModule(
            (VkShaderStageFlagBits)VK_SHADER_STAGE_VERTEX_BIT,
            vs_spv->spv_data,
            vs_spv->spv_length
        );
        FreeSPVData(vs_spv);

        if (!unlit_vs_module)
        {
            std::cerr << "FAIL: CreateShaderModule(Unlit VS) failed.\n";
            FreeSPVData(fs_spv);
            return false;
        }
        std::cout << "OK: Unlit VS VkShaderModule created.\n";

        unlit_fs_module = device->CreateShaderModule(
            (VkShaderStageFlagBits)VK_SHADER_STAGE_FRAGMENT_BIT,
            fs_spv->spv_data,
            fs_spv->spv_length
        );
        FreeSPVData(fs_spv);

        if (!unlit_fs_module)
        {
            std::cerr << "FAIL: CreateShaderModule(Unlit FS) failed.\n";
            return false;
        }
        std::cout << "OK: Unlit FS VkShaderModule created.\n";

        // ======== Phase 10: Pipeline Layout ========
        std::cout << "\n=== Phase 10: Unlit Pipeline Layout ===\n";

        unlit_layout = NewDescriptorSetLayoutFactory::CreateNewPipelineLayout(
            device->GetDevice(),
            SurfaceType::Unlit,
            true    // ssbo_platform = true (PC)
        );

        if (!unlit_layout || unlit_layout->pipeline_layout == VK_NULL_HANDLE)
        {
            std::cerr << "FAIL: CreateNewPipelineLayout(Unlit) failed.\n";
            return false;
        }
        std::cout << "OK: Unlit VkPipelineLayout created (4-set layout).\n";

        // ======== Phase 11: Create VkPipeline ========
        std::cout << "\n=== Phase 11: Unlit VkPipeline Creation ===\n";

        auto *render_context = GetRenderContext();
        if (!render_context) { std::cerr << "FAIL: No RenderContext.\n"; return false; }

        auto *render_target = render_context->GetCurrentRenderTarget();
        if (!render_target) { std::cerr << "FAIL: No RenderTarget.\n"; return false; }

        auto *render_pass = render_target->GetRenderPass();
        if (!render_pass) { std::cerr << "FAIL: No RenderPass.\n"; return false; }

        const PipelineData *solid3d = GetPipelineData(InlinePipeline::Solid3D);
        if (!solid3d) { std::cerr << "FAIL: GetPipelineData(Solid3D) returned null.\n"; return false; }

        ShaderStageCreateInfoList ssci;
        ssci.Add(*unlit_vs_module->GetCreateInfo());
        ssci.Add(*unlit_fs_module->GetCreateInfo());

        unlit_pipeline = render_pass->CreatePipeline(
            "CompositorTest_Unlit_ForwardOpaque_Medium_PC",
            ssci,
            unlit_layout->pipeline_layout,
            nullptr,    // 空顶点输入（SSBO fetch）
            solid3d
        );

        if (!unlit_pipeline)
        {
            std::cerr << "FAIL: RenderPass::CreatePipeline(Unlit) failed!\n";
            return false;
        }

        std::cout << "OK: Unlit VkPipeline created successfully!\n";
        std::cout << "  Pipeline name: " << unlit_pipeline->GetName().c_str() << "\n";

        return true;
    }

    bool InitRenderResources()
    {
        auto *device = GetDevice();
        if (!device || !unlit_pipeline || !unlit_layout || !vdbm)
            return false;

        VkDevice vk_dev = device->GetDevice();

        // ======== Phase 12: Create UBO/SSBO Buffers ========
        std::cout << "\n=== Phase 12: Create UBO/SSBO Buffers ===\n";

        // CameraInfo UBO — 填入简单正交 VP 矩阵
        {
            CameraInfo ci{};
            // 简单正交投影：[-1,1] → [-1,1]，无变换
            ci.projection       = Matrix4f(1.0f);
            ci.inverse_projection = Matrix4f(1.0f);
            ci.view             = Matrix4f(1.0f);
            ci.inverse_view     = Matrix4f(1.0f);
            ci.vp               = Matrix4f(1.0f);   // identity → NDC passthrough
            ci.inverse_vp       = Matrix4f(1.0f);
            ci.znear            = 0.1f;
            ci.zfar             = 100.0f;
            ci.use_reversed_z   = 0;

            camera_ubo = device->CreateBuffer(
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                sizeof(CameraInfo),
                &ci
            );
            if (!camera_ubo) { std::cerr << "FAIL: CreateBuffer(CameraInfo) failed.\n"; return false; }
            std::cout << "OK: CameraInfo UBO created (" << sizeof(CameraInfo) << " bytes).\n";
        }

        // L2W SSBO — 单个 identity 矩阵 (TransformID=0)
        {
            Matrix4f identity(1.0f);
            l2w_ssbo = device->CreateBuffer(
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                sizeof(Matrix4f),
                &identity
            );
            if (!l2w_ssbo) { std::cerr << "FAIL: CreateBuffer(L2W) failed.\n"; return false; }
            std::cout << "OK: L2W SSBO created (" << sizeof(Matrix4f) << " bytes).\n";
        }

        // MI SSBO — Unlit MaterialInstance: vec4 color (红色)
        {
            float color[4] = { 1.0f, 0.2f, 0.1f, 1.0f };  // 红色
            mi_ssbo = device->CreateBuffer(
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                sizeof(color),
                color
            );
            if (!mi_ssbo) { std::cerr << "FAIL: CreateBuffer(MI) failed.\n"; return false; }
            std::cout << "OK: MI SSBO created (16 bytes, color=red).\n";
        }

        // ======== Phase 13: Create Descriptor Pool & Allocate Sets ========
        std::cout << "\n=== Phase 13: Descriptor Pool & Allocate Sets ===\n";

        {
            VkDescriptorPoolSize pool_sizes[] = {
                { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         16 },
                { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         16 },
                { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 16 },
            };

            VkDescriptorPoolCreateInfo pool_ci{};
            pool_ci.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            pool_ci.maxSets       = 4;
            pool_ci.poolSizeCount = 3;
            pool_ci.pPoolSizes    = pool_sizes;

            if (vkCreateDescriptorPool(vk_dev, &pool_ci, nullptr, &test_desc_pool) != VK_SUCCESS)
            {
                std::cerr << "FAIL: vkCreateDescriptorPool failed.\n";
                return false;
            }
            std::cout << "OK: Descriptor pool created.\n";
        }

        // 分配 4 个描述符集
        {
            VkDescriptorSetAllocateInfo alloc_info{};
            alloc_info.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            alloc_info.descriptorPool     = test_desc_pool;
            alloc_info.descriptorSetCount = NEW_DS_COUNT;
            alloc_info.pSetLayouts        = unlit_layout->layouts;

            VkDescriptorSet sets[NEW_DS_COUNT];
            if (vkAllocateDescriptorSets(vk_dev, &alloc_info, sets) != VK_SUCCESS)
            {
                std::cerr << "FAIL: vkAllocateDescriptorSets failed.\n";
                return false;
            }
            ds_per_scene    = sets[0];
            ds_per_view     = sets[1];
            ds_per_material = sets[2];
            ds_per_draw     = sets[3];
            std::cout << "OK: 4 descriptor sets allocated.\n";
        }

        // ======== Phase 14: Write Descriptors ========
        std::cout << "\n=== Phase 14: Write Descriptors ===\n";

        {
            // 仅写入着色器实际访问的绑定（其余通过 PARTIALLY_BOUND 跳过）
            VkDescriptorBufferInfo camera_info = *camera_ubo->GetBufferInfo();
            VkDescriptorBufferInfo l2w_info    = *l2w_ssbo->GetBufferInfo();
            VkDescriptorBufferInfo mi_info     = *mi_ssbo->GetBufferInfo();

            auto vtx_info = vdbm->GetVertexDescriptorInfo();
            auto idx_info = vdbm->GetIndexDescriptorInfo();

            VkWriteDescriptorSet writes[5]{};

            // Set 0, binding 0: CameraInfo UBO
            writes[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[0].dstSet          = ds_per_scene;
            writes[0].dstBinding      = DSBinding::PerScene::CameraInfo;
            writes[0].descriptorCount = 1;
            writes[0].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            writes[0].pBufferInfo     = &camera_info;

            // Set 1, binding 0: L2W SSBO
            writes[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[1].dstSet          = ds_per_view;
            writes[1].dstBinding      = DSBinding::PerView::LocalToWorld;
            writes[1].descriptorCount = 1;
            writes[1].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            writes[1].pBufferInfo     = &l2w_info;

            // Set 2, binding 0: MI SSBO
            writes[2].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[2].dstSet          = ds_per_material;
            writes[2].dstBinding      = DSBinding::PerMaterial::MI_SSBO;
            writes[2].descriptorCount = 1;
            writes[2].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            writes[2].pBufferInfo     = &mi_info;

            // Set 3, binding 18: VertexDataBuffer SSBO
            writes[3].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[3].dstSet          = ds_per_draw;
            writes[3].dstBinding      = DSBinding::PerDraw::VertexDataBuffer;
            writes[3].descriptorCount = 1;
            writes[3].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            writes[3].pBufferInfo     = &vtx_info;

            // Set 3, binding 19: IndexDataBuffer SSBO
            writes[4].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[4].dstSet          = ds_per_draw;
            writes[4].dstBinding      = DSBinding::PerDraw::IndexDataBuffer;
            writes[4].descriptorCount = 1;
            writes[4].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            writes[4].pBufferInfo     = &idx_info;

            vkUpdateDescriptorSets(vk_dev, 5, writes, 0, nullptr);
            std::cout << "OK: 5 descriptor writes committed.\n";
        }

        render_ready = true;
        std::cout << "OK: Render resources ready.\n";
        return true;
    }

public:

    bool Init() override
    {
        std::cout << "========================================\n";
        std::cout << " CompositorRenderTest — Step 5.10 + 7.1\n";
        std::cout << " Compositor SPV → VkPipeline 端到端验证\n";
        std::cout << "========================================\n\n";

        // Standard (Lit) pipeline test (Phase 1-5)
        test_passed = InitCompositorPipeline();

        // SSBO upload test (Phase 6)
        if (test_passed)
            test_passed = InitSSBOUpload();

        // Unlit pipeline test (Phase 7-11)
        if (test_passed)
            test_passed = InitUnlitCompositorPipeline();

        // Render resource setup (Phase 12-14)
        if (test_passed)
            test_passed = InitRenderResources();

        std::cout << "\n========================================\n";
        if (test_passed)
            std::cout << " RESULT: ALL PHASES PASSED (1-14)\n";
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
        if (!render_ready)
            return;

        auto *world = GetECSContext();
        if (!world)
            return;

        auto *cmd = world->GetCurrentRenderCmd();
        if (!cmd)
            return;

        // Bind Unlit pipeline
        cmd->BindPipeline(unlit_pipeline);

        // Bind all 4 descriptor sets
        VkDescriptorSet sets[NEW_DS_COUNT] = { ds_per_scene, ds_per_view, ds_per_material, ds_per_draw };
        cmd->BindDescriptorSets(
            unlit_layout->pipeline_layout,
            0,              // firstSet
            sets,
            NEW_DS_COUNT,   // setCount
            nullptr,        // dynamicOffsets
            0               // dynamicOffsetCount
        );

        // Draw: 3 vertices, 1 instance, firstVertex = SSBO offset
        uint32_t first_vertex = vtx_node ? vtx_node->GetStart() : 0;
        vkCmdDraw(*cmd, 3, 1, first_vertex, 0);
    }

    ~CompositorRenderTest()
    {
        auto *device = GetDevice();
        VkDevice vk_dev = device ? device->GetDevice() : VK_NULL_HANDLE;

        if (vk_dev != VK_NULL_HANDLE && test_desc_pool != VK_NULL_HANDLE)
            vkDestroyDescriptorPool(vk_dev, test_desc_pool, nullptr);

        delete camera_ubo;
        delete l2w_ssbo;
        delete mi_ssbo;
        delete layout_data;
        delete unlit_layout;
        delete vdbm;
    }
};

int main()
{
    return RunFramework<CompositorRenderTest>(OS_TEXT("CompositorRenderTest"), 800, 600);
}
