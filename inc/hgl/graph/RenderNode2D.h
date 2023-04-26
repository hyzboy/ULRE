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

        using RenderNode2DList=List<RenderNode2D>;

        /**
         * 同一材质的对象渲染列表
         */
        class MaterialRenderList2D
        {
            Material *mtl;

            RenderNode2DList rn_list;

        public:

            MaterialRenderList2D(Material *m)
            {
                mtl=m;
            }

            void Add(Renderable *ri,const Matrix3x4f &mat);

            void ClearData()
            {
                rn_list.ClearData();
            }

            void End();
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
