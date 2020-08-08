#include<hgl/graph/SceneDB.h>
#include<hgl/graph/vulkan/VKDevice.h>
#include<hgl/graph/RenderableInstance.h>

namespace hgl
{
    namespace graph
    {
        vulkan::VAB *SceneDB::CreateVAB(VkFormat format,uint32_t count,const void *data,VkSharingMode sharing_mode)
        {
            vulkan::VAB *vb=device->CreateVAB(format,count,data,sharing_mode);

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
        
        vulkan::MaterialInstance *SceneDB::CreateMaterialInstance(vulkan::Material *mtl)
        {
            if(!mtl)return(nullptr);

            vulkan::MaterialInstance *mi=mtl->CreateInstance();

            if(mi)
                Add(mi);

            return mi;
        }

        vulkan::Renderable *SceneDB::CreateRenderable(vulkan::Material *mtl,const uint32_t vertex_count)
        {
            if(!mtl)return(nullptr);

            vulkan::Renderable *ro=mtl->CreateRenderable(vertex_count);

            if(ro)
                Add(ro);

            return ro;
        }

        TextRenderable *SceneDB::CreateTextRenderable(vulkan::Material *mtl)
        {
            if(!mtl)return(nullptr);
            
            TextRenderable *tr=new TextRenderable(device,mtl);

            if(tr)
                Add(tr);

            return tr;
        }

        RenderableInstance *SceneDB::CreateRenderableInstance(vulkan::Pipeline *p,vulkan::MaterialInstance *mi,vulkan::Renderable *r)
        {
            if(!p||!mi||!r)
                return(nullptr);

            RenderableInstance *ri=new RenderableInstance(p,mi,r);

            if(ri)
                Add(ri);

            return ri;
        }
        
        vulkan::Sampler *SceneDB::CreateSampler(VkSamplerCreateInfo *sci)
        {
            vulkan::Sampler *s=device->CreateSampler(sci);

            if(s)
                Add(s);

            return s;
        }
    }//namespace graph
}//namespace hgl
