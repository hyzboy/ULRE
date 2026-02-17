#pragma once

/**
 * ObjectBase 使用示例
 * 
 * 展示如何为各种Vulkan对象添加追踪
 * 只需派生自ObjectBase并在构造/析构时传入位置信息即可
 */

#include<hgl/utils/ObjectBase.h>
#include<vulkan/vulkan.h>

namespace hgl::graph
{
    // ============================================================
    // 示例1: Fence 对象追踪
    // ============================================================
    
    class VKFenceBase : public hgl::utils::ObjectBase
    {
    protected:
        VkDevice device_;
        VkFence fence_;

    public:
        /**
         * 创建Fence并自动追踪
         * 强制传入创建位置
         */
        VKFenceBase(
            VkDevice device,
            VkFence fence,
            const std::string& fence_name,
            const std::source_location& loc = std::source_location::current()
        ) noexcept
            : ObjectBase(hgl::core::ObjectTypeTag::VKFence, fence_name, loc)
            , device_(device)
            , fence_(fence)
        {
            printf("[VKFenceBase] Created Fence 0x%lx: %s at %s\n",
                   object_id_, fence_name.c_str(), creation_loc_.to_string().c_str());
        }

        virtual ~VKFenceBase() noexcept override
        {
            // 自动记录销毁位置
            HGL_OBJECT_DESTROY_LOCATION();
            
            if (fence_ != VK_NULL_HANDLE && device_ != VK_NULL_HANDLE)
            {
                vkDestroyFence(device_, fence_, nullptr);
                printf("[VKFenceBase] Destroyed Fence 0x%lx\n", object_id_);
            }
        }

        VkFence get_handle() const noexcept { return fence_; }
    };

    // ============================================================
    // 示例2: Buffer 对象追踪
    // ============================================================

    class VKBufferBase : public hgl::utils::ObjectBase
    {
    protected:
        VkDevice device_;
        VkBuffer buffer_;
        VkDeviceMemory memory_;
        VkDeviceSize size_;

    public:
        VKBufferBase(
            VkDevice device,
            VkBuffer buffer,
            VkDeviceMemory memory,
            VkDeviceSize size,
            const std::string& buffer_name,
            const std::source_location& loc = std::source_location::current()
        ) noexcept
            : ObjectBase(hgl::core::ObjectTypeTag::VKBuffer, buffer_name, loc)
            , device_(device)
            , buffer_(buffer)
            , memory_(memory)
            , size_(size)
        {
            printf("[VKBufferBase] Created Buffer 0x%lx (%llu bytes): %s\n",
                   object_id_, size, buffer_name.c_str());
        }

        virtual ~VKBufferBase() noexcept override
        {
            HGL_OBJECT_DESTROY_LOCATION();
            
            if (buffer_ != VK_NULL_HANDLE && device_ != VK_NULL_HANDLE)
            {
                vkDestroyBuffer(device_, buffer_, nullptr);
                if (memory_ != VK_NULL_HANDLE)
                {
                    vkFreeMemory(device_, memory_, nullptr);
                }
                printf("[VKBufferBase] Destroyed Buffer 0x%lx\n", object_id_);
            }
        }

        VkBuffer get_handle() const noexcept { return buffer_; }
        VkDeviceSize get_size() const noexcept { return size_; }
    };

    // ============================================================
    // 示例3: Image 对象追踪
    // ============================================================

    class VKImageBase : public hgl::utils::ObjectBase
    {
    protected:
        VkDevice device_;
        VkImage image_;
        VkDeviceMemory memory_;

    public:
        VKImageBase(
            VkDevice device,
            VkImage image,
            VkDeviceMemory memory,
            const std::string& image_name,
            const std::source_location& loc = std::source_location::current()
        ) noexcept
            : ObjectBase(hgl::core::ObjectTypeTag::VKImage, image_name, loc)
            , device_(device)
            , image_(image)
            , memory_(memory)
        {
        }

        virtual ~VKImageBase() noexcept override
        {
            HGL_OBJECT_DESTROY_LOCATION();
            
            if (image_ != VK_NULL_HANDLE && device_ != VK_NULL_HANDLE)
            {
                vkDestroyImage(device_, image_, nullptr);
                if (memory_ != VK_NULL_HANDLE)
                {
                    vkFreeMemory(device_, memory_, nullptr);
                }
            }
        }

        VkImage get_handle() const noexcept { return image_; }
    };

    // ============================================================
    // 示例4: CommandBuffer 对象追踪
    // ============================================================

    class VKCommandBufferBase : public hgl::utils::ObjectBase
    {
    protected:
        VkDevice device_;
        VkCommandBuffer cmd_buf_;
        VkCommandPool pool_;

    public:
        VKCommandBufferBase(
            VkDevice device,
            VkCommandBuffer cmd_buf,
            VkCommandPool pool,
            const std::string& cmdname,
            const std::source_location& loc = std::source_location::current()
        ) noexcept
            : ObjectBase(hgl::core::ObjectTypeTag::VKRenderCmdBuf, cmdname, loc)
            , device_(device)
            , cmd_buf_(cmd_buf)
            , pool_(pool)
        {
        }

        virtual ~VKCommandBufferBase() noexcept override
        {
            HGL_OBJECT_DESTROY_LOCATION();
            
            if (cmd_buf_ != VK_NULL_HANDLE && device_ != VK_NULL_HANDLE)
            {
                vkFreeCommandBuffers(device_, pool_, 1, &cmd_buf_);
            }
        }

        VkCommandBuffer get_handle() const noexcept { return cmd_buf_; }
    };

    // ============================================================
    // 示例5: Material 对象追踪
    // ============================================================

    class MaterialBase : public hgl::utils::ObjectBase
    {
    protected:
        std::string material_data_;

    public:
        MaterialBase(
            const std::string& name,
            const std::source_location& loc = std::source_location::current()
        ) noexcept
            : ObjectBase(hgl::core::ObjectTypeTag::Material, name, loc)
        {
            printf("[MaterialBase] Created Material: %s\n", name.c_str());
        }

        virtual ~MaterialBase() noexcept override
        {
            HGL_OBJECT_DESTROY_LOCATION();
            printf("[MaterialBase] Destroyed Material: %s\n", object_name_.c_str());
        }
    };

} // namespace hgl::graph

// ============================================================
// 使用方式总结
// ============================================================

/**
 * 迁移现有代码的步骤：
 * 
 * 1. 为每个需要追踪的类创建基类版本
 *    class MyResourceBase : public ObjectBase { ... }
 * 
 * 2. 在构造函数中传入创建位置
 *    : ObjectBase(TYPE, name, std::source_location::current())
 * 
 * 3. 在析构函数中记录销毁位置
 *    virtual ~MyResourceBase() override
 *    {
 *        HGL_OBJECT_DESTROY_LOCATION();
 *        // 清理代码...
 *    }
 * 
 * 4. 跟踪信息自动保存，无需额外代码
 * 
 * 5. 可以随时查询对象信息
 *    HGL_LIST_ALL_OBJECTS();
 *    HGL_REPORT_LEAKS();
 *    auto obj = HGL_FIND_OBJECT(id, MyResourceBase);
 */
