#include<hgl/graph/DrawNode.h>
#include<hgl/graph/VertexDataManager.h>
#include<hgl/graph/mesh/Primitive.h>
#include<hgl/component/PrimitiveComponent.h>
#include<hgl/graph/SceneNode.h>

VK_NAMESPACE_BEGIN
/**
* 见排序说明
*/
std::strong_ordering DrawNode::operator<=>(const DrawNode &other) const
{
    auto *prim_one = other.GetPrimitive()->GetGeometry();
    auto *prim_two = this->GetPrimitive()->GetGeometry();

    //比较VDM
    if(prim_one->GetVDM())      //有VDM
    {
        hgl::int64 vdm_off = (hgl::int64)prim_one->GetVDM()
                           - (hgl::int64)prim_two->GetVDM();

        if(vdm_off != 0)
            return vdm_off < 0 ? std::strong_ordering::less : std::strong_ordering::greater;

        //比较模型
        {
            hgl::int64 prim_off = (hgl::int64)prim_one
                                - (hgl::int64)prim_two;

            if(prim_off != 0)
            {
                //保证vertex offset小的在前面
                hgl::int64 offset_diff = prim_one->GetVertexOffset() - prim_two->GetVertexOffset();
                return offset_diff < 0 ? std::strong_ordering::less : std::strong_ordering::greater;
            }
        }
    }

    //比较距离（备注：正负方向等实际测试后再调整）
    float foff = other.to_camera_distance - to_camera_distance;

    if(foff > 0)
        return std::strong_ordering::greater;
    else if(foff < 0)
        return std::strong_ordering::less;
    else
        return std::strong_ordering::equal;
}

// DrawNode implementation
DrawNode::DrawNode(PrimitiveComponent *pc)
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

SceneNode *DrawNode::GetSceneNode() const
{
    return comp?comp->GetOwnerNode():nullptr;
}

MaterialInstance *DrawNode::GetMaterialInstance() const
{
    //PrimitiveComponent中含有OverrideMaterial的逻辑，所以不要直接从primitive中取MaterialInstance
    return comp?comp->GetMaterialInstance():nullptr;
}

NodeTransform *DrawNode::GetTransform() const
{
    return comp; // 直接使用组件自身的变换
}
VK_NAMESPACE_END
