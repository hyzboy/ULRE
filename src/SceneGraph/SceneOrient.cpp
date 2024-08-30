#include<hgl/graph/SceneOrient.h>
namespace hgl
{
    namespace graph
    {
        SceneOrient::SceneOrient(const SceneOrient &so)
        {
            scene_matrix=so.scene_matrix;

            scene_matrix.UpdateNewestData();
        }

        SceneOrient::SceneOrient(const Matrix4f &mat):SceneOrient()
        {
            scene_matrix.SetLocalMatrix(mat);
            
            scene_matrix.UpdateNewestData();
        }

        void SceneOrient::RefreshMatrix()
        {
            if (scene_matrix.IsNewestVersion())
            {
                //是最新版本，证明没有更新，那不用刷新了
                return;
            }
        }
    }//namespace graph
}//namespace hgl
