#include<hgl/graph/SceneDB.h>
#include<hgl/graph/vulkan/VKDevice.h>
#include<hgl/graph/RenderableInstance.h>

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

        vulkan::IndexBuffer *SceneDB::CreateIBO(VkIndexType index_type,uint32_t count,const void *data,VkSharingMode sharing_mode)
        {
            vulkan::IndexBuffer *buf=device->CreateIBO(index_type,count,data,sharing_mode);

            if(!buf)return(nullptr);
            rm_buffers.Add(buf);
            return(buf);
        }

        RenderableInstance *SceneDB::CreateRenderableInstance(vulkan::Pipeline *p,vulkan::MaterialInstance *mi,vulkan::Renderable *r)
        {
            if(!p||!mi||!r)
                return(nullptr);

            RenderableInstance *ri=new RenderableInstance(p,mi,r);

            Add(ri);

            return ri;
        }
    }//namespace graph
}//namespace hgl
