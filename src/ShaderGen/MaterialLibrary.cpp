#include<hgl/graph/mtl/MaterialLibrary.h>
#include<hgl/graph/mtl/Material2DCreateConfig.h>
#include<hgl/graph/mtl/Material3DCreateConfig.h>

namespace hgl::graph::mtl{

const char *GetInlineMaterialName(const InlineMaterial mtl_id)
{
    switch(mtl_id)
    {
        case InlineMaterial::VertexColor2D:         return "VertexColor2D";
        case InlineMaterial::PureColor2D:           return "PureColor2D";
        case InlineMaterial::PureTexture2D:         return "PureTexture2D";
        case InlineMaterial::RectTexture2D:         return "RectTexture2D";
        case InlineMaterial::RectTexture2DArray:    return "RectTexture2DArray";
        case InlineMaterial::Text2D:                return "Text2D";
        case InlineMaterial::PureColor3D:           return "PureColor3D";
        case InlineMaterial::VertexColor3D:         return "VertexColor3D";
        case InlineMaterial::VertexLuminance3D:     return "VertexLuminance3D";
        case InlineMaterial::VertexPattleColor3D:   return "VertexPattleColor3D";
        case InlineMaterial::Gizmo3D:               return "Gizmo3D";
        case InlineMaterial::TextureBlinnPhong:     return "TextureBlinnPhong";
        case InlineMaterial::TerrainGrid:           return "TerrainGrid";
        case InlineMaterial::SkyMinimal:            return "SkyMinimal";
        case InlineMaterial::Billboard2D:           return "Billboard2D";
        case InlineMaterial::BasicLit:              return "BasicLit";
        default:                                    return nullptr;
    }
}

MaterialCreateInfo *CreateMaterialCreateInfo(const VulkanDevAttr *dev_attr,const InlineMaterial mtl_id,MaterialCreateConfig *cfg)
{
    if(!cfg)
        return(nullptr);

    switch(mtl_id)
    {
        case InlineMaterial::VertexColor2D:         return CreateVertexColor2D      (dev_attr,(const Material2DCreateConfig *)cfg);
        case InlineMaterial::PureColor2D:           return CreatePureColor2D        (dev_attr,(Material2DCreateConfig *)cfg);
        case InlineMaterial::PureTexture2D:         return CreatePureTexture2D      (dev_attr,(const Material2DCreateConfig *)cfg);
        case InlineMaterial::RectTexture2D:         return CreateRectTexture2D      (dev_attr,(Material2DCreateConfig *)cfg);
        case InlineMaterial::RectTexture2DArray:    return CreateRectTexture2DArray (dev_attr,(Material2DCreateConfig *)cfg);
        case InlineMaterial::Text2D:                return CreateText2D             (dev_attr,(const Text2DMaterialCreateConfig *)cfg);

        case InlineMaterial::PureColor3D:           return CreatePureColor3D        (dev_attr,(Material3DCreateConfig *)cfg);
        case InlineMaterial::VertexColor3D:         return CreateVertexColor3D      (dev_attr,(const Material3DCreateConfig *)cfg);
        case InlineMaterial::VertexLuminance3D:     return CreateVertexLuminance3D  (dev_attr,(Material3DCreateConfig *)cfg);
        case InlineMaterial::VertexPattleColor3D:   return CreateVertexPattleColor3D(dev_attr,(const Material3DCreateConfig *)cfg);
        case InlineMaterial::Gizmo3D:               return CreateGizmo3D            (dev_attr,(Material3DCreateConfig *)cfg);
        case InlineMaterial::TextureBlinnPhong:     return CreateTextureBlinnPhong  (dev_attr,(const Material3DCreateConfig *)cfg);
        case InlineMaterial::TerrainGrid:           return CreateTerrainGrid        (dev_attr,(const TerrainGridCreateConfig *)cfg);
        case InlineMaterial::SkyMinimal:            return CreateSkyMinimal         (dev_attr,(const SkyMinimalCreateConfig *)cfg);
        case InlineMaterial::Billboard2D:           return CreateBillboard2D        (dev_attr,(BillboardMaterialCreateConfig *)cfg);
        case InlineMaterial::BasicLit:              return CreateBasicLit           (dev_attr,(BasicLitMaterialCreateConfig *)cfg);

        default:                                    return nullptr;
    }
}

}//namespace hgl::graph::mtl
