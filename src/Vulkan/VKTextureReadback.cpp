#include<hgl/vk/VKTextureReadback.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKCommandBuffer.h>
#include<hgl/vk/VKFormat.h>
#include<hgl/vk/buffer/DeviceBuffer.h>
#include<hgl/log/Log.h>
#include<hgl/type/String.h>

namespace hgl::graph
{
    namespace
    {
        /// 转入/转出 TRANSFER_SRC 时所需的（源阶段/源访问）与（还原目的阶段/目的访问）
        struct BarrierStage
        {
            VkPipelineStageFlags2 src_stage;
            VkAccessFlags2        src_access;
            VkPipelineStageFlags2 dst_stage;
            VkAccessFlags2        dst_access;
        };

        /**
         * 依据"纹理当前布局 + 是否深度"推出 barrier 的两端。
         *
         * 源端必须与"谁最后写了这张图"匹配（布局是它的结果）；目的端是还原时的写入者。
         * 布局未知/附件布局按 aspect 归类，SHADER_READ_ONLY 按采样读，TRANSFER/GENERAL 按传输。
         */
        BarrierStage MakeBarrierStage(const VkImageLayout layout, const bool depth)
        {
            const VkPipelineStageFlags2 depth_stage = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT |
                                                      VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
            const VkAccessFlags2 depth_access = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                                                VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

            switch (layout)
            {
                case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
                case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
                    return { VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT,
                             VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT };

                case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
                case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
                    return { VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT | VK_ACCESS_2_TRANSFER_WRITE_BIT,
                             VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT | VK_ACCESS_2_TRANSFER_WRITE_BIT };

                case VK_IMAGE_LAYOUT_GENERAL:
                    return { VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT,
                             VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT };

                // UNDEFINED / PRESENT_SRC / 各种附件布局：按"渲染刚写过"处理
                case VK_IMAGE_LAYOUT_UNDEFINED:
                case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
                case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
                case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
                case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL:
                case VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL:
                default:
                    if (depth)
                        return { VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                                 depth_stage, depth_access };

                    return { VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                             VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                             VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT };
            }
        }

        /// 该格式是否含模板位（决定 aspectMask 能否声明 STENCIL）
        bool HasStencil(const VkFormat format)
        {
            return format == VK_FORMAT_D16_UNORM_S8_UINT ||
                   format == VK_FORMAT_D24_UNORM_S8_UINT ||
                   format == VK_FORMAT_D32_SFLOAT_S8_UINT ||
                   format == VK_FORMAT_S8_UINT;
        }
    }//namespace

    bool ReadbackTexture(VulkanDevice *device, Texture *tex,
                         std::vector<uint8_t> &out_pixels,
                         TextureReadbackInfo *out_info)
    {
        if (out_info)
            *out_info = TextureReadbackInfo{};

        out_pixels.clear();

        if (!device || !tex)
            return false;

        const VkImage image = tex->GetImage();
        const VkExtent3D *extent = tex->GetExtent();
        const VkFormat format = tex->GetFormat();

        if (image == VK_NULL_HANDLE || !extent || extent->width == 0 || extent->height == 0)
            return false;

        const uint32_t width = extent->width;
        const uint32_t height = extent->height;
        const uint32_t pixel_size = GetStrideByFormat(format);

        if (pixel_size == 0)
        {
            GLogError(u8"[Readback] 格式 %d 无已知每像素字节数（压缩格式或非图像格式），无法读回",
                      static_cast<int>(format));
            return false;
        }

        // aspect 优先取纹理自身的（ImageView 上已按格式判定）；缺省时按格式归类
        VkImageAspectFlags aspect = tex->GetAspect();
        if (aspect == 0)
            aspect = (format == VK_FORMAT_D16_UNORM || format == VK_FORMAT_D32_SFLOAT ||
                      HasStencil(format))
                         ? VK_IMAGE_ASPECT_DEPTH_BIT
                         : VK_IMAGE_ASPECT_COLOR_BIT;

        // D16/D32 这类纯深度格式不能声明 STENCIL 位（与 EndRenderingPresent 的规则一致）
        if (!HasStencil(format))
            aspect &= ~VK_IMAGE_ASPECT_STENCIL_BIT;

        const bool is_depth = (aspect & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) != 0;

        const VkDeviceSize bytes = VkDeviceSize(width) * VkDeviceSize(height) * VkDeviceSize(pixel_size);
        const VkImageLayout cur_layout = tex->GetImageLayout();
        const BarrierStage stage = MakeBarrierStage(cur_layout, is_depth);
        const VkImageSubresourceRange range{aspect, 0, 1, 0, 1};

        // 读回在帧外进行：先把图形队列排空，避免与在途帧竞争同一张图（内容也会更确定）
        vkQueueWaitIdle(device->GetGraphicsQueue());

        DeviceBuffer *staging = device->CreateBuffer(
            ObjectNameBuilder(AnsiString("TextureReadback:Staging")),
            VK_BUFFER_USAGE_TRANSFER_DST_BIT, bytes, bytes, nullptr,
            BufferAllocPolicy::Readback, SharingMode::Exclusive);
        if (!staging)
            return false;

        TextureCmdBuffer *cmd = device->CreateTextureCommandBuffer(
            ObjectNameBuilder(AnsiString("TextureReadback:Cmd")));
        if (!cmd)
        {
            delete staging;
            return false;
        }

        bool ok = cmd->Begin();

        if (ok)
        {
            cmd->ImageMemoryBarrier2(image,
                                     stage.src_stage, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                                     stage.src_access, VK_ACCESS_2_TRANSFER_READ_BIT,
                                     cur_layout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                     range);

            VkBufferImageCopy region{};
            region.imageSubresource = {aspect, 0, 0, 1};
            region.imageExtent = {width, height, 1};

            cmd->CopyImageToBuffer(image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                   staging->GetBuffer(), 1, &region);

            cmd->ImageMemoryBarrier2(image,
                                     VK_PIPELINE_STAGE_2_TRANSFER_BIT, stage.dst_stage,
                                     VK_ACCESS_2_TRANSFER_READ_BIT, stage.dst_access,
                                     VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, cur_layout,
                                     range);

            ok = cmd->End();
        }

        if (ok)
        {
            const VkCommandBuffer raw_cmd = *cmd;

            VkFenceCreateInfo fi{};
            fi.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

            VkFence fence = VK_NULL_HANDLE;

            if (vkCreateFence(device->GetDevice(), &fi, nullptr, &fence) != VK_SUCCESS)
            {
                ok = false;
            }
            else
            {
                VkSubmitInfo si{};
                si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
                si.commandBufferCount = 1;
                si.pCommandBuffers = &raw_cmd;

                ok = (vkQueueSubmit(device->GetGraphicsQueue(), 1, &si, fence) == VK_SUCCESS);

                if (ok)
                    ok = (vkWaitForFences(device->GetDevice(), 1, &fence, VK_TRUE, UINT64_MAX) == VK_SUCCESS);

                vkDestroyFence(device->GetDevice(), fence, nullptr);
            }
        }

        if (ok)
        {
            const void *mapped = staging->GetGPUBuffer()->Map(0, bytes);

            if (mapped)
            {
                const uint8_t *first = static_cast<const uint8_t *>(mapped);
                out_pixels.assign(first, first + bytes);
                staging->GetGPUBuffer()->Unmap();
            }
            else
            {
                ok = false;
            }
        }

        delete cmd;
        delete staging;

        if (ok && out_info)
        {
            out_info->width = width;
            out_info->height = height;
            out_info->pixel_size = pixel_size;
            out_info->row_pitch = width * pixel_size;
            out_info->format = format;
            out_info->is_depth = is_depth;
            out_info->valid = true;
        }

        return ok;
    }

    bool ReadbackColorTarget(IRenderTarget *rt, std::vector<uint8_t> &out_pixels,
                             uint32_t color_index, TextureReadbackInfo *out_info)
    {
        if (!rt)
            return false;

        return ReadbackTexture(rt->GetDevice(), rt->GetColorTexture(static_cast<int>(color_index)),
                               out_pixels, out_info);
    }

    bool ReadbackDepthTarget(IRenderTarget *rt, std::vector<uint8_t> &out_pixels,
                             TextureReadbackInfo *out_info)
    {
        if (!rt)
            return false;

        return ReadbackTexture(rt->GetDevice(), rt->GetDepthTexture(), out_pixels, out_info);
    }
}//namespace hgl::graph
