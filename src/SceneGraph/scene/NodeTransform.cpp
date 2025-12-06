#include<hgl/graph/NodeTransform.h>
namespace hgl
{
    namespace graph
    {
        NodeTransform::NodeTransform(const NodeTransform &so)
        {
            transform_state=so.transform_state;

            transform_state.UpdateNewestData();
        }

        NodeTransform::NodeTransform(const math::Matrix4f &mat):NodeTransform()
        {
            transform_state.SetLocalMatrix(mat);
            
            transform_state.UpdateNewestData();
        }

        void NodeTransform::UpdateWorldTransform()
        {
            transform_state.Update();
        }
    }//namespace graph
}//namespace hgl
