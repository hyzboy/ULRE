#include<hgl/graph/VK.h>
#include<hgl/filesystem/FileSystem.h>
#include<hgl/type/StringList.h>
#include<hgl/type/StrChar.h>

VK_NAMESPACE_BEGIN
namespace
{
    using namespace hgl::filesystem;

    const OSString GetUUIDCachePath(const VkPhysicalDeviceProperties &pdp)
    {
        OSString app_data;
        
        if(!GetLocalAppdataPath(app_data))return OS_TEXT("");

        OSStringList sl;

        sl.Add(app_data);
        sl.Add(OS_TEXT("VkPipelineCache.com"));
        sl.Add(OSString::valueOf(VK_PIPELINE_CACHE_HEADER_VERSION_ONE));
        sl.Add(OSString::valueOf(pdp.vendorID));
        sl.Add(OSString::valueOf(pdp.deviceID));

        return ComboFilename(sl);
    }

    void LoadPipelineCacheFile(VkPipelineCacheCreateInfo *pcci,const VkPhysicalDeviceProperties &pdp)
    {
        if(!pcci)return;

        const OSString pathname=GetUUIDCachePath(pdp);
        const OSString filename=VkUUID2String<os_char>(pdp.pipelineCacheUUID);
        const OSString fullname=MergeFilename(pathname,filename);

        if(!FileExist(fullname))
        {
            pcci->initialDataSize=0;
            pcci->pInitialData=nullptr;

            if(!IsDirectory(pathname))
            {
                MakePath(pathname);
                return;
            }
        }
        else
        {
            int64 size;
            pcci->pInitialData=LoadFileToMemory(fullname,size);
            pcci->initialDataSize=size;
        }
    }
}//namespace

VkPipelineCache CreatePipelineCache(VkDevice device,const VkPhysicalDeviceProperties &pdp)
{
    VkPipelineCacheCreateInfo pipelineCache;

    pipelineCache.sType             = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    pipelineCache.pNext             = nullptr;
    pipelineCache.flags             = 0;
    pipelineCache.initialDataSize   = 0;
    pipelineCache.pInitialData      = nullptr;

    LoadPipelineCacheFile(&pipelineCache,pdp);

    VkPipelineCache cache;

    VkResult result=vkCreatePipelineCache(device, &pipelineCache, nullptr, &cache);
    
    if(pipelineCache.pInitialData)
        delete[] (char *)pipelineCache.pInitialData;

    if(result==VK_SUCCESS)
        return cache;
    else
        return(VK_NULL_HANDLE);
}

void SavePipelineCacheData(VkDevice device,VkPipelineCache cache,const VkPhysicalDeviceProperties &pdp)
{
    size_t size = 0;
    AutoDeleteArray<char> data;
    
    if(vkGetPipelineCacheData(device, cache, &size, nullptr)!=VK_SUCCESS)
        return;

    data.alloc(size);

    if(!vkGetPipelineCacheData(device, cache, &size, data)==VK_SUCCESS)
        return;

    const OSString pathname=GetUUIDCachePath(pdp);
    const OSString filename=VkUUID2String<os_char>(pdp.pipelineCacheUUID);
    const OSString fullname=MergeFilename(pathname,filename);
        
    if(!IsDirectory(pathname))
        if(!MakePath(pathname))
            return;

    SaveMemoryToFile(fullname,data,size);
}
VK_NAMESPACE_END
