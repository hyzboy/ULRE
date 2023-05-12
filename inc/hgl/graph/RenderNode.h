#ifndef HGL_GRAPH_RENDER_NODE_INCLUDE
#define HGL_GRAPH_RENDER_NODE_INCLUDE

#include<hgl/math/Math.h>
#include<hgl/type/Map.h>
#include<hgl/type/SortedSets.h>
#include<hgl/graph/VK.h>
namespace hgl
{
    namespace graph
    {
        class Renderable;
        class Material;
        class GPUDevice;
        struct VertexInputData;
        struct IndexBufferData;

        struct RenderNode
        {
            Matrix4f local_to_world;

            Renderable *ri;
        };

        using RenderNodeList=List<RenderNode>;

        struct RenderNodeExtraBuffer;

        /**
         * 同一材质的对象渲染列表
         */
        class MaterialRenderList
        {
            GPUDevice *device;
            RenderCmdBuffer *cmd_buf;

            Material *mtl;

            RenderNodeList rn_list;

        private:

            RenderNodeExtraBuffer *extra_buffer;

            struct RenderItem
            {
                        uint32_t            first;
                        uint32_t            count;

                        Pipeline *          pipeline;
                        MaterialInstance *  mi;
                const   VertexInputData *   vid;

            public:

                void Set(Renderable *);
            };

            SortedSets<MaterialInstance *> mi_set;
            List<RenderItem> ri_list;
            uint ri_count;

            void Stat();

        protected:

            uint32_t binding_count;
            VkBuffer *buffer_list;
            VkDeviceSize *buffer_offset;

                  MaterialInstance *  last_mi;
                  Pipeline *          last_pipeline;
            const VertexInputData *   last_vid;
                  uint                last_index;

            void Bind(MaterialInstance *);
            bool Bind(const VertexInputData *,const uint);

            void Render(RenderItem *);

        public:

            MaterialRenderList(GPUDevice *d,Material *m);
            ~MaterialRenderList();

            void Add(Renderable *ri,const Matrix4f &mat);

            void ClearData()
            {
                rn_list.ClearData();
            }

            void End();

            void Render(RenderCmdBuffer *);
        };

        class MaterialRenderMap:public ObjectMap<Material *,MaterialRenderList>
        {
        public:

            MaterialRenderMap()=default;
            virtual ~MaterialRenderMap()=default;

            void Begin()
            {
                for(auto *it:data_list)
                    it->value->ClearData();
            }

            void End()
            {
                for(auto *it:data_list)
                    it->value->End();
            }

            void Render(RenderCmdBuffer *rcb)
            {
                if(!rcb)return;

                for(auto *it:data_list)
                    it->value->Render(rcb);
            }
        };
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_RENDER_NODE_INCLUDE
