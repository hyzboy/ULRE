#ifndef HGL_GRAPH_RENDER_NODE_2D_INCLUDE
#define HGL_GRAPH_RENDER_NODE_2D_INCLUDE

#include<hgl/math/Math.h>
#include<hgl/type/Map.h>
namespace hgl
{
    namespace graph
    {
        class Renderable;
        class Material;

        struct RenderNode2D
        {
            Matrix3x4f local_to_world;

            Renderable *ri;
        };

        using RenderNode2DList=List<RenderNode2D *>;

        /**
         * 同一材质的对象渲染列表
         */
        struct MaterialRenderList2D
        {
            Material *mtl;

            RenderNode2DList rn_list;
        };

        class MaterialRenderMap2D:public ObjectMap<Material *,MaterialRenderList2D>
        {
        public:

            MaterialRenderMap2D()=default;
            virtual ~MaterialRenderMap2D()=default;

            void ClearData()
            {
                for(auto *it:data_list)
                    it->value->rn_list.ClearData();
            }
        };
    }//namespace graph
}//namespace hgl

using RenderNode2DPointer=hgl::graph::RenderNode2D *;
using RenderNode2DComparator=Comparator<RenderNode2DPointer>;
#endif//HGL_GRAPH_RENDER_NODE_2D_INCLUDE
