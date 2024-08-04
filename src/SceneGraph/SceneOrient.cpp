#include<hgl/graph/SceneOrient.h>
namespace hgl
{
    namespace graph
    {
        SceneOrient::SceneOrient()
        {
            Position=Vector3f(0.0f);
            Direction=Vector3f(0.0f);
        }
        
        SceneOrient::SceneOrient(const SceneOrient &so)
        {
            hgl_cpy(*this,so);
        }

        SceneOrient::SceneOrient(const Transform &t)
        {
            SetLocalTransform(t);
        }

        void SceneOrient::SetLocalTransform(const Transform &t)
        {
            if(LocalTransform==t)
                return;

            LocalTransform=t;
        }

        void SceneOrient::SetWorldTransform(const Transform &t)
        {
            if(WorldTransform==t)
                return;

            WorldTransform=t;
        }

        /**
         * 刷新世界变换
         * @param m 上一级local to world变换
         */
        bool SceneOrient::RefreshTransform(const Transform &t)
        {
            if(!t.IsLastVersion())      //都不是最新版本
                return(false);

            //理论上讲，Transform在正常转const的情况下，就已经做了UpdateMatrix()的操作，这个需要测试一下

            if(LocalTransform.IsIdentity())
            {
                SetWorldTransform(t);
            }
            else
            {
                SetWorldTransform(t.TransformTransform(LocalTransform));
            }

            return(true);
        }
    }//namespace graph
}//namespace hgl
