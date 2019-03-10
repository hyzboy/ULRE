#ifndef HGL_RENDER_STATE_INCLUDE
#define HGL_RENDER_STATE_INCLUDE

#include<hgl/type/Color4f.h>
#include<hgl/CompOperator.h>
namespace hgl
{
    enum DEPTH_TEST_FUNC
    {
        DEPTH_TEST_NEVER=0,
        DEPTH_TEST_LESS,
        DEPTH_TEST_EQUAL,
        DEPTH_TEST_LEQUAL,
        DEPTH_TEST_GREATER,
        DEPTH_TEST_NOTEQUAL,
        DEPTH_TEST_GEQUAL,
        DEPTH_TEST_ALWAYS
    };//

    struct DepthStatus
    {
        float           near_depth  =0,
                        far_depth   =1;

        bool            depth_mask  =true;
        float           clear_depth =far_depth;

        DEPTH_TEST_FUNC depth_func  =DEPTH_TEST_LESS;
        bool            depth_test;

    public:

        CompOperatorMemcmp(struct DepthStatus &);
    };//

    /**
     * 渲染状态
     */
    struct RenderState
    {
        bool    color_mask[4];
        Color4f clear_color;

        DepthStatus depth;
        //             uint		draw_face;																	///<绘制的面
        //             uint		fill_mode;																	///<填充方式
        //
        //             uint		cull_face;																	///<裁剪面,0:不裁(双面材质),FACE_FRONT:正面,FACE_BACK:背面
        //
        //             bool		depth_test;																	///<深度测试
        //             uint		depth_func;																	///<深度处理方式
        //             bool		depth_mask;																	///<深度遮罩
    };//class RenderState
}//namespace hgl
#endif//HGL_RENDER_STATE_INCLUDE
