#pragma once

#include<hgl/vk/VK.h>
#include<hgl/vk/VKTexture.h>
#include<hgl/vk/VKRenderTarget.h>
#include<vector>

/**
 * 纹理/渲染目标读回（GPU → CPU）。
 *
 * 引擎基础功能：把整幅纹理（颜色或深度附件）同步读回 CPU 字节流，
 * 供离线处理与诊断取证使用（写图、逐像素契约、与参考数据比对）。
 *
 * 设计要点：
 * - **不改动布局跟踪**：内部从纹理跟踪的当前布局转到 TRANSFER_SRC，读完还原。
 *   布局跟踪由 `RenderCmdBuffer::EndRenderingPresent` 在渲染结束时维护
 *   （交换链颜色 → PRESENT_SRC_KHR；离屏颜色/深度 → SHADER_READ_ONLY_OPTIMAL）。
 * - **像素为原始字节**：按行主序、自上而下（与 Vulkan 图像坐标一致，无需翻转），
 *   不做任何格式转换；每像素字节数取 `GetStrideByFormat(纹理格式)`。
 * - 每次调用新建 staging buffer 与一次性命令缓冲，读完即释放，并在提交前排空图形队列
 *   ⇒ 只用于**离线/诊断**，不要放进每帧渲染热路径。
 */
namespace hgl::graph
{
    /// 读回结果描述（像素本体在调用方的 vector 里）
    struct TextureReadbackInfo
    {
        uint32_t width      = 0;                    ///<纹理宽（像素）
        uint32_t height     = 0;                    ///<纹理高（像素）
        uint32_t pixel_size = 0;                    ///<每像素字节数（GetStrideByFormat）
        uint32_t row_pitch  = 0;                    ///<行字节数（= width * pixel_size，本 API 不做行对齐）
        VkFormat format     = VK_FORMAT_UNDEFINED;  ///<纹理格式
        bool     is_depth   = false;                ///<是否深度（含模板）附件
        bool     valid      = false;                ///<本结构是否已填写
    };

    /**
     * 读回一张纹理的整幅内容。
     * @param device 所属设备（纹理自身不回落设备，需显式给出）
     * @param tex 目标纹理（需为 2D、单 mip、单层；格式的每像素字节数必须可知，压缩格式不支持）
     * @param out_pixels 输出原始像素字节（尺寸 = row_pitch * height）
     * @param out_info 可选：尺寸/格式/步长信息
     */
    bool ReadbackTexture(VulkanDevice *device, Texture *tex,
                         std::vector<uint8_t> &out_pixels,
                         TextureReadbackInfo *out_info = nullptr);

    /// 读回渲染目标的颜色附件（color_index 越界或该附件不存在返回 false）
    bool ReadbackColorTarget(IRenderTarget *rt, std::vector<uint8_t> &out_pixels,
                             uint32_t color_index = 0,
                             TextureReadbackInfo *out_info = nullptr);

    /// 读回渲染目标的深度附件
    bool ReadbackDepthTarget(IRenderTarget *rt, std::vector<uint8_t> &out_pixels,
                             TextureReadbackInfo *out_info = nullptr);
}//namespace hgl::graph
