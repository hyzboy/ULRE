#ifndef HGL_GRAPH_RENDER_LIST_2D_INCLUDE
#define HGL_GRAPH_RENDER_LIST_2D_INCLUDE

#include<hgl/graph/VK.h>
#include<hgl/graph/SceneNode.h>
#include<hgl/graph/RenderNode2D.h>
#include<hgl/graph/VKArrayBuffer.h>
#include<hgl/graph/VKMaterial.h>
namespace hgl
{
    namespace graph
    {
        /**
         * 渲染对象列表<br>
         * 已经展开的渲染对象列表，产生mvp用UBO/SSBO等数据，最终创建RenderCommandBuffer
         */
        class RenderList2D
        {
        protected:  

            GPUDevice *         device;
            RenderCmdBuffer *   cmd_buf;        

        private:

//            GPUArrayBuffer *    mvp_array;

            RenderNode2DList    render_node_list;       ///<场景节点列表
            MaterialSets        material_sets;          ///<材质合集

            RenderNode2DComparator render_node_comparator;

        private:

            List<Renderable *> ri_list;

            VkDescriptorSet ds_list[DESCRIPTOR_SET_TYPE_COUNT];
            DescriptorSet *renderable_desc_sets;

            uint32_t ubo_offset;
            uint32_t ubo_align;

        protected:

            virtual bool    Begin();
            virtual bool    ExpendNode(SceneNode *);
            virtual void    End();

        private:

            Pipeline *          last_pipeline;
            MaterialParameters *last_mp[DESCRIPTOR_SET_TYPE_COUNT];
            uint32_t            last_vbo;

            void Render(Renderable *);

        public:

            RenderList2D(GPUDevice *);
            virtual ~RenderList2D();
            
            virtual bool Expend(SceneNode *);

            virtual bool Render(RenderCmdBuffer *);
        };//class RenderList2D
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_RENDER_LIST_2D_INCLUDE
