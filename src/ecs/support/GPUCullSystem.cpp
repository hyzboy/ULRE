#include <hgl/ecs/support/GPUCullSystem.h>
#include <hgl/ecs/support/CameraInfoStorage.h>
#include <hgl/ecs/support/PerfTimer.h>
#include <hgl/ecs/core/MaterialBatch.h>
#include <hgl/ecs/core/Entity.h>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <hgl/ecs/core/EntityGPU.h>
#include <hgl/ecs/support/EntityGPUTable.h>
#include <hgl/graph/module/GlobalSSBOBufferRegistry.h>
#include <hgl/mtl/MeshShaderLimits.h>
#include <hgl/mtl/ShaderLibraryPath.h>
#include <hgl/graph/module/ShaderProgramManager.h>
#include <hgl/graph/module/BufferManager.h>
#include <hgl/graph/core/GraphicsContext.h>
#include <hgl/vk/VKCommandBuffer.h>
#include <hgl/vk/VKDevice.h>
#include <hgl/vk/buffer/BufferPolicy.h>
#include <hgl/vk/VKBufferReadback.h>
#include <hgl/io/FileInputStream.h>
#include <hgl/type/Smart.h>
#include <hgl/log/Log.h>
#include <hgl/time/Time.h>

namespace hgl::ecs
{
    namespace
    {
        // Push constants 结构（必须与 GLSL 侧 entity_gpu_cull.glsl 的
        // layout(push_constant) **逐字段同序同型**：u64 全部前置（8B 对齐自然成链），
        // u32 收尾。布局漂移 = shader 读错字段 = 静默剔错，门抓不到（push constant
        // 不经描述符 schema）。
        struct CullPushConstants
        {
            uint64_t addr_world_addresses;
            uint64_t addr_global_addresses;
            uint64_t visible_indices_addr;
            uint64_t indirect_commands_addr;
            uint64_t l2w_out_addr;
            uint64_t cmds_out_addr;
            uint64_t mtl_out_addr;
            uint64_t draw_count_addr;
            uint64_t batch_tag_addr;      // T3i：批标签数组（当前帧槽；0xFFFFFFFF = 无批）
            uint64_t batch_info_addr;     // T3i：批分区表（{row_base, capacity} × batch_count）
            uint64_t batch_counter_addr;  // T3i：每批计数（末位 = overflow 计数）
            uint32_t entity_count;
            uint32_t max_visible_count;
            uint32_t camera_row;          // ECSContext::GetActiveCameraRow()
            uint32_t batch_count;         // T3i：本帧主帧批数（0 = 无接管批）
            uint32_t batch_block_stride;  // T4c2：批行/计数每 pass 块的行数（= kBatchBlockStride）
            uint32_t batch_count_shadow;  // 本帧阴影批数（0 = 无阴影接管批）
        };

        // 11×u64(88) + 6×u32(24) = 112（对齐 8）
        static_assert(sizeof(CullPushConstants) == 112, "CullPushConstants layout mismatch");
        static_assert(offsetof(CullPushConstants, batch_tag_addr) == 64, "T3i pc 段偏移");
        static_assert(offsetof(CullPushConstants, batch_count) == 100, "T3i pc 段偏移");
        static_assert(offsetof(CullPushConstants, batch_block_stride) == 104, "T4c2 pc 段偏移");
        static_assert(offsetof(CullPushConstants, batch_count_shadow) == 108, "batch_count_shadow pc 段偏移");

        // 引擎合规文件读取（先例：ShaderCodeModuleRegistry::LoadFileContent ——
        // 同一读法与缓冲口径），路径由 ShaderLibrary 单源解析（禁绝对路径）。
        AnsiString LoadShaderSource(const std::string &path)
        {
            hgl::io::OpenFileInputStream opener(hgl::ToOSString(path.c_str()));
            if (!opener)
            {
                GLogError(u8"[GPUCullSystem] Failed to open shader: %s", path.c_str());
                return AnsiString();
            }

            const int64 size = opener->GetSize();
            if (size <= 0)
                return AnsiString();

            hgl::AutoDeleteArray<char> buffer(size_t(size) + 1);
            if (!buffer)
                return AnsiString();

            if (opener->Read(static_cast<void *>(buffer.data()), size) != size)
                return AnsiString();

            buffer[size_t(size)] = 0;
            return AnsiString(buffer.data(), int(size));
        }
    }

    GPUCullSystem::GPUCullSystem()
        : System("GPUCullSystem")
    {
        // T3i/2β：**必须晚于 RenderBatch 的批装配**——批标签/分区是本帧产物，
        // cull 要与本帧分区、本帧绘制三者同源（在 RenderCollect 会读到上一帧批归属
        // ⇒ 切片错位）。RenderFrameSync 位于"render pass 外、上传之后、绘制之前"，
        // 且本提交仍早于主帧 cmd 提交 ⇒ 同队列 submission-order 保证绘制可见其写出。
        SetExecutionPhase(ExecutionPhase::RenderFrameSync);
    }

    void GPUCullSystem::Shutdown()
    {
        // 幂等：每个成员释放后置空，Shutdown 与析构可重复进入（ECSContext 的 active
        // 路径先调 Shutdown 再销毁系统对象，析构里的调用此时全是空操作）。
        //
        // compute_pipeline_ 的所有权：`ShaderProgramManager::CreateComputePipeline` 走的是
        // `device->CreateComputePipeline()` 的裸 new（owns_pipeline_layout=true，专用
        // VkPipelineLayout 随它一起销毁）。ShaderProgramManager 只缓存 **ShaderModule**，
        // **不缓存也不持有** ComputePipeline ⇒ 必须由这里 delete，否则 VkPipeline 与
        // VkPipelineLayout 双双泄漏（旧注释「由 MaterialManager 管理」是错的）。
        const bool had_pipeline = (compute_pipeline_ != nullptr);
        SAFE_CLEAR(compute_pipeline_);

        // 派发资源显式释放（T3g 泄漏修复的另一半：与 CreateDispatchResources 一次
        // new 对应；fence/cmd 的最终归属由 DeviceQueue/DeviceBuffer 析构链处理）
        SAFE_CLEAR(cull_queue_);
        for (graph::ComputeCmdBuffer *&cmd : cull_cmds_)
            SAFE_CLEAR(cmd);

        SAFE_CLEAR(visible_indices_buffer_);
        SAFE_CLEAR(indirect_commands_buffer_);
        SAFE_CLEAR(draw_count_buffer_);
        SAFE_CLEAR(l2w_out_buffer_);
        SAFE_CLEAR(cmds_out_buffer_);
        SAFE_CLEAR(mtl_out_buffer_);
        SAFE_CLEAR(batch_tag_buffer_);
        SAFE_CLEAR(batch_info_buffer_);
        SAFE_CLEAR(batch_counter_buffer_);

        // 映射指针随 buffer 一起失效，必须同时复位（否则重建后仍指向旧映射）
        batch_tag_cpu_ = nullptr;
        batch_tag_cpu_slot_ = UINT32_MAX;
        batch_counter_cpu_ = nullptr;

        initialized_ = false;

        GLogInfo(u8"[GPUCullSystem] Shutdown 完成：compute pipeline %s，派发资源与缓冲已释放",
                 had_pipeline ? u8"已销毁" : u8"无需销毁");
    }

    GPUCullSystem::~GPUCullSystem()
    {
        // 兜底：ECSContext 的 inactive 路径直接清系统容器、不经 System::Shutdown；
        // 那时成员已在 Shutdown 里置空（若之前调过），重复调用不会碰设备。
        Shutdown();
    }

    bool GPUCullSystem::Initialize(ECSContext* context)
    {
        GLogInfo(u8"[GPUCullSystem] Initialize started (deferred)");
        
        if (!context) 
        {
            GLogError(u8"[GPUCullSystem] Initialize failed: context is null");
            return false;
        }
        context_ = context;
        
        // 延迟初始化：设备可能还没准备好，等到第一次 Update 时再初始化
        GLogInfo(u8"[GPUCullSystem] Initialize registered, will init on first Update");
        return true;
    }

    bool GPUCullSystem::EnsureInitialized()
    {
        if (initialized_)
            return true;
            
        if (!context_)
            return false;
            
        device_ = context_->GetGPUDevice();
        if (!device_) 
        {
            return false;  // 设备还没准备好，继续等待
        }

        GLogInfo(u8"[GPUCullSystem] Deferred initialization started");

        if (!CreateComputePipeline())
        {
            GLogError(u8"[GPUCullSystem] Failed to create compute pipeline");
            return false;
        }

        if (!CreateOutputBuffers())
        {
            GLogError(u8"[GPUCullSystem] Failed to create output buffers");
            return false;
        }

        if (!CreateDispatchResources())
        {
            GLogError(u8"[GPUCullSystem] Failed to create dispatch resources");
            return false;
        }

        initialized_ = true;
        GLogInfo(u8"[GPUCullSystem] Initialized: max_visible=%u, work_group=%u",
                 kMaxVisibleEntities, kWorkGroupSize);
        return true;
    }

    bool GPUCullSystem::CreateComputePipeline()
    {
        auto* gc = context_->GetGraphicsContext();
        if (!gc) 
        {
            GLogError(u8"[GPUCullSystem] CreateComputePipeline: GetGraphicsContext returned null");
            return false;
        }

        auto* shader_manager = gc->GetMaterialManager();
        if (!shader_manager) 
        {
            GLogError(u8"[GPUCullSystem] CreateComputePipeline: GetMaterialManager returned null");
            return false;
        }

        // 加载着色器源码（ShaderLibrary 单源路径，禁绝对路径）
        const std::string shader_path =
            graph::mtl::GetShaderLibraryPath() + "/ecs/entity_gpu_cull.glsl";
        AnsiString shader_source = LoadShaderSource(shader_path);
        if (shader_source.IsEmpty())
        {
            GLogError(u8"[GPUCullSystem] CreateComputePipeline: Failed to load shader source: %s",
                      shader_path.c_str());
            return false;
        }

        GLogInfo(u8"[GPUCullSystem] Shader source loaded, size=%d", shader_source.Length());

        // 创建 compute pipeline
        compute_pipeline_ = shader_manager->CreateComputePipeline(
            "GPUCullSystem.Pipeline",
            shader_source,
            sizeof(CullPushConstants)
        );

        if (!compute_pipeline_)
        {
            GLogError(u8"[GPUCullSystem] CreateComputePipeline failed (pipeline is null)");
            return false;
        }

        GLogInfo(u8"[GPUCullSystem] Compute pipeline created successfully");
        return true;
    }

    bool GPUCullSystem::CreateOutputBuffers()
    {
        if (!device_) return false;

        // 可见实体索引表（uint32_t * kMaxVisibleEntities）
        visible_indices_buffer_ = device_->CreateBuffer(
            "GPUCullSystem.VisibleIndices",
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            kMaxVisibleEntities * sizeof(uint32_t),
            kMaxVisibleEntities * sizeof(uint32_t),
            nullptr,
            graph::BufferAllocPolicy::Auto,
            graph::SharingMode::Exclusive
        );
        if (!visible_indices_buffer_) return false;
        visible_indices_addr_ = device_->GetBufferDeviceAddressAligned16(visible_indices_buffer_->GetBuffer());

        // 间接绘制命令表（IndirectMeshTaskBuffer）
        // VkDrawMeshTasksIndirectCommandEXT = { groupCountX, groupCountY, groupCountZ }
        // ⚠ T4c1 漏改：ICB 必须与 gather 三表同域 —— 按 **kArenaRows** 分配。
        //   阴影扇区切片从 4096 行起，绘制偏移 = row_base × 12 ⇒ 只按 4096 条分配会触发
        //   VUID-vkCmdDrawMeshTasksIndirectCountEXT 越界读（实测 AMDVLK ⇒ VK_ERROR_DEVICE_LOST）。
        indirect_commands_buffer_ = device_->CreateIndirectMeshTaskBuffer(
            kArenaRows,
            graph::BufferAllocPolicy::Auto,
            "GPUCullSystem.IndirectCommands",
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT
        );
        if (!indirect_commands_buffer_) return false;
        indirect_commands_addr_ = device_->GetBufferDeviceAddressAligned16(indirect_commands_buffer_->GetBuffer());

        // 可见计数（uint32_t）
        // ⚠ usage 必带 TRANSFER_DST：T3g 起计数清零走 `vkCmdFillBuffer`
        //（VUID-vkCmdFillBuffer-dstBuffer-00031：dst 必须建缓冲时就带该位）
        draw_count_buffer_ = device_->CreateBuffer(
            "GPUCullSystem.DrawCount",
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            sizeof(uint32_t),
            sizeof(uint32_t),
            nullptr,
            graph::BufferAllocPolicy::Auto,
            graph::SharingMode::Exclusive
        );
        if (!draw_count_buffer_) return false;
        draw_count_addr_ = device_->GetBufferDeviceAddressAligned16(draw_count_buffer_->GetBuffer());

        // ── 形态 B（T3f-2）gather 输出三表（容量 = kArenaRows = 每 pass 4096 × 2 扇区；icb 复用上面）──
        // 行距契约（与着色器镜像逐字段一致，§8.2d 的"读真源零复制"口径）：
        //   l2w_out  = uint32 × 4 B/行；cmds_out = MeshDrawCommand 8 B/行；
        //   mtl_out  = MaterialInstanceAddresses 16 B/行。
        // usage：STORAGE（CS 写）+ INDIRECT（icb/count 已有；三张行表被 mesh shader
        // 经 BDA 解引用 ⇒ SHADER_DEVICE_ADDRESS 必带，与 §7.6 设备地址纪律一致）。
        const VkDeviceSize l2w_bytes  = VkDeviceSize(kArenaRows) * sizeof(uint32_t);
        const VkDeviceSize cmds_bytes = VkDeviceSize(kArenaRows) * 8;    // MeshDrawCommand
        const VkDeviceSize mtl_bytes  = VkDeviceSize(kArenaRows) * 16;   // MaterialInstanceAddresses

        const VkBufferUsageFlags gather_usage =
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

        l2w_out_buffer_ = device_->CreateBuffer(
            "GPUCullSystem.L2WOut", gather_usage, l2w_bytes, l2w_bytes, nullptr,
            graph::BufferAllocPolicy::Auto, graph::SharingMode::Exclusive);
        if (!l2w_out_buffer_) return false;
        l2w_out_addr_ = device_->GetBufferDeviceAddressAligned16(l2w_out_buffer_->GetBuffer());

        cmds_out_buffer_ = device_->CreateBuffer(
            "GPUCullSystem.CmdsOut", gather_usage, cmds_bytes, cmds_bytes, nullptr,
            graph::BufferAllocPolicy::Auto, graph::SharingMode::Exclusive);
        if (!cmds_out_buffer_) return false;
        cmds_out_addr_ = device_->GetBufferDeviceAddressAligned16(cmds_out_buffer_->GetBuffer());

        mtl_out_buffer_ = device_->CreateBuffer(
            "GPUCullSystem.MtlOut", gather_usage, mtl_bytes, mtl_bytes, nullptr,
            graph::BufferAllocPolicy::Auto, graph::SharingMode::Exclusive);
        if (!mtl_out_buffer_) return false;
        mtl_out_addr_ = device_->GetBufferDeviceAddressAligned16(mtl_out_buffer_->GetBuffer());

        // ── T3i（③ 多批）：批标签 / 分区 / 每批计数（按帧槽分份；CPU 只写当前份）──
        // tag = kSlots × 4096 × u32；info = kSlots × kArenaPasses × kBatchBlockStride × 8 B；
        // counter = kSlots × kArenaPasses × kBatchBlockStride × u32（每块末位 = 该块 overflow 槽）。
        // usage：STORAGE（CS 读写）+ SHADER_DEVICE_ADDRESS（BDA 下发）；counter 另带
        // TRANSFER_DST（cull cmd 内 FillBuffer 清零，VUID-vkCmdFillBuffer-dstBuffer-00031）。
        // T4c2 同族：标签表**按 pass 分块**（块步长 = kMaxVisibleEntities，与实体 4-ID 表同口径）。
        // 此前只有帧槽维度 => 一帧内两个 pass 共用一张表，而 `BeginBatchTagWrite` 整表清空重填
        // => 后装配的 pass 盖掉先装配的标签 => 静止物体按**另一 pass 的批索引**写行 =>
        // 挤进批 0（其 capacity=1）=> 切片溢出、该行被丢
        //（实测 AlphaTestShadow/CascadeShadowMap 每图一帧地面消失；见计划 §11.9）。
        const VkDeviceSize tag_slot_bytes     = VkDeviceSize(kArenaPasses * kMaxVisibleEntities) * sizeof(uint32_t);
        const VkDeviceSize info_slot_bytes    = VkDeviceSize(kArenaPasses * kBatchBlockStride) * sizeof(BatchInfoRow);
        const VkDeviceSize counter_slot_bytes = VkDeviceSize(kArenaPasses * kBatchBlockStride) * sizeof(uint32_t);
        const VkDeviceSize batch_slots        = VkDeviceSize(HGL_FRAME_SLOT_TOTAL);

        const VkBufferUsageFlags batch_usage =
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

        // ── T4c-ini：三个按帧槽分份的缓冲**建缓冲即填初值** ──────────────────────
        // 每个帧槽的布局只在"轮到它的那一帧"才被写；未写过的槽（头几帧 / 离屏 RT 的槽 4/5）
        // 若被当成有效布局 ⇒ gather 按垃圾 `capacity`/`row_base` 往**共享竞技场**写行；
        // 主帧侧则表现为"布局未初始化 ⇒ count=0 ⇒ 该帧不画"（实测 AlphaTestShadow 第 1 帧）。
        // 约定：标签初值 = `kInvalidBatchTag`（cull 直接跳过）；布局/计数初值 = 0
        // （`capacity == 0` ⇒ 该 pass 该批跳过，与"未接管"同口径）。
        std::vector<uint32_t> tag_init(tag_slot_bytes * batch_slots / sizeof(uint32_t),
                                       GPUCullSystem::kInvalidBatchTag);
        std::vector<uint8_t>  zero_init(size_t(info_slot_bytes * batch_slots), 0);
        std::vector<uint8_t>  counter_zero_init(size_t(counter_slot_bytes * batch_slots), 0);

        batch_tag_buffer_ = device_->CreateBuffer(
            "GPUCullSystem.BatchTag", batch_usage,
            tag_slot_bytes * batch_slots, tag_slot_bytes * batch_slots, tag_init.data(),
            graph::BufferAllocPolicy::Auto, graph::SharingMode::Exclusive);
        if (!batch_tag_buffer_) return false;
        batch_tag_addr_ = device_->GetBufferDeviceAddressAligned16(batch_tag_buffer_->GetBuffer());

        batch_info_buffer_ = device_->CreateBuffer(
            "GPUCullSystem.BatchInfo", batch_usage,
            info_slot_bytes * batch_slots, info_slot_bytes * batch_slots, zero_init.data(),
            graph::BufferAllocPolicy::Auto, graph::SharingMode::Exclusive);
        if (!batch_info_buffer_) return false;
        batch_info_addr_ = device_->GetBufferDeviceAddressAligned16(batch_info_buffer_->GetBuffer());

        // ⚠ 必带 INDIRECT_BUFFER：本缓冲当 `vkCmdDrawMeshTasksIndirectCountEXT` 的
        // countBuffer 用（VUID-vkCmdDrawMeshTasksIndirectCountEXT-countBuffer-02715）；
        // 另需 TRANSFER_DST 供 cull cmd 内 FillBuffer 清零。
        batch_counter_buffer_ = device_->CreateBuffer(
            "GPUCullSystem.BatchCounter",
            batch_usage | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            counter_slot_bytes * batch_slots, counter_slot_bytes * batch_slots, counter_zero_init.data(),
            graph::BufferAllocPolicy::Auto, graph::SharingMode::Exclusive);
        if (!batch_counter_buffer_) return false;
        batch_counter_addr_ = device_->GetBufferDeviceAddressAligned16(batch_counter_buffer_->GetBuffer());

        GLogInfo(u8"[GPUCullSystem] T3i batch buffers ready: tag=%p info=%p counter=%p（slots=%u, max_batches=%u）",
                 (void*)batch_tag_addr_, (void*)batch_info_addr_, (void*)batch_counter_addr_,
                 uint32_t(HGL_FRAME_SLOT_TOTAL), kMaxBatches);

        GLogInfo(u8"[GPUCullSystem] Output buffers created: visible_indices=%p, indirect_commands=%p, draw_count=%p, gather_l2w=%p cmds=%p mtl=%p",
                 (void*)visible_indices_addr_, (void*)indirect_commands_addr_, (void*)draw_count_addr_,
                 (void*)l2w_out_addr_, (void*)cmds_out_addr_, (void*)mtl_out_addr_);
        return true;
    }

    // ── T3i（③ 多批）批标签 / 分区 / 计数 ─────────────────────────────

    uint32_t *GPUCullSystem::BeginBatchTagWrite(uint32_t pass_index)
    {
        if (!batch_tag_buffer_ || !context_ || pass_index >= kArenaPasses)
            return nullptr;

        // T4c2：本 pass 那一块批分区清零——**必须每帧清**，否则上一帧写过、本帧没写的批
        // 会留下残留行（capacity 非零），被本帧仍有效的标签引用 ⇒ 凭空写行。
        for (uint32_t i = 0; i < kBatchBlockStride; ++i)
            batch_info_staging_[BatchBlockBase(pass_index) + i] = BatchInfoRow{};

        const uint32_t slot = context_->GetFrameIndex() % HGL_FRAME_SLOT_TOTAL;
        const VkDeviceSize slot_bytes = VkDeviceSize(kArenaPasses * kMaxVisibleEntities) * sizeof(uint32_t);
        const VkDeviceSize block_bytes = VkDeviceSize(kMaxVisibleEntities) * sizeof(uint32_t);

        // 只 Map / 只清**本 pass 那一块**（整表清空会盖掉另一 pass 的标签）。
        uint32_t *p = static_cast<uint32_t *>(
            batch_tag_buffer_->Map(VkDeviceSize(slot) * slot_bytes
                                       + VkDeviceSize(pass_index) * block_bytes,
                                   block_bytes));
        if (!p)
            return nullptr;

        batch_tag_cpu_ = p;
        batch_tag_cpu_slot_ = slot;

        // 整表打"无批"初值：本帧未上标签的实体（未接管批 / 未进批）一律被 cull 跳过。
        for (uint32_t i = 0; i < kMaxVisibleEntities; ++i)
            p[i] = kInvalidBatchTag;

        return p;
    }

    void GPUCullSystem::SetBatchLayout(uint32_t pass_index, uint32_t batch_index, uint32_t row_base,
                                       uint32_t capacity, uint32_t topology, uint32_t group_size)
    {
        if (pass_index >= kArenaPasses)
        {
            GLogError(u8"[GPUCullSystem] pass 号越界：%u ≥ kArenaPasses(%u)（fail-fast）",
                      pass_index, kArenaPasses);
            return;
        }

        const uint32_t stage_index = BatchBlockBase(pass_index) + batch_index;

        if (batch_index >= kMaxBatches)
        {
            GLogError(u8"[GPUCullSystem] 批号越界：%u ≥ kMaxBatches(%u)（超限 = 项目 bug，fail-fast）",
                      batch_index, kMaxBatches);
            return;
        }

        // 契约（§9 M4）：切片起点必须 **4 行对齐**——绘制侧把三表 push 地址加 `row_base × 行字节`
        // （cmds 8B / l2w 4B / mtl 16B），而引用声明是 16B 对齐 ⇒ `row_base % 4 == 0` 是同时满足
        // 三者的唯一条件（§8.6 T3i-4）。违反 = 装配侧 bug（推进式 `(cursor + capacity + 3) & ~3`），
        // 响亮报一次（不静默写出违对齐的切片）。
        {
            if (!reported_misaligned_rows_ && (row_base % 4u) != 0u)
            {
                reported_misaligned_rows_ = true;
                GLogError(u8"[GPUCullSystem] 切片起点未 4 行对齐：row_base=%u（契约 §9 M4）"
                          u8"⇒ 三表 push 地址会违 16B 对齐", row_base);
            }
        }

        batch_info_staging_[stage_index].row_base   = row_base;
        batch_info_staging_[stage_index].capacity   = capacity;
        batch_info_staging_[stage_index].topology   = topology;
        batch_info_staging_[stage_index].group_size = group_size;
    }

    void GPUCullSystem::EndBatchLayout(uint32_t pass_index, uint32_t batch_count)
    {
        if (batch_count > kMaxBatches)
        {
            GLogError(u8"[GPUCullSystem] 批数越界：%u > kMaxBatches(%u) ⇒ 钳到上限（超限 = 项目 bug）",
                      batch_count, kMaxBatches);
            batch_count = kMaxBatches;
        }

        batch_count_[pass_index] = batch_count;

        // 1) 标签 unmap（Map 期内已直写；host-visible ⇒ unmap 后对后续提交可见）
        if (batch_tag_cpu_ && batch_tag_buffer_)
        {
            batch_tag_buffer_->Unmap();
            batch_tag_cpu_ = nullptr;
            batch_tag_cpu_slot_ = UINT32_MAX;
        }

        // 2) 分区表落**当前帧槽**那一份（T4c2：整张两块一起落——未写的那块是零，
        //    shader 据 `capacity == 0` 跳过该 pass）
        if (batch_info_buffer_ && context_ && batch_count > 0 && pass_index < kArenaPasses)
        {
            const uint32_t slot = context_->GetFrameIndex() % HGL_FRAME_SLOT_TOTAL;
            const VkDeviceSize slot_bytes = VkDeviceSize(kArenaPasses * kBatchBlockStride) * sizeof(BatchInfoRow);

            void *dst = batch_info_buffer_->Map(VkDeviceSize(slot) * slot_bytes, slot_bytes);
            if (dst)
            {
                std::memcpy(dst, batch_info_staging_,
                            size_t(kArenaPasses * kBatchBlockStride) * sizeof(BatchInfoRow));
                batch_info_buffer_->Unmap();
            }

        }
    }

    uint32_t GPUCullSystem::GetBatchCounterByteOffset(uint32_t pass_index, uint32_t batch_index) const
    {
        const uint32_t slot = context_ ? (context_->GetFrameIndex() % HGL_FRAME_SLOT_TOTAL) : 0u;
        const VkDeviceSize slot_bytes = VkDeviceSize(kArenaPasses * kBatchBlockStride) * sizeof(uint32_t);

        return uint32_t(VkDeviceSize(slot) * slot_bytes
                      + VkDeviceSize(BatchBlockBase(pass_index) + batch_index) * sizeof(uint32_t));
    }

    uint32_t GPUCullSystem::GetOverflowCounterByteOffset(uint32_t pass_index) const
    {
        // 块内溢出槽 = 块内第 `batch_count` 行（与 GLSL 口径一致）
        // 与着色器同口径：GLSL 溢出槽 = `block_base + pc.batch_count`，而 pc 下发的是**主帧**
        // 那份批数 ⇒ 读侧也用主帧那份（pass 1 的该槽仅为信息量，不作判据）。
        return GetBatchCounterByteOffset(pass_index, batch_count_[0]);
    }

    uint32_t *GPUCullSystem::MapCurrentBatchCounters(uint32_t pass_index)
    {
        if (!batch_counter_buffer_ || !context_)
            return nullptr;

        // 契约（§9 M6）：Map 之前必须 `SyncForParityReadback()` 把队列排到**本帧** dispatch 完成
        // ——Host Coherent ≠ 已执行（异步派发后直接 Map 读到的是上一已定帧的残影：对拍会"全绿"
        // 而数据是旧的）。违反 = 台架接线错 ⇒ 响亮一次 + **不给数据**（返回 null 让调用方跳过）。
        if (counters_sync_frame_ != context_->GetFrameIndex())
        {
            if (!reported_unsynced_map_)
            {
                reported_unsynced_map_ = true;
                GLogError(u8"[GPUCullSystem] MapCurrentBatchCounters 未先 SyncForParityReadback"
                          u8"（本帧=%u / 已同步帧=%u）⇒ 读数是残影，拒绝返回（契约 §9 M6）",
                          context_->GetFrameIndex(), counters_sync_frame_);
            }
            return nullptr;
        }

        const uint32_t slot = context_->GetFrameIndex() % HGL_FRAME_SLOT_TOTAL;
        const VkDeviceSize slot_bytes = VkDeviceSize(kArenaPasses * kBatchBlockStride) * sizeof(uint32_t);
        const VkDeviceSize block_off  = VkDeviceSize(BatchBlockBase(pass_index)) * sizeof(uint32_t);

        // 返回**该 pass 块的起点**：调用方按 `counters[b]`（b < batch_count）+ `counters[batch_count]`
        // （块内溢出槽）解读，与单块时代口径一致。
        batch_counter_cpu_ = static_cast<uint32_t *>(
            batch_counter_buffer_->Map(VkDeviceSize(slot) * slot_bytes + block_off, slot_bytes - block_off));

        return batch_counter_cpu_;
    }

    void GPUCullSystem::UnmapCurrentBatchCounters()
    {
        if (batch_counter_cpu_ && batch_counter_buffer_)
        {
            batch_counter_buffer_->Unmap();
            batch_counter_cpu_ = nullptr;
        }
    }

    bool GPUCullSystem::SyncForParityReadback()
    {
        // 对拍台架同步点（T3g）：Map 读回**之前**必须等本帧 dispatch 完成——
        // Host Coherent ≠ 已执行（FIFO 只保证序，不保证到）。台架每 60 帧采样一次
        // ⇒ 这个 fence 成本只在采样帧付出；生产路径（Update / guard）绝不调它。
        if (!cull_queue_)
            return false;

        if (!cull_queue_->WaitLastSubmitFence())
            return false;

        // 契约（§9 M6）：记下"本帧已同步"，供 `MapCurrentBatchCounters` 校验
        // （台架的 Sync → Map 在同一采样帧内成套执行；跨帧复用即残影）。
        counters_sync_frame_ = context_ ? context_->GetFrameIndex() : 0u;
        return true;
    }

    bool GPUCullSystem::CreateDispatchResources()
    {
        if (cull_queue_)
            return true;

        // 队列一份：fence 环深度 = 帧槽数；每个命令缓冲单独记住其提交 fence 槽，
        // 重录前等待该槽，而不是假设固定帧数后 GPU 必定完成。
        cull_queue_ = device_->CreateQueue("GPUCullSystem:Queue",
                                           uint32_t(HGL_FRAME_SLOT_TOTAL), false);
        if (!cull_queue_)
        {
            GLogError(u8"[GPUCullSystem] CreateDispatchResources: CreateQueue failed");
            return false;
        }

        // 命令缓冲环：一次性分配 HGL_FRAME_SLOT_TOTAL 个，按帧槽取用；Begin 前的
        // fence wait 保证它不再处于 pending 状态，graphics pool 允许单独 reset。
        for (uint32_t i = 0; i < HGL_FRAME_SLOT_TOTAL; ++i)
        {
            const AnsiString name = AnsiString("GPUCullSystem.Cmd[") + AnsiString::numberOf(i) + "]";
            cull_cmds_[i] = device_->CreateComputeCommandBuffer(name);
            if (!cull_cmds_[i])
            {
                GLogError(u8"[GPUCullSystem] CreateDispatchResources: cmd ring[%u] failed", i);
                return false;
            }
        }

        GLogInfo(u8"[GPUCullSystem] Dispatch resources created: queue + %u-slot cmd ring",
                 uint32_t(HGL_FRAME_SLOT_TOTAL));
        return true;
    }

    void GPUCullSystem::Update(float delta_time)
    {
        // 延迟初始化：第一次 Update 时执行
        if (!EnsureInitialized())
            return;

        if (!context_ || !device_ || !compute_pipeline_) return;

        // 获取实体表（返回引用）
        const auto& entity_table = context_->GetEntityGPUTable();
        uint32_t entity_count = entity_table.kCapacity;
        if (entity_count == 0) return;

        // 获取世界地址和全局地址的设备地址
        uint32_t frame_slot = context_->GetFrameIndex();
        uint64_t world_addr = context_->GetWorldAddressesAddress(frame_slot);
        if (world_addr == 0) return;

        // 全局地址表从 GlobalSSBOBufferRegistry 获取
        auto* gc = context_->GetGraphicsContext();
        if (!gc) return;
        auto* global_registry = gc->GetGlobalSSBOBufferRegistry();
        if (!global_registry) return;
        uint64_t global_addr = global_registry->GetGlobalAddressesAddress(frame_slot);
        if (global_addr == 0) return;

        // gpu_dispatch = 派发录制的 CPU 侧成本（T3g 起 = record+submit，不再含 fence 等待与
        // Map 读回——那两项就是旧数据 1.1ms 的元凶）。
        // 附加诊断 tracked_objects：泄漏判据（T3g）—— 稳态派发路径不再产生任何 Vk 对象 ⇒
        // 跟踪表计数应在启动期建完之后**恒定**；随帧上涨 = 又有每帧 new 混进来。
        static PerfTimer s_dispatch_perf("gpu_dispatch");
        const uint64_t t_dispatch0 = s_dispatch_perf.on ? GetUptimeUs() : 0;

        // 命令缓冲按帧槽取用（T3g：环一次建好，这里只重录——旧版每帧 Create 是泄漏）
        const uint32_t batch_slot = frame_slot % HGL_FRAME_SLOT_TOTAL;
        graph::ComputeCmdBuffer *cmd = cull_cmds_[batch_slot];
        if (!cmd) return;

        // Frame-slot reuse must wait for the submission that last used this exact cmd.
        // The queue fence ring can drift from frame slots when dispatches are skipped.
        if(cull_cmd_submitted_[batch_slot]
           &&!cull_queue_->WaitFenceAt(cull_cmd_fence_slots_[batch_slot]))
        {
            GLogError(u8"[GPUCullSystem] refusing to re-record pending command buffer frame=%u slot=%u cmd=%p fence_slot=%u",
                      frame_slot,batch_slot,(void*)(VkCommandBuffer)*cmd,
                      cull_cmd_fence_slots_[batch_slot]);
            return;
        }

        // 准备 push constants
        CullPushConstants pc{};
        pc.addr_world_addresses = world_addr;
        pc.addr_global_addresses = global_addr;
        pc.visible_indices_addr = visible_indices_addr_;
        pc.indirect_commands_addr = indirect_commands_addr_;
        pc.l2w_out_addr = l2w_out_addr_;
        pc.cmds_out_addr = cmds_out_addr_;
        pc.mtl_out_addr = mtl_out_addr_;
        pc.draw_count_addr = draw_count_addr_;
        pc.entity_count = entity_count;
        pc.max_visible_count = kMaxVisibleEntities;
        pc.camera_row = context_->GetActiveCameraRow();   // 与材质侧 pc_root.camera_row 同式
        // 形态 B：gather 开关恒等于 opt-in（表内容由 CPU guard 决定批是否消费；
        // 未接管帧写出的 gather 表 = 无人读的富余写，代价一次 gather 写）。

        // T3i：批标签 / 分区 / 计数（**当前帧槽**的份；与实体表同槽口径）
        pc.batch_tag_addr     = batch_tag_addr_ + VkDeviceSize(batch_slot)
                                * VkDeviceSize(kArenaPasses * kMaxVisibleEntities) * sizeof(uint32_t);
        pc.batch_info_addr    = batch_info_addr_ + VkDeviceSize(batch_slot)
                                * VkDeviceSize(kArenaPasses * kBatchBlockStride) * sizeof(BatchInfoRow);
        pc.batch_counter_addr = batch_counter_addr_ + VkDeviceSize(batch_slot)
                                * VkDeviceSize(kArenaPasses * kBatchBlockStride) * sizeof(uint32_t);
        pc.batch_count        = batch_count_[0];
        pc.batch_block_stride = kBatchBlockStride;
        pc.batch_count_shadow = batch_count_[1];

        // 诊断接线（`ECS_TAKEOVER_TRACE=1`）：派发当帧用的槽 —— 与接管侧（绘制读的槽）对照，
        // 定位"派发写的槽 ≠ 绘制读的槽"（帧内切 RT 时槽会变，而派发每帧只跑一次）。
        static const bool s_dispatch_trace = (getenv("ECS_TAKEOVER_TRACE") != nullptr);
        if (s_dispatch_trace)
            GLogInfo("[T3i][TRACE] dispatch frame_slot=%u(槽=%u) 批=[main:%u, shadow:%u] 相机行=%u",
                     context_->GetFrameIndex(), batch_slot, batch_count_[0], batch_count_[1], context_->GetActiveCameraRow());

        if(!cmd->Begin())
        {
            GLogError(u8"[GPUCullSystem] command buffer begin failed frame=%u slot=%u cmd=%p",
                      frame_slot,batch_slot,(void*)(VkCommandBuffer)*cmd);
            return;
        }

        // ── 计数 GPU 侧清零（T3g：旧版 CPU `Write(&zero)` 是同步上传，去掉）──
        // ⚠ 防坑 1：FillBuffer 属 TRANSFER 阶段（`vkCmdFillBuffer` size=4/offset=0
        // 满足 4B 对齐规范）；同一 cmd 内紧随其后的 compute 原子自增是 SHADER 读写
        // ——**批内无隐式序**（跨批才有 submission-order 保证，Vulkan 同步章）⇒
        // 必须显式 TRANSFER→COMPUTE barrier，否则 RAW 冒险 = 计数可能被清零覆盖。
        cmd->FillBuffer(draw_count_buffer_->GetBuffer(), 0, sizeof(uint32_t), 0);
        cmd->BufferMemoryBarrier2(draw_count_buffer_->GetBuffer(),
                                  VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                                  VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                  VK_ACCESS_2_TRANSFER_WRITE_BIT,
                                  VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT);

        // T3i：每批计数（含 overflow 末位槽）同 cmd 内清零——同一 FillBuffer 纪律
        // （TRANSFER→COMPUTE barrier，批内无隐式序）。只清**本帧槽**那一份。
        // ⚠ T4c2 起按 pass **两块**：偏移与长度都必须用整槽尺寸，否则只清块 0
        //   ⇒ 块 1（阴影）计数逐帧累加 ⇒ 阴影超画（实测：ATS 契约破 + Cascade 挂）。
        cmd->FillBuffer(batch_counter_buffer_->GetBuffer(),
                        VkDeviceSize(batch_slot) * VkDeviceSize(kArenaPasses * kBatchBlockStride) * sizeof(uint32_t),
                        VkDeviceSize(kArenaPasses * kBatchBlockStride) * sizeof(uint32_t), 0);
        cmd->BufferMemoryBarrier2(batch_counter_buffer_->GetBuffer(),
                                  VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                                  VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                  VK_ACCESS_2_TRANSFER_WRITE_BIT,
                                  VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT);

        cmd->BindPipeline(compute_pipeline_);
        cmd->PushConstants(compute_pipeline_->GetPipelineLayout(), &pc, sizeof(pc));

        uint32_t group_count = (entity_count + kWorkGroupSize - 1) / kWorkGroupSize;
        cmd->Dispatch(group_count, 1, 1);

        // 批内保守屏障：compute 写 → indirect 消费的可见性。
        // ⚠ 口径纠正：跨 cmd **不能**只靠"同队列 submission order"——那只保证**执行次序**，
        //   **不建立内存可见性**（Vulkan 同步章：即使同队列、即使提交序在前，内存依赖也须由
        //   barrier/event/semaphore 建立）。这里能覆盖跨 cmd 的 consumer，靠的是
        //   **内存 barrier 的作用域按 submission order 向后延伸**（dst 阶段点名了
        //   `DRAW_INDIRECT`）——**不是**靠"提交序"本身。改这条时别把 dst 阶段删掉。
        VkMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
        vkCmdPipelineBarrier(*cmd,
                            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                            VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
                            0,
                            1, &barrier,
                            0, nullptr,
                            0, nullptr);

        if(!cmd->End())
        {
            GLogError(u8"[GPUCullSystem] command buffer end failed frame=%u slot=%u cmd=%p",
                      frame_slot,batch_slot,(void*)(VkCommandBuffer)*cmd);
            return;
        }

        // 提交即返回（T3g：删 WaitLastSubmitFence——旧版每帧同步等待 = 流水线自杀；
        // 生产路径不需要 CPU 侧知道结果，接管 guard 用 dispatch_count_ 纯 CPU 判据）
        const uint32_t submit_fence_slot=cull_queue_->GetCurrentFenceIndex();
        if(!cull_queue_->Submit(cmd, nullptr, 0, nullptr, 0))
        {
            GLogError(u8"[GPUCullSystem] command buffer submit failed frame=%u slot=%u cmd=%p dispatch_count=%llu",
                      frame_slot,batch_slot,(void*)(VkCommandBuffer)*cmd,
                      static_cast<unsigned long long>(dispatch_count_));
            return;
        }
        cull_cmd_fence_slots_[batch_slot]=submit_fence_slot;
        cull_cmd_submitted_[batch_slot]=true;
        ++dispatch_count_;

        if (s_dispatch_perf.on)
            s_dispatch_perf.Add(GetUptimeUs() - t_dispatch0, "tracked_objects",
                                device_ ? (size_t)device_->GetTrackedObjectCount() : 0);

        static const bool s_cull_ssbo_dump = (getenv("CULL_SSBO_DUMP") != nullptr);
        if (s_cull_ssbo_dump)
        {
            if(cull_queue_->WaitLastSubmitFence())
            {
                // §3-④：fence 已等 ⇒ 本帧可安全读回（供 GetGPUVisibleCount 的自守卫判定）
                readback_ready_frame_ = context_ ? context_->GetFrameIndex() : 0xFFFFFFFFu;
                DumpCullSSBOs("AFTER_DISPATCH");
            }
            else
                GLogError(u8"[GPUCullSystem] SSBO dump skipped: dispatch fence wait failed");
        }
    }

    bool GPUCullSystem::ApplyGPUCullOverrideHook(MaterialBatch *batch, uint32_t batch_index,
                                                 uint32_t pass_index)
    {
        // 调用前提（guard 已在 `PrimitiveBatchPipeline::TryApplyGPUCullOverride` 核完）：
        // opt-in 开 / gather 输出与批行表**同容量契约** / 全 mesh / …
        // 本函数只做"指向 + 置标志"，不再重复校验 ⇒ 校验与接线分离，口径唯一。
        // T3i：逐批调用；本批在共享竞技场里的切片从**本帧分区表**取（与标签写值同源）。
        if (!batch || !indirect_commands_buffer_ || !l2w_out_buffer_
         || !cmds_out_buffer_ || !mtl_out_buffer_ || !batch_counter_buffer_)
            return false;

        if (batch_index >= kMaxBatches || batch_count_[pass_index] == 0)
            return false;

        const BatchInfoRow &info = batch_info_staging_[BatchBlockBase(pass_index) + batch_index];

        // 切片容量必须容得下本批全部 item（写行最坏情况 = 全可见）：不等 ⇒ 口径冲突
        if (batch->items.size() > info.capacity)
            return false;

        // 2026-10-14（批 D）：CPU 侧 per-batch 表构建整族已删 ⇒ 交接帧**没有自有 buffer 要释放**，
        // 原先的队列排空（vkQueueWaitIdle 全排空，为释放自有 ICB/行表而付）与四个
        // `own_*` 置 false 一并删除；下面的指针一律指向剔除输出（批只持**非拥有**指针）。
        batch->gpu_driven_override            = true;

        // T3i / T4c3c：切片信息（绘制侧的 maxDrawCount / 地址偏移全从这里取）——
        // 写进**本 pass 那一份**，不覆盖另一 pass（批对象跨 pass 共享）。
        {
            MaterialBatch::GpuSlice &slice = batch->gpu_slice[pass_index];
            slice.row_base            = info.row_base;
            slice.capacity            = info.capacity;
            slice.count_buffer_offset = GetBatchCounterByteOffset(pass_index, batch_index);
        }

        batch->icb_mesh_tasks                 = indirect_commands_buffer_;
        // 计数 = **本批**的 GPU 原子计数（末位 overflow 槽不在此列；maxDrawCount 取
        // 切片容量，实际条数由该计数给出 ⇒ GPU-Driven 动态计数口径不变）
        batch->icb_count_buffer               = batch_counter_buffer_;
        // 计数偏移随切片按 pass 存（见上）⇒ 这里不再单写
        batch->mesh_draw_params_buffer        = cmds_out_buffer_;
        batch->l2w_index_buffer               = l2w_out_buffer_;
        batch->material_data_index_rows_buffer = mtl_out_buffer_;
        // 容量一律从 `gpu_slice[pass].capacity` 取（上面的切片信息）⇒ 不再单抄一份。

        return true;
    }

    uint32_t GPUCullSystem::GetGPUVisibleCount() const
    {
        if (!draw_count_buffer_) return 0;

        // §3-④（2026-10-11）：前置条件自守卫 —— 本帧必须已"等过 fence"（SSBO dump 路径）
        // 或已 `SyncForParityReadback()`（对拍台架路径）。都不满足 ⇒ 读到的是上一已定帧的残影
        // （Host Coherent ≠ 已执行）⇒ **拒绝返回**（一次性报错 + 返回 0），不把残影当数据。
        {
            const uint32_t frame = context_ ? context_->GetFrameIndex() : 0xFFFFFFFFu;

            if (counters_sync_frame_ != frame && readback_ready_frame_ != frame)
            {
                if (!readback_guard_warned_)
                {
                    readback_guard_warned_ = true;
                    GLogError(u8"[GPUCullSystem] GetGPUVisibleCount 前置条件未满足：本帧(%u)既未"
                              u8"WaitLastSubmitFence（已记账帧=%u）也未 SyncForParityReadback"
                              u8"（已同步帧=%u）⇒ 拒绝返回残影（契约 §9 M6 / §3-④）",
                              frame, readback_ready_frame_, counters_sync_frame_);
                }

                return 0;
            }
        }

        uint32_t visible_count = 0;
        void* mapped = draw_count_buffer_->Map(0, sizeof(uint32_t));
        if (mapped)
        {
            visible_count = *(uint32_t*)mapped;
            draw_count_buffer_->Unmap();
        }
        return visible_count;
    }

    void GPUCullSystem::DumpCullSSBOs(const char *stage_tag)
    {
        if (!context_ || !device_)
            return;

        const uint32_t current_frame = context_->GetFrameIndex();
        const uint32_t current_slot  = current_frame % HGL_FRAME_SLOT_TOTAL;

        GLogInfo(u8"========== [SSBO DUMP: %s] frame=%u (slot=%u) ==========",
                 stage_tag ? stage_tag : "INFO", current_frame, current_slot);

        // 1. 全局幸存数 draw_count_buffer_
        const uint32_t global_survivors = GetGPUVisibleCount();
        GLogInfo(u8"  [DrawCount] 全局幸存数: %u", global_survivors);

        // 2. 批计数缓冲 batch_counter_buffer_
        if (batch_counter_buffer_)
        {
            const VkDeviceSize slot_bytes = VkDeviceSize(kArenaPasses * kBatchBlockStride) * sizeof(uint32_t);
            const VkDeviceSize total_counter_bytes = slot_bytes * HGL_FRAME_SLOT_TOTAL;
            ValueArray<uint32_t> counters;
            counters.Resize(static_cast<int>(total_counter_bytes / sizeof(uint32_t)));

            if (graph::ReadbackBuffer(batch_counter_buffer_, 0, total_counter_bytes, counters.GetData()))
            {
                for (uint32_t s = 0; s < HGL_FRAME_SLOT_TOTAL; ++s)
                {
                    AnsiString main_info = "Main: [";
                    AnsiString shadow_info = "Shadow: [";
                    const uint32_t slot_u32_base = s * (kArenaPasses * kBatchBlockStride);

                    // 主帧 pass
                    for (uint32_t b = 0; b < batch_count_[0] && b < 10; ++b)
                    {
                        main_info += "b" + AnsiString::numberOf(b) + "=" + AnsiString::numberOf(counters[slot_u32_base + b]) + " ";
                    }
                    const uint32_t main_ovf = counters[slot_u32_base + batch_count_[0]];
                    main_info += "ovf=" + AnsiString::numberOf(main_ovf) + "]";

                    // 阴影 pass
                    const uint32_t shadow_u32_base = slot_u32_base + kBatchBlockStride;
                    for (uint32_t b = 0; b < batch_count_[1] && b < 10; ++b)
                    {
                        shadow_info += "b" + AnsiString::numberOf(b) + "=" + AnsiString::numberOf(counters[shadow_u32_base + b]) + " ";
                    }
                    const uint32_t shadow_ovf = counters[shadow_u32_base + batch_count_[1]];
                    shadow_info += "ovf=" + AnsiString::numberOf(shadow_ovf) + "]";

                    const char *slot_mark = (s == current_slot) ? " <= CURRENT" : "";
                    GLogInfo(u8"  [BatchCounters] Slot %u (offset=%u): %s %s%s",
                             s, static_cast<uint32_t>(s * slot_bytes),
                             main_info.c_str(), shadow_info.c_str(), slot_mark);
                }
            }
        }

        // 3. 当前槽的批信息 batch_info_buffer_
        if (batch_info_buffer_)
        {
            const VkDeviceSize slot_info_bytes = VkDeviceSize(kArenaPasses * kBatchBlockStride) * sizeof(BatchInfoRow);
            // 裸 vector：ValueArray<T> 实例化 Find() 隐式要求 operator==，
            // 而 BatchInfoRow 必须与 GLSL 逐字段同序同型（字段数受门对拍）⇒ 不加成员函数
            std::vector<BatchInfoRow> infos(size_t(kArenaPasses) * kBatchBlockStride);

            if (graph::ReadbackBuffer(batch_info_buffer_, VkDeviceSize(current_slot) * slot_info_bytes,
                                      slot_info_bytes, infos.data()))
            {
                for (uint32_t p = 0; p < kArenaPasses; ++p)
                {
                    const uint32_t blk_base = BatchBlockBase(p);
                    for (uint32_t b = 0; b < batch_count_[p]; ++b)
                    {
                        const auto &row = infos[blk_base + b];
                        GLogInfo(u8"  [BatchInfo] Slot %u Pass %u Batch %u: row_base=%u, capacity=%u, topology=%u, group_size=%u",
                                 current_slot, p, b, row.row_base, row.capacity, row.topology, row.group_size);
                    }
                }
            }
        }

        // 4. 当前槽的批标签 batch_tag_buffer_
        if (batch_tag_buffer_)
        {
            const VkDeviceSize slot_tag_bytes = VkDeviceSize(kArenaPasses * kMaxVisibleEntities) * sizeof(uint32_t);
            ValueArray<uint32_t> tags;
            tags.Resize(kArenaPasses * kMaxVisibleEntities);

            if (graph::ReadbackBuffer(batch_tag_buffer_, VkDeviceSize(current_slot) * slot_tag_bytes,
                                      slot_tag_bytes, tags.GetData()))
            {
                for (uint32_t p = 0; p < kArenaPasses; ++p)
                {
                    uint32_t valid_tags = 0;
                    uint32_t batch_histogram[16] = {};
                    const uint32_t blk_off = p * kMaxVisibleEntities;

                    for (uint32_t e = 0; e < kMaxVisibleEntities; ++e)
                    {
                        const uint32_t tag = tags[blk_off + e];
                        if (tag != kInvalidBatchTag)
                        {
                            ++valid_tags;
                            if (tag < 16)
                                ++batch_histogram[tag];
                        }
                    }

                    AnsiString hist_str;
                    for (uint32_t b = 0; b < 16 && b < batch_count_[p]; ++b)
                    {
                        hist_str += "b" + AnsiString::numberOf(b) + ":" + AnsiString::numberOf(batch_histogram[b]) + " ";
                    }

                    GLogInfo(u8"  [BatchTag] Slot %u Pass %u: 有效标签实体数=%u (%s)",
                             current_slot, p, valid_tags, hist_str.c_str());
                }
            }
        }

        // 5. 间接命令缓冲 indirect_commands_buffer_（前几个命令）
        if (indirect_commands_buffer_)
        {
            constexpr uint32_t kDumpCmds = 8;
            VkDrawMeshTasksIndirectCommandEXT cmds[kDumpCmds] = {};
            if (graph::ReadbackBuffer(device_, indirect_commands_buffer_->GetVkBuffer(), 0,
                                      sizeof(cmds), cmds))
            {
                for (uint32_t i = 0; i < kDumpCmds; ++i)
                {
                    if (cmds[i].groupCountX != 0 || cmds[i].groupCountY != 0)
                    {
                        GLogInfo(u8"  [IndirectCmd] row %u: groupCount=(%u, %u, %u)",
                                 i, cmds[i].groupCountX, cmds[i].groupCountY, cmds[i].groupCountZ);
                    }
                }
            }
        }

        // 6. 相机行与视锥平面
        auto *storage = context_->GetCameraInfoStorage();
        if (storage && storage->IsReady())
        {
            const uint32_t active_cam_row = context_->GetActiveCameraRow();
            const graph::CameraInfo *ci = storage->GetCameraRow(0, current_slot);
            if (ci)
            {
                GLogInfo(u8"  [CameraInfo] Slot %u (row=%u): pos=(%.2f, %.2f, %.2f) z=(%.2f, %.2f) revZ=%u",
                         current_slot, active_cam_row, ci->pos.x, ci->pos.y, ci->pos.z,
                         ci->znear, ci->zfar, ci->use_reversed_z);
                for (int p = 0; p < 6; ++p)
                {
                    GLogInfo(u8"    plane[%d] = (%.3f, %.3f, %.3f, w=%.3f)",
                             p, ci->frustum_planes[p].x, ci->frustum_planes[p].y,
                             ci->frustum_planes[p].z, ci->frustum_planes[p].w);
                }
            }
            else
            {
                GLogWarning(u8"  [CameraInfo] Slot %u: GetCameraRow 返回 nullptr!", current_slot);
            }
        }

        // 7. 实体行与 4-ID
        const auto& entity_table = context_->GetEntityGPUTable();
        const auto& rid_table = context_->GetEntityRenderIDTable();
        for (uint32_t e = 0; e < 3; ++e)
        {
            const auto *row = entity_table.Row(e);
            const auto *desc = rid_table.Row(0, e);
            if (row && desc)
            {
                GLogInfo(u8"  [EntityDump] Slot %u Entity %u: flags=0x%08X, transform_id=%u, geom_id=%u",
                         current_slot, e, row->flags, row->row[1], desc->geometry_id);
            }
        }

        GLogInfo(u8"============================================================");
    }
}
