#ifndef HGL_GRAPH_DESCRIPTOR_BINDING_MANAGE_INCLUDE
#define HGL_GRAPH_DESCRIPTOR_BINDING_MANAGE_INCLUDE

#include<hgl/type/Map.h>
#include<hgl/type/String.h>
#include<hgl/graph/VKDescriptorSetType.h>
namespace hgl
{
    namespace graph
    {
        class DeviceBuffer;
        class Texture;
        class Material;

        class DescriptorBinding
        {
            DescriptorSetType set_type;

            Map<AnsiString,DeviceBuffer *> ubo_map;
            Map<AnsiString,DeviceBuffer *> ssbo_map;
            Map<AnsiString,Texture *> texture_map;

        public:

            DescriptorBinding(const DescriptorSetType &dst)
            {
                set_type=dst;
            }

            bool AddUBO(const AnsiString &name,DeviceBuffer *buf)
            {
                if(!buf)return(false);
                if(name.IsEmpty())return(false);

                return ubo_map.Add(name,buf);
            }

            DeviceBuffer *GetUBO(const AnsiString &name)
            {
                if(name.IsEmpty())return(nullptr);

                return GetListObject(ubo_map,name);
            }

            void RemoveUBO(DeviceBuffer *buf)
            {
                if(!buf)return;

                ubo_map.DeleteByValue(buf);
            }

            bool AddSSBO(const AnsiString &name,DeviceBuffer *buf)
            {
                if(!buf)return(false);
                if(name.IsEmpty())return(false);

                return ssbo_map.Add(name,buf);
            }

            DeviceBuffer *GetSSBO(const AnsiString &name)
            {
                if(name.IsEmpty())return(nullptr);

                return GetListObject(ssbo_map,name);
            }

            void RemoveSSBO(DeviceBuffer *buf)
            {
                if(!buf)return;

                ssbo_map.DeleteByValue(buf);
            }

            bool AddTexture(const AnsiString &name,Texture *tex)
            {
                if(!tex)return(false);
                if(name.IsEmpty())return(false);

                return texture_map.Add(name,tex);
            }

            Texture *GetTexture(const AnsiString &name)
            {
                if(name.IsEmpty())return(nullptr);

                return GetListObject(texture_map,name);
            }

            void RemoveTexture(Texture *tex)
            {
                if(!tex)return;

                texture_map.DeleteByValue(tex);
            }

            bool Bind(Material *);
        };//class DescriptorBinding
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_DESCRIPTOR_BINDING_MANAGE_INCLUDE
