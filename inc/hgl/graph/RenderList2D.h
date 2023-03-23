#ifndef HGL_GRAPH_RENDER_LIST_INCLUDE
#define HGL_GRAPH_RENDER_LIST_INCLUDE

#include<hgl/graph/VK.h>
#include<hgl/graph/Camera.h>
#include<hgl/graph/SceneNode.h>
#include<hgl/graph/RenderNode.h>
#include<hgl/graph/VKArrayBuffer.h>
#include<hgl/graph/SceneInfo.h>
#include<hgl/color/Color4f.h>
#include<hgl/type/SortedSets.h>
namespace hgl
{
    namespace graph
    {
        using MaterialSets=SortedSets<Material *>;

        /**
         * 渲染对象列表<br>
         * 已经展开的渲染对象列表，产生mvp用UBO/SSBO等数据，最终创建RenderCommandBuffer
         */
        class RenderList
        {
        protected:  

            GPUDevice *     device;
            RenderCmdBuffer *cmd_buf;

        private:

            RenderNodeList  render_node_list;       ///<场景节点列表
            MaterialSets    material_sets;          ///<材质合集

            RenderNodeComparator *render_node_comparator;

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

        class RenderList2D:public RenderList
        {

        protected:

            virtual bool    Begin() override;
            virtual bool    Expend(SceneNode *) override;
            virtual void    End() override;

        public:

            RenderList2D();
            virtual ~RenderList2D() override;

            virtual bool Expend(SceneNode *);
        };

        class RenderList3D:public RenderList
        {
        protected:

            CameraInfo      camera_info;            
            GPUArrayBuffer *mvp_array;

        protected:

            virtual bool    Begin() override;
            virtual bool    Expend(SceneNode *) override;
            virtual void    End() override;

        public:

            RenderList3D();
            virtual ~RenderList3D() override;

            bool Expend(const CameraInfo &,SceneNode *);
        };
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_RENDER_LIST_INCLUDE
