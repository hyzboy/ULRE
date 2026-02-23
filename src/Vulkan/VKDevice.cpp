#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKSemaphore.h>
#include<hgl/vk/VKFence.h>
#include<hgl/vk/VKQueue.h>
#include<hgl/vk/VKCommandBuffer.h>
#include<hgl/vk/pipeline/VKComputePipeline.h>
#include<hgl/vk/VKObjectName.h>
#include<hgl/vk/IGPUBuffer.h>
#include<hgl/log/Log.h>
#include <algorithm>
#include <functional>
#include <thread>
#include <utility>
#include <cstdint>
#include <unordered_map>
#include <string>

namespace hgl::graph{
namespace
{
    std::unordered_map<VkDevice, VulkanDevice *> g_device_map;

    void BuildDebugNames(const AnsiString &name, std::string &buffer_name, std::string &memory_name)
    {
        const std::string base = name.c_str();
        const std::string token = ":Buffer:";

        const size_t pos = base.find(token);
        if (pos != std::string::npos)
        {
            buffer_name = base;
            memory_name = base;
            memory_name.replace(pos, token.size(), ":Memory:");
            return;
        }

        const std::string buffer_suffix = ":Buffer";
        const std::string memory_suffix = ":Memory";
        if (base.size() >= buffer_suffix.size() &&
            base.compare(base.size() - buffer_suffix.size(), buffer_suffix.size(), buffer_suffix) == 0)
        {
            buffer_name = base;
            memory_name = base.substr(0, base.size() - buffer_suffix.size()) + ":Memory";
            return;
        }

        if (base.size() >= memory_suffix.size() &&
            base.compare(base.size() - memory_suffix.size(), memory_suffix.size(), memory_suffix) == 0)
        {
            memory_name = base;
            buffer_name = base.substr(0, base.size() - memory_suffix.size()) + ":Buffer";
            return;
        }

        buffer_name = base + ":Buffer";
        memory_name = base + ":Memory";
    }
}

VulkanDevice::VulkanDevice(VulkanDevAttr *da)
{
    attr=da;

    if(attr && attr->device)
        g_device_map[attr->device] = this;
}

VulkanDevice::~VulkanDevice()
{
    if(attr && attr->device)
        g_device_map.erase(attr->device);

    // 在设备销毁前，输出所有未销毁的对象以检测泄漏
    LogInfo("\n=== VulkanDevice Destructor Called ===");

    DumpTrackedObjects();

    LogInfo("=== End VulkanDevice Destructor ===");

    delete attr;
}

VulkanDevice *VulkanDevice::FromDevice(VkDevice device)
{
    auto it = g_device_map.find(device);
    return it == g_device_map.end() ? nullptr : it->second;
}

void VulkanDevice::RegisterGPUBuffer(IGPUBuffer *buf)
{
    if (!buf)
        return;
    gpu_buffer_registry.push_back(buf);
}

void VulkanDevice::UnregisterGPUBuffer(IGPUBuffer *buf)
{
    if (!buf)
        return;
    auto it = std::find(gpu_buffer_registry.begin(), gpu_buffer_registry.end(), buf);
    if (it != gpu_buffer_registry.end())
        gpu_buffer_registry.erase(it);
}

void VulkanDevice::TrackObject(VkObjectType type, uint64_t handle, const ObjectNameBuilder &name, const std::source_location &loc)
{
    if (!handle)
        return;

    ObjectDebugRecord record;
    record.name = name.ToString();  // 只在需要时转换为字符串
    record.file = loc.file_name();
    record.function = loc.function_name();
    record.line = loc.line();
    record.stack_depth = hgl::utils::get_current_allocation_stack(record.stack, 64);

#ifdef _DEBUG
    // 详细日志：记录对象创建
    LogDebug("[CREATE] Object tracked");
#endif//_DEBUG

    ObjectKey key{type, handle};
    auto it = tracked_objects.find(key);
    if (it != tracked_objects.end())
    {
        if (!record.name.IsEmpty())
            it->second.name = record.name;

        if (it->second.file.empty())
            it->second = std::move(record);
    }
    else
    {
        tracked_objects.emplace(key, std::move(record));
    }

#ifdef _DEBUG
    if (attr && attr->debug_utils,type!=VK_OBJECT_TYPE_UNKNOWN)
    {
        attr->debug_utils->SetName(type, handle, name.ToString().c_str());
    }
#endif//_DEBUG
}

void VulkanDevice::TrackObjectWithoutLocation(VkObjectType type, uint64_t handle, const ObjectNameBuilder &name)
{
    if (!handle)
        return;

    ObjectKey key{type, handle};
    auto it = tracked_objects.find(key);
    if (it != tracked_objects.end())
    {
        if (it->second.name.IsEmpty())
            it->second.name = name.ToString();  // 只在需要时转换为字符串
        return;
    }

    ObjectDebugRecord record;
    record.name = name.ToString();
    record.file = "";
    record.function = "";
    record.line = 0;
    record.stack_depth = hgl::utils::get_current_allocation_stack(record.stack, 64);

    tracked_objects.emplace(key, std::move(record));
}

void VulkanDevice::UntrackObject(VkObjectType type, uint64_t handle)
{
    if (!handle)
        return;

    ObjectKey key{type, handle};
    auto it = tracked_objects.find(key);

#ifdef _DEBUG
    if (it == tracked_objects.end())
    {
        LogWarning("[DESTROY-WARNING] Object not tracked - possible double destroy");
    }
#endif//_DEBUG

    tracked_objects.erase(key);
}

void VulkanDevice::DumpTrackedObjects() const
{
    if (tracked_objects.empty())
    {
        GLogInfo("No live objects at device destruction");
        return;
    }

    GLogWarning("%lu objects still alive at device destruction", (unsigned long)tracked_objects.size());

    // 按类型分类统计
    std::map<VkObjectType, int> type_counts;
    for (const auto &entry : tracked_objects)
    {
        type_counts[entry.first.type]++;
    }

    // 输出各类型统计
    for (const auto &tc : type_counts)
    {
        const char *type_name = "UNKNOWN";
        if (tc.first == VK_OBJECT_TYPE_BUFFER) type_name = "VkBuffer";
        else if (tc.first == VK_OBJECT_TYPE_IMAGE) type_name = "VkImage";
        else if (tc.first == VK_OBJECT_TYPE_DEVICE_MEMORY) type_name = "VkDeviceMemory";
        else if (tc.first == VK_OBJECT_TYPE_IMAGE_VIEW) type_name = "VkImageView";
        else if (tc.first == VK_OBJECT_TYPE_SAMPLER) type_name = "VkSampler";
        else if (tc.first == VK_OBJECT_TYPE_FRAMEBUFFER) type_name = "VkFramebuffer";
        else if (tc.first == VK_OBJECT_TYPE_RENDER_PASS) type_name = "VkRenderPass";
        else if (tc.first == VK_OBJECT_TYPE_PIPELINE) type_name = "VkPipeline";
        else if (tc.first == VK_OBJECT_TYPE_PIPELINE_LAYOUT) type_name = "VkPipelineLayout";
        else if (tc.first == VK_OBJECT_TYPE_DESCRIPTOR_SET) type_name = "VkDescriptorSet";
        else if (tc.first == VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT) type_name = "VkDescriptorSetLayout";

        GLogWarning("  %s: %d", type_name, tc.second);
    }

    // 输出详细泄漏信息
    for (const auto &entry : tracked_objects)
    {
        const ObjectKey &key = entry.first;
        const ObjectDebugRecord &record = entry.second;

        GLogWarning("[LEAK] Type: 0x%x Handle: 0x%llx Name: %s",
                    (unsigned int)key.type,
                    (unsigned long long)key.handle,
                    record.name.c_str());
    }

}

void VulkanDevice::TrackBuffer(VkBufferOwner *buf, const ObjectNameBuilder &name, const std::source_location &loc)
{
    if (!buf)
        return;

    TrackObject(VK_OBJECT_TYPE_BUFFER, (uint64_t)(uintptr_t)buf->GetBuffer(), name.Append(ObjectTypeTag::VKBuffer), loc);
    TrackObject(VK_OBJECT_TYPE_DEVICE_MEMORY, (uint64_t)(uintptr_t)buf->GetVkMemory(), name.Append(ObjectTypeTag::VKMemory), loc);
}

void VulkanDevice::TrackTexture(Texture *tex, const ObjectNameBuilder &name, const std::source_location &loc)
{
    if (!tex)
        return;

    TrackObject(VK_OBJECT_TYPE_IMAGE, (uint64_t)(uintptr_t)tex->GetImage(), name.Append(ObjectTypeTag::VKImage), loc);
    TrackObject(VK_OBJECT_TYPE_IMAGE_VIEW, (uint64_t)(uintptr_t)tex->GetVulkanImageView(), name.Append(ObjectTypeTag::VKImageView), loc);
    TrackObject(VK_OBJECT_TYPE_DEVICE_MEMORY, (uint64_t)(uintptr_t)tex->GetDeviceMemory(), name.Append(ObjectTypeTag::VKMemory), loc);
}

void VulkanDevice::WaitIdle() const
{
    if (!attr || attr->device == VK_NULL_HANDLE)
    {
        GLogError("VulkanDevice::WaitIdle invalid device (attr=%p device=%p)",
                  (void *)attr,
                  attr ? (void *)attr->device : nullptr);
        return;
    }

    const VkResult res = vkDeviceWaitIdle(attr->device);
    if (res != VK_SUCCESS)
    {
        const auto tid = std::hash<std::thread::id>{}(std::this_thread::get_id());
        GLogError("VulkanDevice::WaitIdle failed res=%d device=%p thread=%llu",
                  static_cast<int>(res),
                  (void *)attr->device,
                  static_cast<unsigned long long>(tid));
    }
}

VkCommandBuffer VulkanDevice::CreateCommandBuffer(const AnsiString &name)
{
    if(!attr->cmd_pool)
        return(VK_NULL_HANDLE);

    CommandBufferAllocateInfo cmd;

    cmd.commandPool         =attr->cmd_pool;
    cmd.level               =VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmd.commandBufferCount  =1;

    VkCommandBuffer cmd_buf;

    VkResult res=vkAllocateCommandBuffers(attr->device,&cmd,&cmd_buf);

    if(res!=VK_SUCCESS)
        return(VK_NULL_HANDLE);

#ifdef _DEBUG
    if(attr->debug_utils)
        attr->debug_utils->SetCommandBuffer(cmd_buf,name);
#endif//_DEBUG

    return cmd_buf;
}

RenderCmdBuffer *VulkanDevice::CreateRenderCommandBuffer(const ObjectNameBuilder &name, const std::source_location &loc)
{
    VkCommandBuffer cb=CreateCommandBuffer(name.ToString());

    if(cb==VK_NULL_HANDLE)return(nullptr);

    RenderCmdBuffer *result = new RenderCmdBuffer(attr,cb);
    if (result)
        TrackObject(VK_OBJECT_TYPE_COMMAND_BUFFER, (uint64_t)(uintptr_t)cb, name.Append(ObjectTypeTag::VKRenderCommandBuffer), loc);
    return result;
}

TextureCmdBuffer *VulkanDevice::CreateTextureCommandBuffer(const ObjectNameBuilder &name, const std::source_location &loc)
{
    VkCommandBuffer cb=CreateCommandBuffer(name.ToString());

    if(cb==VK_NULL_HANDLE)return(nullptr);

    TextureCmdBuffer *result = new TextureCmdBuffer(attr,cb);
    if (result)
        TrackObject(VK_OBJECT_TYPE_COMMAND_BUFFER, (uint64_t)(uintptr_t)cb, name.Append(ObjectTypeTag::VKTextureCommandBuffer), loc);
    return result;
}

/**
 * 创建栅栏
 * @param create_signaled 是否创建初始信号
 */
Fence *VulkanDevice::CreateFence(const ObjectNameBuilder &name, bool create_signaled, const std::source_location &loc)
{
    FenceCreateInfo fenceInfo(create_signaled?VK_FENCE_CREATE_SIGNALED_BIT:0);

    VkFence fence;

    if(vkCreateFence(attr->device, &fenceInfo, nullptr, &fence)!=VK_SUCCESS)
        return(nullptr);

    TrackObject(VK_OBJECT_TYPE_FENCE, (uint64_t)(uintptr_t)fence, name.Append(ObjectTypeTag::VKFence), loc);

    // 创建Fence对象并传入追踪信息
    std::string fence_name = name.Append(ObjectTypeTag::VKFence).ToString().c_str();
    return(new Fence(attr->device, fence, fence_name, loc));
}

Fence *VulkanDevice::CreateFence(bool create_signaled, const std::source_location &loc)
{
    return CreateFence(ObjectNameBuilder("Fence"), create_signaled, loc);
}

Semaphore *VulkanDevice::CreateGPUSemaphore(const ObjectNameBuilder &name, const std::source_location &loc)
{
    SemaphoreCreateInfo SemaphoreCreateInfo;

    VkSemaphore sem;

    if(vkCreateSemaphore(attr->device, &SemaphoreCreateInfo, nullptr, &sem)!=VK_SUCCESS)
        return(nullptr);

    Semaphore *result = new Semaphore(attr->device, sem);
    if (result)
        TrackObject(VK_OBJECT_TYPE_SEMAPHORE, (uint64_t)(uintptr_t)sem, name.Append(ObjectTypeTag::VKSemaphore), loc);
    return result;
}

DeviceQueue *VulkanDevice::CreateQueue(const ObjectNameBuilder &name, const uint32_t fence_count, const bool create_signaled, const std::source_location &loc)
{
    if(fence_count<=0)return(nullptr);

    Fence **fence_list=new Fence *[fence_count];

    const std::string base_name = name.ToString().c_str();
    for(uint32_t i=0;i<fence_count;i++)
    {
        std::string fence_name = base_name;
        fence_name += ":Fence[";
        fence_name += std::to_string(i);
        fence_name += "]";
        fence_list[i] = CreateFence(ObjectNameBuilder(fence_name.c_str()), create_signaled, loc);
    }

    DeviceQueue *result = new DeviceQueue(attr->device, attr->graphics_queue, fence_list, fence_count);
    // Note: We don't track VkQueue because it's retrieved via vkGetDeviceQueue and
    // is implicitly destroyed when VkDevice is destroyed. Multiple DeviceQueue C++ wrappers
    // may share the same VkQueue handle.
    return result;
}

ComputePipeline *VulkanDevice::CreateComputePipeline(const AnsiString &name, VkShaderModule shader_module, VkPipelineLayout pipeline_layout)
{
    VkComputePipelineCreateInfo compute_pipeline_info = {};
    compute_pipeline_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    compute_pipeline_info.layout = pipeline_layout;
    compute_pipeline_info.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    compute_pipeline_info.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    compute_pipeline_info.stage.module = shader_module;
    compute_pipeline_info.stage.pName = "main";

    VkPipeline pipeline;
    if (vkCreateComputePipelines(attr->device, attr->pipeline_cache, 1, &compute_pipeline_info, nullptr, &pipeline) != VK_SUCCESS)
    {
        LogError(name + " create compute pipeline failed!");
        return nullptr;
    }

#ifdef _DEBUG
    if(attr->debug_utils)
        attr->debug_utils->SetPipeline(pipeline, name);
#endif//_DEBUG

    return new ComputePipeline(name, attr->device, pipeline, pipeline_layout);
}

}//namespace hgl::graph
