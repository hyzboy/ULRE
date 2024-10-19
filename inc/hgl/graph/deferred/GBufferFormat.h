#pragma once

#include<hgl/graph/VK.h>

VK_NAMESPACE_BEGIN

//Material models from Google Filament
//@see https://google.github.io/filament/Materials.html

enum class MaterialModels:uint8
{
    Unlit=0,
    Lit,
    Subsurface,
    Cloth,
};

enum class ColorMode
{
    Unknow=0,

    RGB,
    Luminance,
    YCbCr,

    ENUM_CLASS_RANGE(Unknow,YCbCr)
};

enum class GBufferComponent:uint8
{
    ShadingModel=0,

    RGB,
    Y,
    CbCr,

    Normal,

    Depth,
    Stencil,

    Metallic,
    Roughness,
    Specular,
    Glosses,
    Emissive,
    AmbientOcclusion,
    Anisotropy,
    Reflectance,
    ClearCoat,
    ClearCoatRoughness,

    Opacity,

    MotionVector,

    WorldPosition,

    ENUM_CLASS_RANGE(ShadingModel,WorldPosition)
};

struct GBufferComponentConfig
{
    char name[32];                  ///<成分名称

    VkFormat format;

    uint count;                     ///<成分数量
    uint size[4];                   ///<成分长度
    GBufferComponent components[4]; ///<具体成份(分别对应xyzw四位)

public:

    const uint GetComponentSize(const GBufferComponent &c)const noexcept;
};//struct GBufferComponent

constexpr const size_t GBUFFER_MAX_COMPONENTS=16;

/**
* GBuffer格式配置
*/
class GBufferFormat
{
    uint                    count;                                                                  ///<成分数量
    GBufferComponentConfig  components_list[GBUFFER_MAX_COMPONENTS];                                ///<成分配置

    int                     components_index[size_t(GBufferComponent::RANGE_SIZE)];                 ///<成分索引

private:

    friend class GPUDevice;

public:

    const uint              GetCount()const noexcept{return count;}                                     ///<取得成分数量

    const GBufferComponentConfig *GetConfig(const int index)const noexcept                              ///<通过索引取得成分配置
    {
        return (index<0||index>=count)?nullptr:components_list+index;
    }

    const VkFormat          GetFormatFromComponent(const GBufferComponent &)const noexcept;             ///<通过成分取得格式

    const ColorMode         GetColorMode()const noexcept;                                               ///<取得颜色模式
    const bool              IsRGB       ()const noexcept{return GetColorMode()==ColorMode::RGB;}        ///<是否为RGB模式
    const bool              IsLuminance ()const noexcept{return GetColorMode()==ColorMode::Luminance;}  ///<是否为Luminance模式
    const bool              IsYCbCr     ()const noexcept{return GetColorMode()==ColorMode::YCbCr;}      ///<是否为YCbCr模式

    const uint              GetNormalSize()const noexcept;                                              ///<取得法线数据大小

public:

    GBufferFormat()
    {
        count=0;
        hgl_zero(components_list);
        hgl_set(components_index,-1,size_t(GBufferComponent::RANGE_SIZE));
    }

    ~GBufferFormat()=default;
};//class GBufferFormat

struct InitGBufferColorFormatConfig
{

};

bool InitGBufferFormat(GPUDevice *,GBufferFormat *);

//void InitGBufferFormat(GBufferFormat &bf)
//{
//    bf.BaseColor        =PF_A2BGR10UN;
//
//    bf.Depth            =PF_D24UN_S8U;
//    bf.Stencil          =PF_D24UN_S8U;
//
//    bf.Normal           =PF_RG8UN;
//
//    bf.MetallicRoughness=PF_RG8UN;
//
//    bf.Emissive         =PF_A2BGR10UN;
//    bf.AmbientOcclusion =PF_R8UN;
//
//    bf.MotionVector     =PF_RG16SN;
//}

VK_NAMESPACE_END
