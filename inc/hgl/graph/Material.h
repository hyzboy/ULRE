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
         *  假设
         *  scene
         *  {
         *      mesh
         *      {
         *          Vertex
         *          TexCoordColor
         *          TexCoordNormal
         *      }
         *
         *      texture
         *      {
         *          Color
         *          Normal
         *      }
         *  };
         *
         *  有多种材质
         *
         *  material depth_render           //只渲染深度，用于shadow maps或oq等
         *  {
         *      in_vertex_attrib
         *      {
         *          Vertex
         *      }
         *
         *      in_uniform
         *      {
         *          mvp
         *      }
         *  }
         *
         *  material normal_render
         *  {
         *      in_vertex_attrib
         *      {
         *          Vertex,
         *          TexCoordColor,
         *          TexCoordNormal,
         *      }
         *
         *      in_uniform
         *      {
         *          mvp
         *          light
         *      }
         *  }
         */


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
