#include<hgl/graph/SceneOrient.h>
namespace hgl
{
    namespace graph
    {
        SceneOrient::SceneOrient(const SceneOrient &so)
        {
            scene_matrix=so.scene_matrix;
            WorldPosition=so.WorldPosition;
        }

        SceneOrient::SceneOrient(const Matrix4f &mat):SceneOrient()
        {
            scene_matrix.SetLocalMatrix(mat);

            WorldPosition=TransformPosition(mat,ZeroVector3f);
        }

        void SceneOrient::RefreshMatrix()
        {
            if (scene_matrix.IsNewestVersion())
            {
                //是最新版本，证明没有更新，那不用刷新了
                return;
            }

            const Matrix4f &l2w=scene_matrix.GetNewestVersionData();

            WorldPosition=TransformPosition(l2w,ZeroVector3f);
        }
    }//namespace graph
}//namespace hgl
