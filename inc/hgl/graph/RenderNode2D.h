﻿#ifndef HGL_GRAPH_RENDER_NODE_2D_INCLUDE
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

            RenderNode2DExtraBuffer *extra_buffer;

        protected:

            uint32_t binding_count;
            VkBuffer *buffer_list;
            VkDeviceSize *buffer_offset;

            MaterialInstance *last_mi;
            Pipeline *last_pipeline;
            Primitive *last_primitive;
            uint first_index;

            void Render(const uint index,Renderable *);

        public:

            MaterialRenderList2D(GPUDevice *d,RenderCmdBuffer *,Material *m);
            ~MaterialRenderList2D();

            void Add(Renderable *ri,const Matrix3x4f &mat);

            void ClearData()
            {
                rn_list.ClearData();
            }

            void End();

            void Render();
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
        };
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_RENDER_NODE_2D_INCLUDE
