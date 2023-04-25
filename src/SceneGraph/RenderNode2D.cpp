#include<hgl/graph/RenderNode2D.h>
#include<hgl/graph/VKRenderable.h>
#include<hgl/util/sort/Sort.h>

/**
* 
* �����Ͻ���������Ҫ������˳������
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

    //�ȽϹ���
    {
        off=ri_one->GetPipeline()
           -ri_two->GetPipeline();

        if(off)
            return off;
    }

    //�Ƚϲ���ʵ��
    //{
    //    for(int i =(int)hgl::graph::DescriptorSetType::BEGIN_RANGE;
    //            i<=(int)hgl::graph::DescriptorSetType::END_RANGE;
    //            i++)
    //    {
    //        off=ri_one->GetMP((hgl::graph::DescriptorSetType)i)
    //           -ri_two->GetMP((hgl::graph::DescriptorSetType)i);

    //        if(off)
    //            return off;
    //    }
    //}

    //�Ƚ�vbo+ebo
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
            Comparator<hgl::graph::RenderNode2D> rnc;

            Sort(rn_list,&rnc);

            
        }
    }//namespace graph
}//namespace hgl
