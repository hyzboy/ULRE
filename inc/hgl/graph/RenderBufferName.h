#pragma once

#include<hgl/graph/VK.h>

VK_NAMESPACE_BEGIN

const RENDER_BUFFER_NAME RenderBuffer_Swapchain     ="Swapchain";

const RENDER_BUFFER_NAME RenderBuffer_Depth         ="Depth";
const RENDER_BUFFER_NAME RenderBuffer_Stencil       ="Stencil";

const RENDER_BUFFER_NAME VBuffer_MeshID             ="VB_MeshID";               ///< 所绘制的Mesh编号, RGBA8UI格式
const RENDER_BUFFER_NAME VBuffer_TriangleID         ="VB_TriangleID";           ///< 三角形编号, R8UI格式
const RENDER_BUFFER_NAME VBuffer_MaterialID         ="VB_MaterialID";           ///< 材质ID与材质实例ID, RG8UI格式

const RENDER_BUFFER_NAME GBuffer_BaseColor          ="GB_BaseColor";            ///<基础颜色(RGB565/RGB5A1/RGBA8/RGB10A2/RG11B10,RGBA16F)
const RENDER_BUFFER_NAME GBuffer_Luminance          ="GB_Luminance";            ///<亮度(R8/R16F/R32F)
const RENDER_BUFFER_NAME GBuffer_CbCr               ="GB_CbCr";                 ///<色度(RG8/RG16F)

const RENDER_BUFFER_NAME GBuffer_Normal             ="GB_Normal";               ///<法线(RG8,RG16F)
const RENDER_BUFFER_NAME GBuffer_BentNormal         ="GB_BentNormal";           ///<弯曲法线(RG8,RG16F)

const RENDER_BUFFER_NAME GBuffer_Metallic           ="GB_Metallic";             ///<金属度(R8/R16F/R32F)
const RENDER_BUFFER_NAME GBuffer_Roughness          ="GB_Roughness";            ///<粗糙度(R8/R16F/R32F)
const RENDER_BUFFER_NAME GBuffer_Specular           ="GB_Specular";             ///<高光(R8/R16F/R32F)
const RENDER_BUFFER_NAME GBuffer_Glossiness         ="GB_Glossiness";           ///<光泽度(R8/R16F/R32F)

const RENDER_BUFFER_NAME GBuffer_Reflection         ="GB_Reflection";           ///<反射(R8)
const RENDER_BUFFER_NAME GBuffer_Refraction         ="GB_Refraction";           ///<折射(R8)

const RENDER_BUFFER_NAME GBuffer_ClearCoat          ="GB_ClearCoat";            ///<透明涂层(R8/R16F/R32F)
const RENDER_BUFFER_NAME GBuffer_ClearCoatRoughness ="GB_ClearCoatRoughness";   ///<透明涂层粗糙度(R8/R16F/R32F)
const RENDER_BUFFER_NAME GBuffer_ClearCoatNormal    ="GB_ClearCoatNormal";      ///<透明涂层法线(RG8/RG16F)

const RENDER_BUFFER_NAME GBuffer_IOR                ="GB_IOR";                  ///<折射率(R8/R16F/R32F)
const RENDER_BUFFER_NAME GBuffer_Tranmission        ="GB_Tranmission";          ///<透射率(R8/R16F/R32F)
const RENDER_BUFFER_NAME GBuffer_Absorption         ="GB_Absorption";           ///<吸收率(R8/R16F/R32F)

const RENDER_BUFFER_NAME GBuffer_MicroThickness     ="GB_MicroThickness";       ///<微厚度(R8/R16F/R32F)

const RENDER_BUFFER_NAME GBuffer_AmbientOcclusion   ="GB_AmbientOcclusion";     ///<环境光遮蔽(R8/R16F/R32F)

const RENDER_BUFFER_NAME GBuffer_Anisotropy         ="GB_Anisotropy";           ///<各向异性(R8/R16F/R32F)
const RENDER_BUFFER_NAME GBuffer_AnisotropyDirection="GB_AnisotropyDirection";  ///<各向异性方向(RG8/RG16F/RG32F)

const RENDER_BUFFER_NAME GBuffer_Emissive           ="GB_Emissive";             ///<自发光(RGB565/RGB5A1/RGBA8/RGB10A2/RG11B10,RGBA16F)

const RENDER_BUFFER_NAME GBuffer_SheenColor         ="GB_SheenColor";           ///<光泽颜色(RGB565/RGB5A1/RGBA8/RGB10A2/RG11B10,RGBA16F)
const RENDER_BUFFER_NAME GBuffer_SheenRoughness     ="GB_SheenRoughness";       ///<光泽粗糙度(R8/R16F/R32F)

const RENDER_BUFFER_NAME GBuffer_SubsurfaceColor    ="GB_SubsurfaceColor";      ///<次表面颜色(RGB565/RGB5A1/RGBA8/RGB10A2/RG11B10,RGBA16F)

const RENDER_BUFFER_NAME GBuffer_ShadingModel       ="GB_ShadingModel";         ///<着色模型(R4UI/R8UI)

const RENDER_BUFFER_NAME GBuffer_LightingChannels   ="GB_LightingChannels";     ///<光照通道(R8UI)

const RENDER_BUFFER_NAME GBuffer_MotionVector       ="GB_MotionVector";         ///<运动矢量(RG16F)

const RENDER_BUFFER_NAME GBuffer_LastScreenPosition ="GB_LastScreenPosition";   ///<上一帧屏幕位置(RG16I)

VK_NAMESPACE_END
