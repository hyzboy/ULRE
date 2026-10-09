#include<hgl/vk/pipeline/VKGraphicsPipelineBackend.h>
#include<hgl/vk/pipeline/VKGraphicsPipelineLibraryBackend.h>
#include<hgl/vk/pipeline/VKMonolithicPipelineBackend.h>
#include<hgl/vk/pipeline/VKShaderObjectPipelineBackend.h>
#include<hgl/vk/VKDevice.h>
#include<hgl/util/hash/FNV1a.h>
#include<hgl/log/Log.h>

namespace hgl::graph
{
    GraphicsPipelineBackend *GraphicsPipelineBackend::Get(const GraphicsPipelineMode mode)
    {
        switch(mode)
        {
        case GraphicsPipelineMode::Monolithic:
            return &MonolithicPipelineBackend::Instance();
        case GraphicsPipelineMode::GraphicsPipelineLibrary:
            return &GraphicsPipelineLibraryBackend::Instance();
        case GraphicsPipelineMode::ShaderObject:
            return &ShaderObjectPipelineBackend::Instance();
        default:
            return nullptr;
        }
    }

    uint64_t GraphicsPipelineBackend::HashBytes(const void *data, const size_t size)
    {
        hgl::hash::FNV1aHasher64 hasher;
        hasher.AppendBytes(data, size);
        return hasher;
    }

    uint64_t GraphicsPipelineBackend::HashShaderStages(VulkanDevice *device,
                                                        const ShaderStageCreateInfoList *shader_stages,
                                                        const VkShaderStageFlags stage_filter)
    {
        if(!device || !shader_stages || shader_stages->IsEmpty())
            return 0;

        const uint count = shader_stages->GetCount();
        const VkPipelineShaderStageCreateInfo *stages = shader_stages->GetData();
        uint filtered_count = 0;

        for(uint i = 0; i < count; ++i)
            if(stage_filter == 0 || (stages[i].stage & stage_filter) != 0)
                ++filtered_count;

        if(filtered_count == 0)
            return 0;

        hgl::hash::FNV1aHasher64 hasher;
        hasher << filtered_count;

        for(uint i = 0; i < count; ++i)
        {
            if(stage_filter != 0 && (stages[i].stage & stage_filter) == 0)
                continue;

            const uint64_t module_hash = device->GetShaderModuleHash(stages[i].module);
            if(module_hash == 0)
            {
                GLogError("[PipelineResolver] shader module 0x%llx (stage 0x%x) has no registered SPIRV hash"
                          " -- not created via VulkanDevice::CreateShaderModule? pipeline key cannot be built",
                          (unsigned long long)(uintptr_t)stages[i].module,
                          (unsigned)stages[i].stage);
                return 0;
            }

            hasher << stages[i].stage
                   << stages[i].flags
                   << module_hash;
            if(stages[i].pName)
                hasher << stages[i].pName;

            const VkSpecializationInfo *specialization = stages[i].pSpecializationInfo;
            hasher << static_cast<uint32_t>(specialization != nullptr);
            if(specialization)
            {
                if((specialization->mapEntryCount > 0 && !specialization->pMapEntries)
                || (specialization->dataSize > 0 && !specialization->pData))
                    return 0;

                hasher << specialization->mapEntryCount;
                for(uint32_t entry_index = 0; entry_index < specialization->mapEntryCount; ++entry_index)
                {
                    const VkSpecializationMapEntry &entry = specialization->pMapEntries[entry_index];
                    hasher << entry.constantID << entry.offset << entry.size;
                }

                hasher << specialization->dataSize;
                if(specialization->dataSize > 0)
                    hasher.AppendBytes(specialization->pData, specialization->dataSize);
            }
        }

        return hasher;
    }
}
