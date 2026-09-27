#pragma once

// RootAddressPush.h — RootAddresses push constant 填充与下发（地址根入口）
//
// SSBO 全 BDA 化后，shader 每个 buffer_reference 起点都需要一个地址来源。地址分两类：
//   · **全局 / 长期有效**：全部收在全局地址表（GlobalAddresses SSBO）里，push 只带该表
//     的基址（addr_global_addresses，表内寻址）；
//   · **每批 / 每材质 / 本字体**：同帧内逐批不同，只能随 push 走（本批的 8 个表地址）。
// 二者合成一个 80B push constant block（RootAddresses，ShaderBufferSources.h X 列表），渲染路径
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
// 参数统一取 IGPUBuffer*：调用方持有 DeviceBuffer*，经 GetGPUBuffer() 得到该接口，
// null 检查后传入即可（文本三表同样是 DeviceBuffer + 数组视图，不再有镜像容器）。

#include<hgl/graph/ShaderBufferSources.h>
#include<hgl/log/Log.h>
#include<hgl/vk/VKCommandBuffer.h>
#include<hgl/vk/buffer/IGPUBuffer.h>
#include<hgl/vk/VKDevice.h>

namespace hgl::graph
{
    inline void PushRootAddresses(RenderCmdBuffer *cmd,
                                  VulkanDevice *dev,
                                  const VkPipelineLayout layout,
                                  const uint64_t addr_global_addresses,
                                  IGPUBuffer *mesh_draw_params,
                                  IGPUBuffer *l2w                     = nullptr,
                                  IGPUBuffer *l2w_index               = nullptr,
                                  IGPUBuffer *mtl_data_addrs          = nullptr,
                                  uint64_t    addr_texture_references = 0,
                                  IGPUBuffer *text_char_info          = nullptr,
                                  IGPUBuffer *text_char_style         = nullptr,
                                  IGPUBuffer *text_char_inst          = nullptr,
                                  uint32_t    camera_id               = 0)
    {
        if (!cmd || !dev || !layout)
            return;

        // 全局地址表基址是本次 push 的根入口：为 0 时 shader 解引用 0 基址 = UB / 设备丢失
        //（0 VUID 判据抓不到），因此缺地址要显式吼一次，而不是静默当「未绑定」。
        if (addr_global_addresses == 0)
        {
            static bool warned_missing_table = false;
            if (!warned_missing_table)
            {
                warned_missing_table = true;
                GLogWarning("[PushRootAddresses] addr_global_addresses=0 —— 全局地址表地址缺失"
                            "（shader 解引用即 UB；查 GlobalSSBOBufferRegistry 是否已初始化）");
            }
        }

        mtl::RootAddresses ra{};

        const auto fill = [&](uint64_t &slot, IGPUBuffer *gpu)
        {
            if (gpu)
                slot = dev->GetBufferDeviceAddressAligned16(gpu->GetVkDeviceBuffer());
        };

        ra.addr_global_addresses     = addr_global_addresses;
        fill(ra.addr_mesh_draw_params,    mesh_draw_params);
        fill(ra.addr_l2w,                 l2w);
        fill(ra.addr_l2w_index,           l2w_index);
        fill(ra.addr_mtl_data_addrs,      mtl_data_addrs);
        ra.addr_texture_references = addr_texture_references;
        fill(ra.addr_text_char_info,      text_char_info);
        fill(ra.addr_text_char_style,     text_char_style);
        fill(ra.addr_text_char_instance,  text_char_inst);
        ra.camera_id    = camera_id;
        ra._pad_camera  = 0;

        cmd->PushConstants(layout, &ra, sizeof(ra));
    }
}//namespace hgl::graph
