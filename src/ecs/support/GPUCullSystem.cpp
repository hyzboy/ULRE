#include <hgl/ecs/support/GPUCullSystem.h>
#include <hgl/ecs/core/Entity.h>
#include <hgl/ecs/core/EntityGPU.h>
#include <hgl/ecs/components/GeometryData.h>
#include <hgl/graph/module/GlobalSSBOBufferRegistry.h>
#include <hgl/graph/module/ShaderProgramManager.h>
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
            uint64_t draw_count_addr;
            uint32_t entity_count;
            uint32_t max_visible_count;
            uint32_t camera_row;          // ECSContext::GetActiveCameraRow()
            uint32_t mesh_group_count_x;  // 间接命令 X 组数（绘制接管=T3f 时按几何给）
        };

        static_assert(sizeof(CullPushConstants) == 56, "CullPushConstants layout mismatch");

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
    }

    GPUCullSystem::~GPUCullSystem()
    {
        if (visible_indices_buffer_) delete visible_indices_buffer_;
        if (indirect_commands_buffer_) delete indirect_commands_buffer_;
        if (draw_count_buffer_) delete draw_count_buffer_;
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

        GLogInfo(u8"[GPUCullSystem] Output buffers created: visible_indices=%p, indirect_commands=%p, draw_count=%p",
                 (void*)visible_indices_addr_, (void*)indirect_commands_addr_, (void*)draw_count_addr_);
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
        pc.draw_count_addr = draw_count_addr_;
        pc.entity_count = entity_count;
        pc.max_visible_count = kMaxVisibleEntities;
        pc.camera_row = context_->GetActiveCameraRow();   // 与材质侧 pc_root.camera_row 同式
        pc.mesh_group_count_x = 0;  // 绘制接管（T3f）前只出计数不出绘制

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
