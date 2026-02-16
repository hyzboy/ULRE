#include<hgl/vk/VKDevice.h>
#include<hgl/vk/VKSemaphore.h>
#include<hgl/vk/VKFence.h>
#include<hgl/vk/VKQueue.h>
#include<hgl/vk/VKCommandBuffer.h>
#include<hgl/vk/VKBufferUpdateQueue.h>
#include<hgl/vk/VKBufferCommitQueue.h>
#include<hgl/vk/pipeline/VKComputePipeline.h>
#include<hgl/vk/BufferPolicyImpl.h>
#include<iostream>
#include <functional>
#include <thread>
#include <utility>
#include <cstdint>
#include <unordered_map>
#include <string>

VK_NAMESPACE_BEGIN
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
    buffer_update_queue = new BufferUpdateQueue(da->device);

    // Initialize BufferCommitQueue with device policies from physical device
    const AllDeviceBufferPolicies *policies = nullptr;
    if(da->physical_device)
        policies = da->physical_device->GetAllBufferPolicies();
    buffer_commit_queue = new BufferCommitQueue(policies);

    if(attr && attr->device)
        g_device_map[attr->device] = this;
}

VulkanDevice::~VulkanDevice()
{
    if(attr && attr->device)
        g_device_map.erase(attr->device);

    // 在设备销毁前，输出所有未销毁的对象以检测泄漏
    std::cout.flush();
    std::cerr.flush();
    std::cout << "\n=== VulkanDevice Destructor Called ===\n";
    std::cout.flush();
    
    DumpTrackedObjects();
    
    std::cout << "=== End VulkanDevice Destructor ===\n";
    std::cout.flush();

    delete buffer_update_queue;
    delete buffer_commit_queue;
    delete attr;
}

VulkanDevice *VulkanDevice::FromDevice(VkDevice device)
{
    auto it = g_device_map.find(device);
    return it == g_device_map.end() ? nullptr : it->second;
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

#ifdef _DEBUG
    // 详细日志：记录对象创建 - 在 move 之前输出
    std::cout << "[CREATE] Type:" << static_cast<uint32_t>(type)
              << " Handle:0x" << std::hex << static_cast<unsigned long long>(handle) << std::dec
              << " Name:" << record.name.c_str()
              << " at " << record.file.c_str()
              << ":" << record.function.c_str()
              << ":" << record.line << std::endl;
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
    if (attr && attr->debug_utils)
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

    tracked_objects.emplace(key, std::move(record));
}

void VulkanDevice::UntrackObject(VkObjectType type, uint64_t handle)
{
    if (!handle)
        return;

    ObjectKey key{type, handle};
    auto it = tracked_objects.find(key);
    
#ifdef _DEBUG
    if (it != tracked_objects.end())
    {
        const ObjectDebugRecord &record = it->second;
        // 详细日志：记录对象销毁 - 直接输出到 stdout（在删除前输出）
        std::cout << "[DESTROY] Type:" << static_cast<uint32_t>(type)
                  << " Handle:0x" << std::hex << static_cast<unsigned long long>(handle) << std::dec
                  << " Name:" << record.name.c_str()
                  << " (previously tracked at " << record.file.c_str()
                  << ":" << record.function.c_str()
                  << ":" << record.line << ")" << std::endl;
    }
    else
    {
        // 对象没有被追踪
        std::cerr << "[DESTROY-WARNING] Type:" << static_cast<uint32_t>(type)
                  << " Handle:0x" << std::hex << static_cast<unsigned long long>(handle) << std::dec
                  << " (NOT TRACKED - possible double destroy or missing TrackObject)" << std::endl;
    }
#endif//_DEBUG

    tracked_objects.erase(key);
}

void VulkanDevice::DumpTrackedObjects() const
{
    if (tracked_objects.empty())
    {
        std::cout << "========== [VULKAN LEAK DETECTION] No live objects at device destruction ==========\n";
        return;
    }

    std::cout << "========== [VULKAN LEAK DETECTION] Live objects at device destruction: " << tracked_objects.size() << " ==========\n";

    // 按类型分类统计
    std::map<VkObjectType, int> type_counts;
    for (const auto &entry : tracked_objects)
    {
        type_counts[entry.first.type]++;
    }

    // 输出各类型统计
    std::cout << "--- Object Type Summary ---\n";
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
        
        std::cout << "  " << type_name << ": " << tc.second << "\n";
    }

    std::cout << "--- Detailed Leak Information ---\n";
    for (const auto &entry : tracked_objects)
    {
        const ObjectKey &key = entry.first;
        const ObjectDebugRecord &record = entry.second;

        std::cout << "[LEAK] Type=0x" << std::hex << static_cast<uint32_t>(key.type) << std::dec
                  << " Handle=0x" << std::hex << static_cast<unsigned long long>(key.handle) << std::dec
                  << " Name=" << record.name.c_str();
        
        if (!record.file.empty())
        {
            std::cout << " Created at " << record.file.c_str()
                      << ":" << record.line
                      << " in " << record.function.c_str() << "()";
        }
        std::cout << "\n";
    }

    std::cout << "========== [END LEAK DETECTION] ==========\n";
    std::cout.flush();  // 确保输出被立即刷新
}

void VulkanDevice::TrackBuffer(DeviceBuffer *buf, const ObjectNameBuilder &name, const std::source_location &loc)
{
    if (!buf)
        return;

    TrackObject(VK_OBJECT_TYPE_BUFFER, (uint64_t)(uintptr_t)buf->GetBuffer(), name.Append(ObjectTypeTag::Buffer), loc);
    TrackObject(VK_OBJECT_TYPE_DEVICE_MEMORY, (uint64_t)(uintptr_t)buf->GetVkMemory(), name.Append(ObjectTypeTag::Memory), loc);
}

void VulkanDevice::UntrackBuffer(DeviceBuffer *buf)
{
    if (!buf)
        return;

    UntrackObject(VK_OBJECT_TYPE_BUFFER, (uint64_t)(uintptr_t)buf->GetBuffer());
    // DeviceMemory will untrack itself in its destructor - don't double-untrack
    // UntrackObject(VK_OBJECT_TYPE_DEVICE_MEMORY, (uint64_t)(uintptr_t)buf->GetVkMemory());
}

void VulkanDevice::TrackTexture(Texture *tex, const ObjectNameBuilder &name, const std::source_location &loc)
{
    if (!tex)
        return;

    TrackObject(VK_OBJECT_TYPE_IMAGE, (uint64_t)(uintptr_t)tex->GetImage(), name.Append(ObjectTypeTag::Image), loc);
    TrackObject(VK_OBJECT_TYPE_IMAGE_VIEW, (uint64_t)(uintptr_t)tex->GetVulkanImageView(), name.Append(ObjectTypeTag::ImageView), loc);
    TrackObject(VK_OBJECT_TYPE_DEVICE_MEMORY, (uint64_t)(uintptr_t)tex->GetDeviceMemory(), name.Append(ObjectTypeTag::Memory), loc);
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
        TrackObject(VK_OBJECT_TYPE_COMMAND_BUFFER, (uint64_t)(uintptr_t)cb, name.Append(ObjectTypeTag::RenderCommandBuffer), loc);
    return result;
}

TextureCmdBuffer *VulkanDevice::CreateTextureCommandBuffer(const ObjectNameBuilder &name, const std::source_location &loc)
{
    VkCommandBuffer cb=CreateCommandBuffer(name.ToString());

    if(cb==VK_NULL_HANDLE)return(nullptr);

    TextureCmdBuffer *result = new TextureCmdBuffer(attr,cb);
    if (result)
        TrackObject(VK_OBJECT_TYPE_COMMAND_BUFFER, (uint64_t)(uintptr_t)cb, name.Append(ObjectTypeTag::TextureCommandBuffer), loc);
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

    TrackObject(VK_OBJECT_TYPE_FENCE, (uint64_t)(uintptr_t)fence, name.Append(ObjectTypeTag::Fence), loc);
    return(new Fence(attr->device,fence));
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
        TrackObject(VK_OBJECT_TYPE_SEMAPHORE, (uint64_t)(uintptr_t)sem, name.Append(ObjectTypeTag::Semaphore), loc);
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

VK_NAMESPACE_END
