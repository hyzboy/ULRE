#ifndef HGL_GRAPH_RENDER_NODE_2D_INCLUDE
#define HGL_GRAPH_RENDER_NODE_2D_INCLUDE

#include<hgl/math/Math.h>
#include<hgl/type/Map.h>
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

        struct RenderNode2D
        {
            Matrix3x4f local_to_world;

            Renderable *ri;
        };

        using RenderNode2DList=List<RenderNode2D>;

        struct RenderNode2DExtraBuffer;

        /**
         * 同一材质的对象渲染列表
         */
        class MaterialRenderList2D
        {
            GPUDevice *device;
            RenderCmdBuffer *cmd_buf;

            Material *mtl;

            RenderNode2DList rn_list;

        private:

            RenderNode2DExtraBuffer *extra_buffer;

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

            MaterialRenderList2D(GPUDevice *d,Material *m);
            ~MaterialRenderList2D();

            void Add(Renderable *ri,const Matrix3x4f &mat);

            void ClearData()
            {
                rn_list.ClearData();
            }

            void End();

            void Render(RenderCmdBuffer *);
        };

        class MaterialRenderMap2D:public ObjectMap<Material *,MaterialRenderList2D>
        {
        public:

            MaterialRenderMap2D()=default;
            virtual ~MaterialRenderMap2D()=default;

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
#endif//HGL_GRAPH_RENDER_NODE_2D_INCLUDE
