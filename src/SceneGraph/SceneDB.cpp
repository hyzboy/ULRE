#include<hgl/graph/SceneDB.h>
#include<hgl/graph/vulkan/VKDevice.h>

namespace hgl
{
    namespace graph
    {
        vulkan::VertexBuffer *SceneDB::CreateVBO(VkFormat format,uint32_t count,const void *data,VkSharingMode sharing_mode)
        {
            vulkan::VertexBuffer *vb=device->CreateVBO(format,count,data,sharing_mode);

            if(!vb)
                return(nullptr);

            rm_buffers.Add(vb);

            return vb;
        }

        #define SCENE_DB_CREATE_BUFFER(name)    vulkan::Buffer *SceneDB::Create##name(VkDeviceSize size,void *data,VkSharingMode sharing_mode) \
                                                {   \
                                                    vulkan::Buffer *buf=device->Create##name(size,data,sharing_mode);   \
                                                    \
                                                    if(!buf)return(nullptr);    \
                                                    rm_buffers.Add(buf);    \
                                                    return(buf);    \
                                                }   \
                                                \
                                                vulkan::Buffer *SceneDB::Create##name(VkDeviceSize size,VkSharingMode sharing_mode)    \
                                                {   \
                                                    vulkan::Buffer *buf=device->Create##name(size,sharing_mode);    \
                                                    \
                                                    if(!buf)return(nullptr);    \
                                                    rm_buffers.Add(buf);    \
                                                    return(buf);    \
                                                }

            SCENE_DB_CREATE_BUFFER(UBO)
            SCENE_DB_CREATE_BUFFER(SSBO)
            SCENE_DB_CREATE_BUFFER(INBO)

        #undef SCENE_DB_CREATE_BUFFER
    }//namespace graph
}//namespace hgl
