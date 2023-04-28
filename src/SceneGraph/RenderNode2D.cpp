#include<hgl/graph/RenderNode2D.h>
#include<hgl/graph/VKRenderable.h>
#include<hgl/util/sort/Sort.h>

/**
* 
* 理论上讲，我们需要按以下顺序排序
*
*   for(material)
*       for(pipeline)
*           for(material_instance)
*               for(vbo)
*/

template<> 
int Comparator<hgl::graph::RenderNode2D>::compare(const hgl::graph::RenderNode2D &obj_one,const hgl::graph::RenderNode2D &obj_two) const
{
    int off;

    hgl::graph::Renderable *ri_one=obj_one.ri;
    hgl::graph::Renderable *ri_two=obj_two.ri;

    //比较管线
    {
        off=ri_one->GetPipeline()
           -ri_two->GetPipeline();

        if(off)
            return off;
    }

    //比较材质实例
    {
        off=ri_one->GetMaterialInstance()
           -ri_two->GetMaterialInstance();

        if(off)
            return off;
    }

    //比较vbo+ebo
    {
        off=ri_one->GetBufferHash()
           -ri_two->GetBufferHash();

        if(off)
            return off;
    }

    return 0;
}

namespace hgl
{
    namespace graph
    {
        void MaterialRenderList2D::Add(Renderable *ri,const Matrix3x4f &mat)
        {
            RenderNode2D rn;

            rn.local_to_world=mat;
            rn.ri=ri;

            rn_list.Add(rn);
        }

        void MaterialRenderList2D::End()
        {
            //排序
            {
                Comparator<hgl::graph::RenderNode2D> rnc;

                Sort(rn_list,&rnc);
            }

            //生成mvp数据
            {
                                
            }
        }
    }//namespace graph
}//namespace hgl
