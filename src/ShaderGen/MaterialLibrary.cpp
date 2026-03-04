#include<hgl/graph/mtl/MaterialLibrary.h>
#include<hgl/graph/mtl/Material2DCreateConfig.h>
#include<hgl/graph/mtl/Material3DCreateConfig.h>
#include<hgl/shadergen/contract/ShaderGenContract.h>

namespace hgl::graph::mtl{

const char *GetInlineMaterialName(const MaterialPreset mtl_id)
{
    switch(mtl_id)
    {
        case MaterialPreset::VertexColor2D:         return "VertexColor2D";
        case MaterialPreset::PureColor2D:           return "PureColor2D";
        case MaterialPreset::PureTexture2D:         return "PureTexture2D";
        case MaterialPreset::RectTexture2D:         return "RectTexture2D";
        case MaterialPreset::RectTexture2DArray:    return "RectTexture2DArray";
        case MaterialPreset::Text2D:                return "Text2D";
        case MaterialPreset::PureColor3D:           return "PureColor3D";
        case MaterialPreset::VertexColor3D:         return "VertexColor3D";
        case MaterialPreset::VertexLuminance3D:     return "VertexLuminance3D";
        case MaterialPreset::VertexPattleColor3D:   return "VertexPattleColor3D";
        case MaterialPreset::Gizmo3D:               return "Gizmo3D";
        case MaterialPreset::TextureBlinnPhong:     return "TextureBlinnPhong";
        case MaterialPreset::TerrainGrid:           return "TerrainGrid";
        case MaterialPreset::SkyMinimal:            return "SkyMinimal";
        case MaterialPreset::Billboard2D:           return "Billboard2D";
        case MaterialPreset::BasicLit:              return "BasicLit";
        default:                                    return nullptr;
    }
}

MaterialCreateInfo *CreateMaterialCreateInfo(const VulkanDevAttr *dev_attr,
                                             const MaterialPreset mtl_id,
                                             MaterialCreateConfig *cfg)
{
    if(!cfg)
        return(nullptr);

    switch(mtl_id)
    {
        case MaterialPreset::VertexColor2D:         return CreateVertexColor2D      (dev_attr,(const Material2DCreateConfig *)cfg);
        case MaterialPreset::PureColor2D:           return CreatePureColor2D        (dev_attr,(Material2DCreateConfig *)cfg);
        case MaterialPreset::PureTexture2D:         return CreatePureTexture2D      (dev_attr,(const Material2DCreateConfig *)cfg);
        case MaterialPreset::RectTexture2D:         return CreateRectTexture2D      (dev_attr,(Material2DCreateConfig *)cfg);
        case MaterialPreset::RectTexture2DArray:    return CreateRectTexture2DArray (dev_attr,(Material2DCreateConfig *)cfg);
        case MaterialPreset::Text2D:                return CreateText2D             (dev_attr,(const Text2DMaterialCreateConfig *)cfg);

        case MaterialPreset::PureColor3D:           return CreatePureColor3D        (dev_attr,(Material3DCreateConfig *)cfg);
        case MaterialPreset::VertexColor3D:         return CreateVertexColor3D      (dev_attr,(const Material3DCreateConfig *)cfg);
        case MaterialPreset::VertexLuminance3D:     return CreateVertexLuminance3D  (dev_attr,(Material3DCreateConfig *)cfg);
        case MaterialPreset::VertexPattleColor3D:   return CreateVertexPattleColor3D(dev_attr,(const Material3DCreateConfig *)cfg);
        case MaterialPreset::Gizmo3D:               return CreateGizmo3D            (dev_attr,(Material3DCreateConfig *)cfg);
        case MaterialPreset::TextureBlinnPhong:     return CreateTextureBlinnPhong  (dev_attr,(const Material3DCreateConfig *)cfg);
        case MaterialPreset::TerrainGrid:           return CreateTerrainGrid        (dev_attr,(const TerrainGridCreateConfig *)cfg);
        case MaterialPreset::SkyMinimal:            return CreateSkyMinimal         (dev_attr,(const SkyMinimalCreateConfig *)cfg);
        case MaterialPreset::Billboard2D:           return CreateBillboard2D        (dev_attr,(BillboardMaterialCreateConfig *)cfg);
        case MaterialPreset::BasicLit:              return CreateBasicLit           (dev_attr,(BasicLitMaterialCreateConfig *)cfg);

        default:                                    return nullptr;
    }
}
}//namespace hgl::graph::mtl
