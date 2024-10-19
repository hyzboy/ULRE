#pragma once

#include<hgl/graph/VK.h>

VK_NAMESPACE_BEGIN

/**
/


//Material models from Google Filament
//@see https://google.github.io/filament/Materials.html

enum class MaterialModels:uint8
{
    Unlit=0,
    Lit,
    Subsurface,
    Cloth,
};

struct GBufferFormat
{
    VkFormat    ShadingModel;       ///<着色模式

    VkFormat    BaseColor;          ///<颜色缓冲区格式
    VkFormat    Depth;              ///<深度缓冲区格式
    VkFormat    Stencil;            ///<模板缓冲区格式

    VkFormat    Normal;             ///<法线缓冲区格式

    VkFormat    Metallic;           ///<金属度
    VkFormat    Roughness;          ///<粗糙度(glossiness)
    VkFormat    Reflectance;        ///<反射率(Specular for non-metals)

    VkFormat    ClearCoat;
    VkFormat    ClearCoatRoughness;
    VkFormat    Anisotropy;

    VkFormat    Emissive;           ///<自发光
    VkFormat    AmbientOcclusion;   ///<环境光遮蔽

    VkFormat    MotionVector;       ///<运动向量
};//struct GBufferFormat

void InitGBufferFormat(GBufferFormat &bf)
{
    bf.BaseColor        =PF_A2BGR10UN;

    bf.Depth            =PF_D24UN_S8U;
    bf.Stencil          =PF_D24UN_S8U;

    bf.Normal           =PF_RG8UN;

    bf.MetallicRoughness=PF_RG8UN;

    bf.Emissive         =PF_A2BGR10UN;
    bf.AmbientOcclusion =PF_R8UN;

    bf.MotionVector     =PF_RG16SN;
}

VK_NAMESPACE_END
