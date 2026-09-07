#pragma once

// RootAddressPush.h — RootAddresses push constant 填充与下发（7 张全局表设备地址）
//
// SSBO 全 BDA 化后，shader 每个 buffer_reference 起点都需要一个地址来源；7 张全局表
// （MeshDrawParams / L2W / L2WIndex / mtl_data_addrs / 文本三表）的地址集中在一个
// 56B push constant block（RootAddresses，ShaderBufferSources.h X 列表）里，渲染路径
// 每 MaterialBatch/每绘制路径渲染前 PushConstants 一次（每材质一次的开销可忽略——
// push 的是当批表地址，不是 per-draw 数据；一次 vkCmdDrawMeshTasksIndirectEXT 内
// N 命令共享同一份，gl_DrawID 仍查行表，不违反间接合批）。
//
// 前提：
//   - 表 buffer 以 SHADER_DEVICE_ADDRESS usage 创建（A1，CreateSSBO 全带）；
//   - GetBufferDeviceAddressAligned16 对非 16B 对齐基址 fail-fast（A2）；
//   - 未拥有的表传 nullptr → 地址 0：shader 只在被本材质消费的地址上解引用，
//     0 地址不触达即安全（与 MeshDrawParams 行整表清零同语义）。
//
// 参数统一取 IGPUBuffer*：DeviceBuffer 经 GetGPUBuffer()、MirroredStructArray
// （文本三表）经 GetGPUBuffer() 都得到该接口，调用方 null 检查后传入即可。

#include<hgl/graph/ShaderBufferSources.h>
#include<hgl/vk/VKCommandBuffer.h>
#include<hgl/vk/IGPUBuffer.h>
#include<hgl/vk/VKDevice.h>

namespace hgl::graph
{
    inline void PushRootAddresses(RenderCmdBuffer *cmd,
                                  VulkanDevice *dev,
                                  const VkPipelineLayout layout,
                                  IGPUBuffer *mesh_draw_params,
                                  IGPUBuffer *l2w             = nullptr,
                                  IGPUBuffer *l2w_index       = nullptr,
                                  IGPUBuffer *mtl_data_addrs  = nullptr,
                                  IGPUBuffer *text_char_info  = nullptr,
                                  IGPUBuffer *text_char_style = nullptr,
                                  IGPUBuffer *text_char_inst  = nullptr)
    {
        if (!cmd || !dev || !layout)
            return;

        mtl::RootAddresses ra{};

        const auto fill = [&](uint64_t &slot, IGPUBuffer *gpu)
        {
            if (gpu)
                slot = dev->GetBufferDeviceAddressAligned16(gpu->GetVkDeviceBuffer());
        };

        fill(ra.addr_mesh_draw_params,    mesh_draw_params);
        fill(ra.addr_l2w,                 l2w);
        fill(ra.addr_l2w_index,           l2w_index);
        fill(ra.addr_mtl_data_addrs,      mtl_data_addrs);
        fill(ra.addr_text_char_info,      text_char_info);
        fill(ra.addr_text_char_style,     text_char_style);
        fill(ra.addr_text_char_instance,  text_char_inst);

        cmd->PushConstants(layout, &ra, sizeof(ra));
    }
}//namespace hgl::graph
