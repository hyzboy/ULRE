#include <hgl/ecs/support/GPUCullSystem.h>
#include <hgl/ecs/core/MaterialBatch.h>
#include <hgl/ecs/core/Entity.h>
#include <hgl/ecs/core/EntityGPU.h>
#include <hgl/ecs/core/RenderItem.h>
#include <hgl/ecs/support/EntityGPUTable.h>
#include <hgl/ecs/support/primitive/PrimitiveCullSystem.h>
#include <hgl/ecs/components/GeometryData.h>
#include <vector>
#include <hgl/graph/module/GlobalSSBOBufferRegistry.h>
#include <hgl/mtl/MeshShaderLimits.h>
#include <hgl/graph/module/ShaderProgramManager.h>
#include <hgl/graph/module/BufferManager.h>
#include <hgl/graph/core/GraphicsContext.h>
#include <hgl/vk/VKCommandBuffer.h>
#include <hgl/vk/VKDevice.h>
#include <hgl/vk/buffer/BufferPolicy.h>
#include <hgl/log/Log.h>
#include <fstream>
#include <sstream>

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
            uint32_t entity_count;
            uint32_t max_visible_count;
            uint32_t camera_row;          // ECSContext::GetActiveCameraRow()
            uint32_t mesh_group_size;     // kMeshVertexPassthroughMaxVertices（CPU 单源下发）
            uint32_t enable_gather;       // 0 = 只出计数；非 0 = 写 gather 四表（形态 B opt-in）
        };

        static_assert(sizeof(CullPushConstants) == 88, "CullPushConstants layout mismatch");

        AnsiString LoadShaderSource(const char* path)
        {
            std::ifstream file(path);
            if (!file.is_open())
            {
                GLogError(u8"[GPUCullSystem] Failed to open shader: %s", path);
                return AnsiString();
            }
            std::stringstream ss;
            ss << file.rdbuf();
            return AnsiString(ss.str().c_str());
        }
    }

    GPUCullSystem::GPUCullSystem()
        : System("GPUCullSystem")
    {
        SetExecutionPhase(ExecutionPhase::RenderCollect);

        // 回灌 `isVisible` 必须晚于 CPU 剔除（本系统是其后的**最终判据**，形态 A/T3f-1）、
        // 早于 `PrimitiveBuildSystem`（不同 phase：RenderCollect < RenderBatch ⇒ 天然满足）。
        AddDependency<PrimitiveCullSystem>();
    }

    GPUCullSystem::~GPUCullSystem()
    {
        if (visible_indices_buffer_) delete visible_indices_buffer_;
        if (indirect_commands_buffer_) delete indirect_commands_buffer_;
        if (draw_count_buffer_) delete draw_count_buffer_;
        if (l2w_out_buffer_) delete l2w_out_buffer_;
        if (cmds_out_buffer_) delete cmds_out_buffer_;
        if (mtl_out_buffer_) delete mtl_out_buffer_;
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

        // 加载着色器源码
        AnsiString shader_source = LoadShaderSource("D:/ULRE/ShaderLibrary/ecs/entity_gpu_cull.glsl");
        if (shader_source.IsEmpty()) 
        {
            GLogError(u8"[GPUCullSystem] CreateComputePipeline: Failed to load shader source");
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
        indirect_commands_buffer_ = device_->CreateIndirectMeshTaskBuffer(
            kMaxVisibleEntities,
            graph::BufferAllocPolicy::Auto,
            "GPUCullSystem.IndirectCommands",
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
        );
        if (!indirect_commands_buffer_) return false;
        indirect_commands_addr_ = device_->GetBufferDeviceAddressAligned16(indirect_commands_buffer_->GetBuffer());

        // 可见计数（uint32_t）
        draw_count_buffer_ = device_->CreateBuffer(
            "GPUCullSystem.DrawCount",
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            sizeof(uint32_t),
            sizeof(uint32_t),
            nullptr,
            graph::BufferAllocPolicy::Auto,
            graph::SharingMode::Exclusive
        );
        if (!draw_count_buffer_) return false;
        draw_count_addr_ = device_->GetBufferDeviceAddressAligned16(draw_count_buffer_->GetBuffer());

        // ── 形态 B（T3f-2）gather 输出三表（容量 = kMaxVisibleEntities；icb 复用上面）──
        // 行距契约（与着色器镜像逐字段一致，§8.2d 的"读真源零复制"口径）：
        //   l2w_out  = uint32 × 4 B/行；cmds_out = MeshDrawCommand 8 B/行；
        //   mtl_out  = MaterialInstanceAddresses 16 B/行。
        // usage：STORAGE（CS 写）+ INDIRECT（icb/count 已有；三张行表被 mesh shader
        // 经 BDA 解引用 ⇒ SHADER_DEVICE_ADDRESS 必带，与 §7.6 设备地址纪律一致）。
        const VkDeviceSize l2w_bytes  = VkDeviceSize(kMaxVisibleEntities) * sizeof(uint32_t);
        const VkDeviceSize cmds_bytes = VkDeviceSize(kMaxVisibleEntities) * 8;    // MeshDrawCommand
        const VkDeviceSize mtl_bytes  = VkDeviceSize(kMaxVisibleEntities) * 16;   // MaterialInstanceAddresses

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

        GLogInfo(u8"[GPUCullSystem] Output buffers created: visible_indices=%p, indirect_commands=%p, draw_count=%p, gather_l2w=%p cmds=%p mtl=%p",
                 (void*)visible_indices_addr_, (void*)indirect_commands_addr_, (void*)draw_count_addr_,
                 (void*)l2w_out_addr_, (void*)cmds_out_addr_, (void*)mtl_out_addr_);
        return true;
    }

    void GPUCullSystem::SetEnabled(bool enabled)
    {
        if (enabled_ == enabled)
            return;

        enabled_ = enabled;
        GLogInfo(u8"[GPUCullSystem] GPU cull dispatch %s (opt-in)",
                 enabled_ ? u8"ENABLED" : u8"disabled");
    }

    void GPUCullSystem::SetDrawOverrideEnabled(bool enabled)
    {
        if (override_draw_ == enabled)
            return;

        override_draw_ = enabled;
        GLogInfo(u8"[GPUCullSystem] batch draw override %s (opt-in)",
                 override_draw_ ? u8"ENABLED" : u8"disabled");
    }

    void GPUCullSystem::SetApplyVisibleSetEnabled(bool enabled)
    {
        if (apply_visible_set_ == enabled)
            return;

        apply_visible_set_ = enabled;
        GLogInfo(u8"[GPUCullSystem] isVisible write-back (form A) %s (opt-in)",
                 apply_visible_set_ ? u8"ENABLED" : u8"disabled");
    }

    void GPUCullSystem::ApplyVisibleSetToRenderItems(uint32_t visible_count)
    {
        if (!context_)
            return;

        auto& cache = context_->GetRenderFrameCache();
        if (cache.renderItems.empty())
            return;

        // 读回 GPU 幸存实体 index（紧凑表，长度 = visible_count ≤ kMaxVisibleEntities）
        static std::vector<uint32_t> s_survivors;
        s_survivors.clear();
        if (visible_count > 0 && visible_indices_buffer_)
        {
            const VkDeviceSize bytes = sizeof(uint32_t) * visible_count;
            void* mapped = visible_indices_buffer_->Map(0, bytes);
            if (mapped)
            {
                const uint32_t* src = static_cast<const uint32_t*>(mapped);
                s_survivors.assign(src, src + visible_count);
                visible_indices_buffer_->Unmap();
            }
        }

        // 首帧护栏：实体行表的脏行上传与 dispatch 同帧发生，表在**首个 dispatch**
        // 时可能还没写齐 ⇒ GPU 会报 `survivors=0`，直接回灌 = 一帧黑屏（实测第一帧
        // off=100）。判据不可信时**本帧不动 isVisible**（保留 CPU 剔除结果）：
        // CPU 与 GPU 同式 ⇒ 最多是"这一帧按 CPU 判据画"，不会画错也不会画空。
        if (visible_count == 0)
        {
            bool any_entity_item = false;
            for (auto& itemPtr : cache.renderItems)
            {
                if (itemPtr && itemPtr->GetEntity())
                {
                    any_entity_item = true;
                    break;
                }
            }
            if (any_entity_item)
            {
                GLogDebug(u8"[GPUCullSystem] write-back skipped: survivors=0 但存在实体项"
                          u8"（判据不可信帧，本帧保留 CPU isVisible）");
                return;
            }
        }

        // 幸存集 → 覆盖位图（按 EntityID.index；越界 index 忽略）
        std::vector<uint8_t> keep(EntityGPUTable::kCapacity, 0);
        for (uint32_t idx : s_survivors)
        {
            if (idx < keep.size())
                keep[idx] = 1;
        }

        // 回灌：item 的 owner 实体在幸存集 ⇒ visible。无 owner 实体的 item（非实体驱动）
        // 不接管（保留 CPU 剔除结果）——本管线只负责实体行驱动的那部分。
        uint32_t applied_on = 0, applied_off = 0, skipped_no_entity = 0;
        for (auto& itemPtr : cache.renderItems)
        {
            RenderItem* item = itemPtr.get();
            if (!item)
                continue;

            Entity* owner = item->GetEntity();
            if (!owner)
            {
                ++skipped_no_entity;
                continue;
            }

            const uint32_t idx = owner->GetEntityID().index;
            const bool vis = (idx < keep.size()) && keep[idx] != 0;
            if (vis)
                ++applied_on;
            else
                ++applied_off;
            item->isVisible = vis;
        }

        GLogDebug(u8"[GPUCullSystem] write-back: survivors=%u on=%u off=%u no_entity=%u",
                  visible_count, applied_on, applied_off, skipped_no_entity);
    }

    void GPUCullSystem::Update(float delta_time)
    {
        // opt-in 门（默认关）：未显式启用就不 dispatch，避免默认全局接管把示例画空。
        if (!enabled_)
            return;

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

        // 重置可见计数
        uint32_t zero = 0;
        draw_count_buffer_->Write(&zero, sizeof(uint32_t));

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
        pc.mesh_group_size = graph::mtl::kMeshVertexPassthroughMaxVertices;  // 单源下发（不在 GLSL 硬编码）
        // 形态 B：gather 开关恒等于 opt-in（表内容由 CPU guard 决定批是否消费；
        // 未接管帧写出的 gather 表 = 无人读的富余写，代价一次 gather 写）。
        pc.enable_gather = override_draw_ ? 1u : 0u;

        // Dispatch compute shader
        auto* cmd = device_->CreateComputeCommandBuffer("GPUCullSystem.Cmd");
        if (!cmd) return;

        cmd->Begin();
        cmd->BindPipeline(compute_pipeline_);
        cmd->PushConstants(compute_pipeline_->GetPipelineLayout(), &pc, sizeof(pc));

        uint32_t group_count = (entity_count + kWorkGroupSize - 1) / kWorkGroupSize;
        cmd->Dispatch(group_count, 1, 1);

        // 内存屏障：compute 写入 -> indirect 读取
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

        // 提交到计算队列
        auto* compute_queue = device_->CreateQueue("GPUCullSystem.Queue");
        if (compute_queue)
        {
            compute_queue->Submit(cmd, nullptr, 0, nullptr, 0);
            compute_queue->WaitLastSubmitFence();
        }

        // 读取可见计数并记录日志
        const uint32_t visible_count = GetGPUVisibleCount();
        GLogDebug(u8"[GPUCullSystem] Dispatched: entities=%u, visible=%u", entity_count, visible_count);

        // 形态 A（T3f-1）：把幸存集回灌 isVisible，下游 batch/行表/ICB 全走既有 CPU 路径。
        if (apply_visible_set_)
            ApplyVisibleSetToRenderItems(visible_count);
    }

    bool GPUCullSystem::ApplyGPUCullOverrideHook(MaterialBatch *batch)
    {
        // 调用前提（guard 已在 `PrimitiveBatchPipeline::TryApplyGPUCullOverride` 核完）：
        // opt-in 开 / gather 输出与批行表**同容量契约** / 单批 / 全默认阴影 / 非 meshlet。
        // 本函数只做"指向 + 置标志"，不再重复校验 ⇒ 校验与接线分离，口径唯一。
        if (!batch || !indirect_commands_buffer_ || !draw_count_buffer_
         || !l2w_out_buffer_ || !cmds_out_buffer_ || !mtl_out_buffer_)
            return false;

        // icb 容量必须容得下最坏情况（全部可见）：批容量 > 表行数 ⇒ 越界 = 项目 bug，
        // 但宁可在这里再判一次（guard 已判 items 数；这里判**表**行数，口径不同源则拒）
        if (batch->items.size() > kMaxVisibleEntities)
            return false;

        // 交接帧（CPU→GPU）：批可能已持有上一帧 CPU 路径分配的**自有** buffer
        // （own_* 仍为 true 时 `MaterialBatch::Clear()` 保留指针）⇒ 覆盖前必须释放，
        // 否则每交接一次漏一套 ICB/行表（稳态接管帧 own=false、指针=GPU 表，无此问题）。
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

        batch->icb_mesh_tasks                 = indirect_commands_buffer_;
        batch->icb_count_buffer               = draw_count_buffer_;
        batch->icb_count_buffer_offset        = 0;
        batch->mesh_draw_params_buffer        = cmds_out_buffer_;
        batch->mesh_draw_params_capacity      = kMaxVisibleEntities;
        batch->l2w_index_buffer               = l2w_out_buffer_;
        batch->l2w_index_capacity             = kMaxVisibleEntities;
        batch->material_data_index_rows_buffer = mtl_out_buffer_;
        batch->material_data_index_rows_capacity = kMaxVisibleEntities;

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
