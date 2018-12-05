#ifndef HGL_GRAPH_MATERIAL_INCLUDE
#define HGL_GRAPH_MATERIAL_INCLUDE

#include<hgl/type/Color4f.h>
#include<hgl/graph/Shader.h>
#include<hgl/graph/Axis.h>
#include<hgl/graph/BlendMode.h>

namespace hgl
{
    namespace graph
    {
        /**
         * 渲染材质
         */
        class Material
        {
        protected:

            Shader *shader;

        protected:

//             Color4f		Color;																		///<颜色
//
//             uint		draw_face;																	///<绘制的面
//             uint		fill_mode;																	///<填充方式
//
//             uint		cull_face;																	///<裁剪面,0:不裁(双面材质),FACE_FRONT:正面,FACE_BACK:背面
//
//             bool		depth_test;																	///<深度测试
//             uint		depth_func;																	///<深度处理方式
//             bool		depth_mask;																	///<深度遮罩
//
//             float		alpha_test;																	///<alpha测试值,0:全部不显示,1:全部显示
//
//             bool		outside_discard;															///<贴图出界放弃
//
//             bool		smooth;																		///<是否平滑(未用)
//
//             bool		color_material;																///<是否使用颜色追踪材质
//
//             bool		blend;																		///<是否开启混合
//
//             BlendMode	blend_mode;																	///<混合模式
//
//             Axis		up_axis;																    ///<高度图向上轴

//             struct
//             {
//                 Color4f		Emission;																///<散射光
//                 Color4f		Ambient;																///<环境光
//                 Color4f		Diffuse;																///<漫反射
//                 Color4f		Specular;																///<镜面光
//                 float		Shininess;																///<镜面指数
//             }front,back;

        public:
        };//class Material
    }//namespace graph
}//namespace hgl
#endif//HGL_GRAPH_MATERIAL_INCLUDE
