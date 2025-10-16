#include<hgl/graph/DrawNode.h>
#include<hgl/graph/VertexDataManager.h>
#include<hgl/graph/mesh/Primitive.h>
#include<hgl/component/PrimitiveComponent.h>
#include<hgl/graph/SceneNode.h>

VK_NAMESPACE_BEGIN
/**
* 见排序说明
*/
const int DrawNode::compare(const DrawNode &other)const
{
    hgl::int64 off;

    auto *prim_one=other.GetPrimitive()->GetGeometry();
    auto *prim_two=this->GetPrimitive()->GetGeometry();

    //比较VDM
    if(prim_one->GetVDM())      //有VDM
    {
        off=prim_one->GetVDM()
           -prim_two->GetVDM();

        if(off)
            return off;

        //比较模型
        {
            off=(hgl::int64)prim_one
               -(hgl::int64)prim_two;

            if(off)
            {
                off=prim_one->GetVertexOffset()-prim_two->GetVertexOffset();        //保证vertex offset小的在前面

                return off;
            }
        }
    }

    //比较距离（备注：正负方向等实际测试后再调整）
    float foff=other.to_camera_distance-to_camera_distance;

    if(foff>0)
        return 1;
    else
        return -1;
}

// DrawNodePrimitive
DrawNodePrimitive::DrawNodePrimitive(PrimitiveComponent *pc)
{
    if(pc)
    {
        comp=pc;
        primitive=pc->GetPrimitive();
    }
    else
    {
        comp=nullptr;
        primitive=nullptr;
    }
}

SceneNode *DrawNodePrimitive::GetSceneNode() const
{
    return comp?comp->GetOwnerNode():nullptr;
}

PrimitiveComponent *DrawNodePrimitive::GetPrimitiveComponent() const
{
    return comp;
}

MaterialInstance *DrawNodePrimitive::GetMaterialInstance() const
{
    //PrimitiveComponent中含有OverrideMaterial的逻辑，所以不要直接从primitive中取MaterialInstance
    return comp?comp->GetMaterialInstance():nullptr;
}

NodeTransform *DrawNodePrimitive::GetTransform() const
{
    return comp; // 直接使用组件自身的变换
}
VK_NAMESPACE_END
