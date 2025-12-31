#pragma once
#include<hgl/type/String.h>
#include<hgl/graph/VKNamespace.h>

VK_NAMESPACE_BEGIN
template<typename T>
inline hgl::String<T> VkUUID2String(const hgl::uint8 *pipelineCacheUUID)
{
    constexpr const size_t UUID_SIZE=16;

    String<T> uuid_string;

    T *hstr=uuid_string.Resize(UUID_SIZE*2);
    
    DataToLowerHexStr(hstr,pipelineCacheUUID,UUID_SIZE);

    return uuid_string;
}
VK_NAMESPACE_END
