#include<hgl/vk/VKShaderModule.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/util/hash/FNV1a.h>
#include<cstdint>

namespace hgl::graph{
struct ShaderModuleCreateInfo:public vkstruct_flag<VkShaderModuleCreateInfo,VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO>
{
public:

    ShaderModuleCreateInfo(const uint32_t *spv_data,const size_t spv_size)
    {
        codeSize=spv_size;
        pCode   =spv_data;
    }
};//struct ShaderModuleCreateInfo

// ── ShaderModule SPIRV 内容 hash 注册表 ──────────────────────────────────────
// pipeline 缓存键（FinalPipelineKey::shader_stages_hash）必须建立在 **shader 内容
// 身份**上：VkShaderModule 句柄值在模块销毁后可被新建模块复用（与已修复的
// resolvedRuntimePipelineMap 无 program 键控同构），用句柄做键会把不同的 shader
// 判成同一个 pipeline → 复用错误管线。模块创建时登记、析构时注销（D2）。
void VulkanDevice::RegisterShaderModuleHash(VkShaderModule module, uint64_t content_hash)
{
    if(module == VK_NULL_HANDLE)
        return;

    ThreadMutexLock lock(&shader_module_hashes_mutex);

    shader_module_hashes[(uint64_t)(uintptr_t)module] = content_hash;
}

void VulkanDevice::UnregisterShaderModuleHash(VkShaderModule module)
{
    if(module == VK_NULL_HANDLE)
        return;

    ThreadMutexLock lock(&shader_module_hashes_mutex);

    shader_module_hashes.erase((uint64_t)(uintptr_t)module);
}

uint64_t VulkanDevice::GetShaderModuleHash(VkShaderModule module)const
{
    if(module == VK_NULL_HANDLE)
        return 0;

    ThreadMutexLock lock(&shader_module_hashes_mutex);

    const auto it = shader_module_hashes.find((uint64_t)(uintptr_t)module);
    return it == shader_module_hashes.end() ? 0 : it->second;
}

ShaderModule *VulkanDevice::CreateShaderModule(VkShaderStageFlagBits shader_stage_flag_bit,const uint32_t *spv_data,const size_t spv_size)
{
    if(!spv_data||spv_size<4)return(nullptr);

    PipelineShaderStageCreateInfo *pss_ci=new PipelineShaderStageCreateInfo(shader_stage_flag_bit);

    ShaderModuleCreateInfo moduleCreateInfo(spv_data,spv_size);

    if(vkCreateShaderModule(attr->device,&moduleCreateInfo,nullptr,&(pss_ci->module))!=VK_SUCCESS)
        return(nullptr);

    hgl::hash::FNV1aHasher64 hasher;
    hasher.AppendBytes(spv_data,spv_size);
    const uint64_t content_hash = hasher;

    RegisterShaderModuleHash(pss_ci->module, content_hash);

    return(new ShaderModule(attr->device,pss_ci,content_hash));
}

ShaderModule::ShaderModule(VkDevice dev,VkPipelineShaderStageCreateInfo *sci,uint64_t content_hash)
{
    device=dev;
    ref_count=0;
    spv_content_hash=content_hash;

    stage_create_info=sci;
}

ShaderModule::~ShaderModule()
{
    VulkanDevice *owner = VulkanDevice::FromDevice(device);
    if (owner)
    {
        owner->UnregisterShaderModuleHash(stage_create_info->module);
        owner->UntrackObject(VK_OBJECT_TYPE_SHADER_MODULE, (uint64_t)(uintptr_t)stage_create_info->module);
    }

    vkDestroyShaderModule(device,stage_create_info->module,nullptr);
    //这里不用删除stage_create_info，材质中会删除的
}
}//namespace hgl::graph
