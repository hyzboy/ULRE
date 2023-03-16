#include<hgl/shadergen/ShaderDescriptorManager.h>
#include<hgl/CompOperator.h>

SHADERGEN_NAMESPACE_BEGIN

using ParamPreciseFlagBits=uint8;
using PPFB=ParamPreciseFlagBits;

constexpr const PPFB PPFB_None          =0x00;  ///<参数没有出现在任何地方
constexpr const PPFB PPFB_Global        =0x01;  ///<参数出现在全局(使用一个const值处理)
constexpr const PPFB PPFB_Vertex        =0x02;  ///<参数出现在顶点(在vertex shader使用一个顶点值处理)
constexpr const PPFB PPFB_Texture       =0x04;  ///<参数出现在纹理(在fragment shader使用一个纹理处理)

using TextureComponentFlagBits=uint8;
using TCFB=TextureComponentFlagBits;

constexpr const TCFB TCFB_Luminance     =0x01;  ///<亮度
constexpr const TCFB TCFB_Alpha         =0x02;  ///<透明度
constexpr const TCFB TCFB_Red           =0x04;  ///<红色
constexpr const TCFB TCFB_Green         =0x08;  ///<绿色
constexpr const TCFB TCFB_Blue          =0x10;  ///<蓝色
constexpr const TCFB TCFB_Cb            =0x20;  ///<Cb
constexpr const TCFB TCFB_Cr            =0x40;  ///<Cr

//constexpr const TCFB TCFB_NormalX       =0x80;
//constexpr const TCFB TCFB_NormalY       =0x100;
//constexpr const TCFB TCFB_NormalZ       =0x200;
//constexpr const TCFB TCFB_Metallic      =0x400;
//constexpr const TCFB TCFB_Roughness     =0x800;
//constexpr const TCFB TCFB_AO            =0x1000;
//constexpr const TCFB TCFB_Emissive      =0x2000;
//constexpr const TCFB TCFB_Specular      =0x4000;
//constexpr const TCFB TCFB_SpecularPower =0x8000;

constexpr const TCFB TCFB_RGB           =TCFB_Red|TCFB_Green|TCFB_Blue;
constexpr const TCFB TCFB_RGBA          =TCFB_RGB|TCFB_Alpha;
constexpr const TCFB TCFB_YCbCr         =TCFB_Luminance|TCFB_Cb|TCFB_Cr;
constexpr const TCFB TCFB_YCbCrAlpha    =TCFB_YCbCr|TCFB_Alpha;
constexpr const TCFB TCFB_LumAlpha      =TCFB_Luminance|TCFB_Alpha;
constexpr const TCFB TCFB_LumRGB        =TCFB_Luminance|TCFB_RGB;
//constexpr const TCFB TCFB_Normal2C      =TCFB_NormalX|TCFB_NormalY;
//constexpr const TCFB TCFB_Normal        =TCFB_Normal2C|TCFB_NormalZ;

/**
* 标准材质，依赖内建的Shader自动生成器
*/
struct StandardMaterial
{
};//struct StandardMaterial

/**
* 标准2D材质
*/
struct Standard2DMaterial:public StandardMaterial
{
    union
    {
        struct
        {
            bool z_test         :1;     ///<是否进行Z测试
            bool z_write        :1;     ///<是否写入Z值

            bool ortho_matrix   :1;     ///<是否使用ortho坐标系
            bool position_matrix:1;     ///<是否包含pos变换矩阵
            bool color_uv       :1;     ///<是否包含颜色纹理坐标
            bool color_uv_matrix:1;     ///<是否包含颜色纹理坐标变换矩阵
            bool alpha_uv       :1;     ///<是否包含透明纹理坐标
            bool alpha_uv_matrix:1;     ///<是否包含透明纹理坐标变换矩阵
        };

        uint8 flags;
    }vert;

    union
    {
        struct
        {
            bool discard:1;             ///<是否使用discard
            bool blur:1;                ///<是否模糊
        };
    }frag;

    TCFB color_texture;                 ///<颜色纹理格式
    TCFB alpha_texture;                 ///<透明纹理格式(即便颜色纹理包含Alpha，也有可能存在独立的alpha纹理)

    PPFB color;                         ///<是否包含颜色缩放系数
    PPFB alpha;                         ///<是否包含alpha缩放系数

    PPFB lum;                           ///<是否包含亮度调整
    PPFB hue;                           ///<是否包含色调调整
    PPFB contrast;                      ///<是否包含对比度调整

    PPFB outline;                       ///<是否描边

public:

    CompOperatorMemcmp(const Standard2DMaterial &);

    void Reset()
    {
        hgl_zero(*this);
    }
};//struct Standard2DMaterial:public StandardMaterial

class MaterialCreater;

Material *CreateMaterial()
{
}
SHADERGEN_NAMESPACE_END
