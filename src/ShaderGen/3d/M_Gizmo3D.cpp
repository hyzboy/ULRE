#include<hgl/shadergen/MaterialCreateInfo.h>
#include<hgl/graph/mtl/MaterialCompiler.h>
#include<hgl/graph/mtl/Material3DCreateConfig.h>
#include"S_Gizmo3D.h"
#include<cstdio>

namespace hgl::graph::mtl{
namespace
{
    // Gizmo3D材质其实就是纯色的blinnphong材质，但不需要外部传入太阳光方向、高光系数等数据。
    // 其全部在Shader中直接包含，它是专门为Gizmo 3D控件所准备的一种材质。

    constexpr const char mi_codes[]="vec4 Color;";                      //材质实例代码
    constexpr const uint32_t mi_bytes=sizeof(math::Vector4f);                 //材质实例数据大小

    constexpr const char vs_main[]=R"(
void main()
{
    HandoverMI();

    Output.Normal   =GetNormal();
    Output.Position =GetPosition3D();

    gl_Position     =Output.Position;
})";

    //一个shader中输出的所有数据，会被定义在一个名为Output的结构中。所以编写时要用Output.XXXX来使用。
    //而同时，这个结构在下一个Shader中以Input名称出现，使用时以Input.XXX的形式使用。

    constexpr const char fs_main[]=R"(

const vec3 SUN_DIRECTION=vec3(0.655386,0.491539,0.573462);      //normalized(8,6,7)
const vec3 SUN_COLOR=vec3(1.0,1.0,1.0);

void main()
{
    MaterialInstance mi=GetMI();

    //点乘法线和光照
    float intensity=0.5*max(dot(Input.Normal,SUN_DIRECTION),0.0)+0.5;

    //直接光颜色
    vec3 direct_color=intensity*SUN_COLOR*mi.Color.rgb;

    vec3 spec_color=vec3(0);

    if(intensity>0.0)
    {
        vec3 half_vector=normalize(SUN_DIRECTION+normalize(Input.Position.xyz+camera.pos));

        float specular=max(dot(half_vector,Input.Normal),0.0);

        spec_color=specular*pow(specular,16)*SUN_COLOR;
    }

    FragColor=vec4(direct_color+spec_color,1.0);
})";

}//namespace

MaterialCreateInfo *CreateGizmo3D(const VulkanDevAttr *dev_attr,Material3DCreateConfig *cfg)
{
    ShaderPermutationKey key;

    if(cfg)
        cfg->material_instance=true;

    MaterialCreateInfo *mci_new = CompileComposedBusinessMaterial(
        dev_attr,
        GIZMO_3D_DEF,
        GIZMO_3D_COMPOSED_DEF,
        GIZMO_3D_LOGIC,
        key,
        cfg);

    if (!mci_new)
        std::fprintf(stderr, "[Gizmo3D] CompileComposedBusinessMaterial failed\n");
    return mci_new;
}
}//namespace hgl::graph::mtl
