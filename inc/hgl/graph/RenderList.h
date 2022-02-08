#ifndef HGL_GRAPH_RENDER_LIST_INCLUDE
#define HGL_GRAPH_RENDER_LIST_INCLUDE

#include<hgl/graph/VK.h>
#include<hgl/graph/Camera.h>
#include<hgl/graph/SceneNode.h>
#include<hgl/graph/RenderNode.h>
#include<hgl/graph/VKArrayBuffer.h>
#include<hgl/graph/SceneInfo.h>
#include<hgl/type/Color4f.h>
#include<hgl/type/SortedSets.h>
namespace hgl
{
    namespace graph
    {
        using MVPArrayBuffer=GPUArrayBuffer<MVPMatrix>;
        using MaterialSets=SortedSets<Material *>;

        /**
         * 渲染对象列表<br>
         * 已经展开的渲染对象列表，产生mvp用UBO/SSBO等数据，最终创建RenderCommandBuffer
         */
        class RenderList
        {
            GPUDevice *     device;
            RenderCmdBuffer *cmd_buf;

        private:

            CameraInfo      camera_info;
            
            RenderNodeList  render_node_list;       ///<场景节点列表
            MaterialSets    material_sets;          ///<材质合集

            RenderNodeComparator render_node_comparator;

        private:

            MVPArrayBuffer *mvp_array;
            List<RenderableInstance *> ri_list;

            VkDescriptorSet ds_list[(size_t)DescriptorSetsType::RANGE_SIZE];
            DescriptorSets *renderable_desc_sets;

            uint32_t ubo_offset;
            uint32_t ubo_align;

        protected:

            virtual bool    Begin();
            virtual bool    Expend(SceneNode *);
            virtual void    End();

        private:

            Pipeline *          last_pipeline;
            MaterialParameters *last_mp[(size_t)DescriptorSetsType::RANGE_SIZE];
            uint32_t            last_vbo;

            void Render(RenderableInstance *);

        public:

            RenderList(GPUDevice *);
            virtual ~RenderList();
            
            virtual bool Expend(const CameraInfo &,SceneNode *);

            virtual bool Render(RenderCmdBuffer *);
        };//class RenderList        
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_RENDER_LIST_INCLUDE
