#ifndef HGL_GRAPH_RENDER_LIST_INCLUDE
#define HGL_GRAPH_RENDER_LIST_INCLUDE

#include<hgl/graph/VK.h>
#include<hgl/graph/Camera.h>
#include<hgl/graph/SceneNode.h>
#include<hgl/graph/RenderNode.h>
#include<hgl/graph/VKArrayBuffer.h>
#include<hgl/graph/SceneInfo.h>
#include<hgl/color/Color4f.h>
#include<hgl/graph/VKMaterial.h>
namespace hgl
{
    namespace graph
    {
        /**
         * 渲染对象列表<br>
         * 已经展开的渲染对象列表，产生mvp用UBO/SSBO等数据，最终创建RenderCommandBuffer
         */
        class RenderList
        {
        protected:

            GPUDevice *         device;
            RenderCmdBuffer *   cmd_buf;

        private:

            GPUArrayBuffer *    mvp_array;
            CameraInfo          camera_info;

            RenderNode3DList    render_node_list;       ///<场景节点列表
            MaterialSets        material_sets;          ///<材质合集

            RenderNode3DComparator *render_node_comparator;

        private:

            List<Renderable *> ri_list;

            VkDescriptorSet ds_list[DESCRIPTOR_SET_TYPE_COUNT];
            DescriptorSet *renderable_desc_sets;

            uint32_t ubo_offset;
            uint32_t ubo_align;

        protected:

            virtual bool    Begin();
            virtual bool    Expend(SceneNode *);
            virtual void    End();

        private:

            Pipeline *          last_pipeline;
            MaterialParameters *last_mp[DESCRIPTOR_SET_TYPE_COUNT];
            uint32_t            last_vbo;

            void Render(Renderable *);

        public:

            RenderList(GPUDevice *);
            virtual ~RenderList();

            virtual bool Expend(const CameraInfo &,SceneNode *);

            virtual bool Render(RenderCmdBuffer *);
        };//class RenderList
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_RENDER_LIST_INCLUDE
