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
        // Push constants 结构（必须与 GLSL 侧一致）
        struct CullPushConstants
        {
            uint64_t addr_world_addresses;
            uint64_t addr_global_addresses;
            uint32_t entity_count;
            uint32_t max_visible_count;
            uint64_t visible_indices_addr;
            uint64_t indirect_commands_addr;
            uint32_t vertex_count_per_entity;
            uint32_t first_vertex_per_entity;
            uint64_t draw_count_addr;
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
        if (!context) return false;
        context_ = context;
        device_ = context_->GetGPUDevice();
        if (!device_) return false;

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

        GLogInfo(u8"[GPUCullSystem] Initialized: max_visible=%u, work_group=%u",
                 kMaxVisibleEntities, kWorkGroupSize);
        return true;
    }

    bool GPUCullSystem::CreateComputePipeline()
    {
        auto* gc = context_->GetGraphicsContext();
        if (!gc) return false;

        auto* shader_manager = gc->GetMaterialManager();
        if (!shader_manager) return false;

        // 加载着色器源码
        AnsiString shader_source = LoadShaderSource("D:/ULRE/ShaderLibrary/ecs/entity_gpu_cull.glsl");
        if (shader_source.IsEmpty()) return false;

        // 创建 compute pipeline
        compute_pipeline_ = shader_manager->CreateComputePipeline(
            "GPUCullSystem.Pipeline",
            shader_source,
            sizeof(CullPushConstants)
        );

        if (!compute_pipeline_)
        {
            GLogError(u8"[GPUCullSystem] CreateComputePipeline failed");
            return false;
        }

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

    void GPUCullSystem::Update(float delta_time)
    {
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
        pc.entity_count = entity_count;
        pc.max_visible_count = kMaxVisibleEntities;
        pc.visible_indices_addr = visible_indices_addr_;
        pc.indirect_commands_addr = indirect_commands_addr_;
        pc.vertex_count_per_entity = 36;  // 立方体顶点数（示例值，实际应从 GeometryData 获取）
        pc.first_vertex_per_entity = 0;
        pc.draw_count_addr = draw_count_addr_;

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
