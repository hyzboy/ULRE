#ifndef HGL_GRAPH_RENDER_LIST_INCLUDE
#define HGL_GRAPH_RENDER_LIST_INCLUDE

#include<hgl/graph/VK.h>
#include<hgl/graph/Camera.h>
#include<hgl/graph/SceneNode.h>
#include<hgl/graph/VKArrayBuffer.h>
#include<hgl/graph/MVPMatrix.h>
#include<hgl/type/Color4f.h>
#include<hgl/type/Sets.h>
namespace hgl
{
    namespace graph
    {
        using SceneNodeList=List<SceneNode *>;        
        using MVPArrayBuffer=GPUArrayBuffer<MVPMatrix>;
        using MVPOffsetBuffer=List<uint32_t>;

        /**
         * 渲染对象列表<br>
         * 已经展开的渲染对象列表，产生mvp用UBO/SSBO等数据，最终创建RenderCommandBuffer
         */
        class RenderList
        {
            GPUDevice *device;
            RenderCmdBuffer *cmd_buf;

        private:

            Camera *camera;

            SceneNodeList scene_node_list;

            MVPArrayBuffer *mvp_array;
            MVPOffsetBuffer mvp_offset;

        private:

            Pipeline *          last_pipeline;
            RenderableInstance *last_ri;

            void Render(SceneNode *,RenderableInstance *);
            void Render(SceneNode *,List<RenderableInstance *> &);

        private:

            friend class GPUDevice;

            RenderList(GPUDevice *);

        public:

            virtual ~RenderList();
            
            void Begin  ();
            void Add    (SceneNode *);
            void End    (CameraMatrix *);

            bool Render (RenderCmdBuffer *);
        };//class RenderList        
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_RENDER_LIST_INCLUDE
