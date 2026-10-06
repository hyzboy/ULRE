#include <hgl/ecs/support/GPUCullSystem.h>
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
#include <hgl/io/FileInputStream.h>
#include <hgl/type/Smart.h>
#include <hgl/log/Log.h>
#include <hgl/time/Time.h>

namespace hgl::ecs
{
    namespace
    {
        // PBR_PERF=1 计时代（临时插桩，与 PrimitiveBatchPipeline.cpp 同口径）：
        //   gpu_dispatch = 派发录制的 CPU 侧成本（T3g 起 = record+submit，不再含
        //                  fence 等待与 Map 读回——那两项就是旧数据 1.1ms 的元凶）
        // 计时用引擎时间源（CMCore hgl/time/Time.h 的单调 uptime，禁 std::chrono）。
        struct CullPerfTimer
        {
            const bool on;
            uint64_t   total_us = 0;
            uint32_t   frames = 0;

            CullPerfTimer() : on(getenv("PBR_PERF") != nullptr) {}

            void Add(const char *name, uint64_t us, graph::VulkanDevice *device)
            {
                if (!on)
                    return;

                total_us += us;
                ++frames;
                if (frames % 240 == 0)
                {
                    // 泄漏判据（T3g）：稳态派发路径不再产生任何 Vk 对象 ⇒ 跟踪表
                    // 计数应在启动期建完之后**恒定**；随帧上涨 = 又有每帧 new 混进来。
                    const size_t tracked =
                        device ? (size_t)device->GetTrackedObjectCount() : 0;
                    GLogInfo("[PBR-PERF] %s: frames=%u avg=%.1f us/frame tracked_objects=%zu",
                             name, frames, double(total_us) / frames, tracked);
                    total_us = 0;
                    frames = 0;
                }
            }
        };
    }
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
            uint32_t batch_count;         // T3i：本帧批数（0 = 无接管批；两 pass 同值）
            uint32_t batch_block_stride;  // T4c2：批行/计数每 pass 块的行数（= kBatchBlockStride）
        };

        // 11×u64(88) + 5×u32(20) = 108 ⇒ 对齐 8 ⇒ sizeof 112
        //（A6-d T2 撤 enable_gather 后曾为 104；T4c2 加 batch_block_stride ⇒ 112）。
        static_assert(sizeof(CullPushConstants) == 112, "CullPushConstants layout mismatch");
        static_assert(offsetof(CullPushConstants, batch_tag_addr) == 64, "T3i pc 段偏移");
        static_assert(offsetof(CullPushConstants, batch_count) == 100, "T3i pc 段偏移");
        static_assert(offsetof(CullPushConstants, batch_block_stride) == 104, "T4c2 pc 段偏移（吃 4B 填充）");

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

    GPUCullSystem::~GPUCullSystem()
    {
        // 派发资源显式释放（T3g 泄漏修复的另一半：与 CreateDispatchResources 一次
        // new 对应；fence/cmd 的最终归属由 DeviceQueue/DeviceBuffer 析构链处理）
        SAFE_CLEAR(cull_queue_);
        for (graph::ComputeCmdBuffer *&cmd : cull_cmds_)
            SAFE_CLEAR(cmd);

        if (visible_indices_buffer_) delete visible_indices_buffer_;
        if (indirect_commands_buffer_) delete indirect_commands_buffer_;
        if (draw_count_buffer_) delete draw_count_buffer_;
        if (l2w_out_buffer_) delete l2w_out_buffer_;
        if (cmds_out_buffer_) delete cmds_out_buffer_;
        if (mtl_out_buffer_) delete mtl_out_buffer_;
        if (batch_tag_buffer_) delete batch_tag_buffer_;
        if (batch_info_buffer_) delete batch_info_buffer_;
        if (batch_counter_buffer_) delete batch_counter_buffer_;
        // compute_pipeline_ 和 shader_program_ 由 MaterialManager 管理，不手动删除
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
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
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
        const VkDeviceSize tag_slot_bytes     = VkDeviceSize(kMaxVisibleEntities) * sizeof(uint32_t);
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
        const VkDeviceSize slot_bytes = VkDeviceSize(kMaxVisibleEntities) * sizeof(uint32_t);

        uint32_t *p = static_cast<uint32_t *>(
            batch_tag_buffer_->Map(VkDeviceSize(slot) * slot_bytes, slot_bytes));
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
            static bool s_reported_misaligned = false;
            if (!s_reported_misaligned && (row_base % 4u) != 0u)
            {
                s_reported_misaligned = true;
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

        batch_count_ = batch_count;

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

            // T4c-ini-2：记下"本槽的布局已写过"（供下一次轮到该槽时判定"派发已见过它"）
            slot_layout_written_[slot] = true;
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
        return GetBatchCounterByteOffset(pass_index, batch_count_);
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
            static bool s_reported_unsynced = false;
            if (!s_reported_unsynced)
            {
                s_reported_unsynced = true;
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

    void GPUCullSystem::DrainForHandover()
    {
        // CPU→GPU 交接沿：排空整条 graphics 队列（含上一帧主帧 cmd）。
        // cull_queue_ 包的就是 attr->graphics_queue（与主帧同一条，VKDeviceCreater
        // 单队列族）⇒ WaitQueue 覆盖主帧引用，这正是释放批自有 buffer 的前提。
        if (cull_queue_)
            cull_queue_->WaitQueue();
    }

    bool GPUCullSystem::CreateDispatchResources()
    {
        if (cull_queue_)
            return true;

        // 队列一份：fence 环深度 = 帧槽数（Submit 自动轮转；本帧只可能重录
        // ≥8 帧前用过的 cmd，那时其 fence 早已轮空）——照 SwapchainModule 样板。
        cull_queue_ = device_->CreateQueue("GPUCullSystem:Queue",
                                           uint32_t(HGL_FRAME_SLOT_TOTAL), false);
        if (!cull_queue_)
        {
            GLogError(u8"[GPUCullSystem] CreateDispatchResources: CreateQueue failed");
            return false;
        }

        // 命令缓冲环：一次性分配 HGL_FRAME_SLOT_TOTAL 个，按帧槽取用、每帧重录。
        // 复用合法性（防坑 2 查实）：引擎 graphics pool 带
        // VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT（VKDeviceCreater.cpp:146），
        // 非在飞 cmd 可直接 Begin()；同槽重录隔 8 帧 ≥ 在飞深度 ⇒ 必已执行完。
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

        static CullPerfTimer s_dispatch_perf;
        const uint64_t t_dispatch0 = s_dispatch_perf.on ? GetUptimeUs() : 0;

        // 命令缓冲按帧槽取用（T3g：环一次建好，这里只重录——旧版每帧 Create 是泄漏）
        graph::ComputeCmdBuffer *cmd = cull_cmds_[frame_slot % HGL_FRAME_SLOT_TOTAL];
        if (!cmd) return;

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
        const uint32_t batch_slot = frame_slot % HGL_FRAME_SLOT_TOTAL;
        pc.batch_tag_addr     = batch_tag_addr_ + VkDeviceSize(batch_slot)
                                * VkDeviceSize(kMaxVisibleEntities) * sizeof(uint32_t);
        pc.batch_info_addr    = batch_info_addr_ + VkDeviceSize(batch_slot)
                                * VkDeviceSize(kArenaPasses * kBatchBlockStride) * sizeof(BatchInfoRow);
        pc.batch_counter_addr = batch_counter_addr_ + VkDeviceSize(batch_slot)
                                * VkDeviceSize(kArenaPasses * kBatchBlockStride) * sizeof(uint32_t);
        pc.batch_count        = batch_count_;

        // 诊断接线（`ECS_TAKEOVER_TRACE=1`）：派发当帧用的槽 —— 与接管侧（绘制读的槽）对照，
        // 定位"派发写的槽 ≠ 绘制读的槽"（帧内切 RT 时槽会变，而派发每帧只跑一次）。
        static const bool s_dispatch_trace = (getenv("ECS_TAKEOVER_TRACE") != nullptr);
        if (s_dispatch_trace)
            GLogInfo("[T3i][TRACE] dispatch frame_slot=%u(槽=%u) 批=%u 相机行=%u",
                     context_->GetFrameIndex(), batch_slot, batch_count_, context_->GetActiveCameraRow());
        pc.batch_block_stride = kBatchBlockStride;

        cmd->Begin();

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

        // 批内保守屏障：compute 写 → 本 cmd 之后的 indirect 消费可见性
        // （跨 cmd 的 [本 cull 提交 → 主帧绘制提交] 序由**同队列 submission order**
        // 保证——CreateQueue 包的就是 attr->graphics_queue，与主帧同一条 VkQueue；
        // 本提交发生在主帧录制/提交之前 ⇒ 后一批全部可见前一批的写出）。
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

        cmd->End();

        // 提交即返回（T3g：删 WaitLastSubmitFence——旧版每帧同步等待 = 流水线自杀；
        // 生产路径不需要 CPU 侧知道结果，接管 guard 用 dispatch_count_ 纯 CPU 判据）
        cull_queue_->Submit(cmd, nullptr, 0, nullptr, 0);
        ++dispatch_count_;

        if (s_dispatch_perf.on)
            s_dispatch_perf.Add("gpu_dispatch", GetUptimeUs() - t_dispatch0, device_);
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

        if (batch_index >= kMaxBatches || batch_count_ == 0)
            return false;

        const BatchInfoRow &info = batch_info_staging_[BatchBlockBase(pass_index) + batch_index];

        // 切片容量必须容得下本批全部 item（写行最坏情况 = 全可见）：不等 ⇒ 口径冲突
        if (batch->items.size() > info.capacity)
            return false;

        // 交接帧（CPU→GPU）：批可能已持有上一帧 CPU 路径分配的**自有** buffer
        // （own_* 仍为 true 时 `MaterialBatch::Clear()` 保留指针）⇒ 覆盖前必须释放，
        // 否则每交接一次漏一套 ICB/行表（稳态接管帧 own=false、指针=GPU 表，无此问题）。
        //
        // ⚠ 释放前必须排空队列（T3g 实测踩 VUID-vkFreeMemory-memory-00677）：
        // 自有 ICB/行表可能仍被**上一帧主帧 cmd** 引用——自家 fence 只覆盖自家
        // 提交（同队列各批"按序开始、可乱序完成"，Vulkan 同步章），等自家提交
        // 不够 ⇒ vkQueueWaitIdle 全排空。只在这次交接**真有自有 buffer 要释放**
        // 时付出（稳态 own=false ⇒ 零成本；每个 CPU↔GPU 沿一次性）。
        const bool has_owned_to_release =
            (batch->own_icb_mesh_tasks && batch->icb_mesh_tasks)
         || (batch->own_mesh_draw_params && batch->mesh_draw_params_buffer)
         || (batch->own_l2w_index && batch->l2w_index_buffer)
         || (batch->own_material_data_rows && batch->material_data_index_rows_buffer);

        if (has_owned_to_release)
            DrainForHandover();

        // 释放口径与 ~MaterialBatch / ReleaseICB 逐字一致。
        if (batch->own_icb_mesh_tasks && batch->icb_mesh_tasks)
        {
            delete batch->icb_mesh_tasks;
            batch->icb_mesh_tasks = nullptr;
        }
        if (batch->own_mesh_draw_params && batch->mesh_draw_params_buffer)
        {
            if (batch->buffer_manager)
                batch->buffer_manager->Release(batch->mesh_draw_params_buffer);
            else
                delete batch->mesh_draw_params_buffer;
            batch->mesh_draw_params_buffer = nullptr;
        }
        if (batch->own_l2w_index && batch->l2w_index_buffer)
        {
            if (batch->buffer_manager)
                batch->buffer_manager->Release(batch->l2w_index_buffer);
            else
                delete batch->l2w_index_buffer;
            batch->l2w_index_buffer = nullptr;
        }
        if (batch->own_material_data_rows && batch->material_data_index_rows_buffer)
        {
            if (batch->buffer_manager)
                batch->buffer_manager->Release(batch->material_data_index_rows_buffer);
            else
                delete batch->material_data_index_rows_buffer;
            batch->material_data_index_rows_buffer = nullptr;
        }

        // 指向剔除输出（own_* 全 false ⇒ 批析构/复位绝不 delete 这些 buffer）。
        batch->gpu_driven_override            = true;
        batch->own_icb_mesh_tasks             = false;
        batch->own_mesh_draw_params           = false;
        batch->own_l2w_index                  = false;
        batch->own_material_data_rows         = false;

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
        batch->mesh_draw_params_capacity      = info.capacity;
        batch->l2w_index_buffer               = l2w_out_buffer_;
        batch->l2w_index_capacity             = info.capacity;
        batch->material_data_index_rows_buffer = mtl_out_buffer_;
        batch->material_data_index_rows_capacity = info.capacity;

        return true;
    }

    uint32_t GPUCullSystem::GetGPUVisibleCount() const
    {
        if (!draw_count_buffer_) return 0;

        uint32_t visible_count = 0;
        void* mapped = draw_count_buffer_->Map(0, sizeof(uint32_t));
        if (mapped)
        {
            visible_count = *(uint32_t*)mapped;
            draw_count_buffer_->Unmap();
        }
        return visible_count;
    }
}
