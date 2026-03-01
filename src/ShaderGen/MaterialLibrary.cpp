#include<hgl/graph/mtl/MaterialLibrary.h>
#include<hgl/graph/mtl/Material2DCreateConfig.h>
#include<hgl/graph/mtl/Material3DCreateConfig.h>
#include<hgl/type/IDName.h>
#include<hgl/type/UnorderedMap.h>

#include<memory>

namespace hgl::graph::mtl{

namespace
{
    using MaterialFactoryMap=UnorderedMap<int,std::unique_ptr<MaterialFactory>>;

    MaterialFactoryMap &GetMaterialFactoryMap()
    {
        static MaterialFactoryMap material_factory_map;
        return material_factory_map;
    }
}//namespace

bool RegisterMaterialFactory(MaterialFactory *mf)
{
    if(!mf)
        return(false);

    const MaterialName &name=mf->GetName();
    const int name_id=name.GetID();

    auto &material_factory_map=GetMaterialFactoryMap();

    if(material_factory_map.ContainsKey(name_id))
    {
        delete mf;
        return(false);
    }

    material_factory_map.Add(name_id,std::unique_ptr<MaterialFactory>(mf));

    return(true);
}

MaterialFactory *GetMaterialFactory(const MaterialName &name)
{
    auto &material_factory_map=GetMaterialFactoryMap();
    std::unique_ptr<MaterialFactory>* ptr = material_factory_map.GetValuePointer(name.GetID());
    return ptr ? ptr->get() : nullptr;
}

void ClearMaterialFactory()
{
    GetMaterialFactoryMap().Clear();
}

const char *GetInlineMaterialName(const InlineMaterial mtl_id)
{
    switch(mtl_id)
    {
        case InlineMaterial::VertexColor2D:         return inline_material::VertexColor2D;
        case InlineMaterial::PureColor2D:           return inline_material::PureColor2D;
        case InlineMaterial::PureTexture2D:         return inline_material::PureTexture2D;
        case InlineMaterial::RectTexture2D:         return inline_material::RectTexture2D;
        case InlineMaterial::RectTexture2DArray:    return inline_material::RectTexture2DArray;
        case InlineMaterial::Text2D:                return inline_material::Text2D;
        case InlineMaterial::PureColor3D:           return inline_material::PureColor3D;
        case InlineMaterial::VertexColor3D:         return inline_material::VertexColor3D;
        case InlineMaterial::VertexLuminance3D:     return inline_material::VertexLuminance3D;
        case InlineMaterial::VertexPattleColor3D:   return inline_material::VertexPattleColor3D;
        case InlineMaterial::Gizmo3D:               return inline_material::Gizmo3D;
        case InlineMaterial::TextureBlinnPhong:     return inline_material::TextureBlinnPhong;
        case InlineMaterial::TerrainGrid:           return inline_material::TerrainGrid;
        case InlineMaterial::SkyMinimal:            return inline_material::SkyMinimal;
        case InlineMaterial::Billboard2D:           return inline_material::Billboard2D;
        case InlineMaterial::BasicLit:              return inline_material::BasicLit;
        default:                                    return nullptr;
    }
}

bool TryGetInlineMaterialByName(const AnsiString &mtl_name, InlineMaterial &out_mtl_id)
{
    if (mtl_name.IsEmpty())
        return false;

    const MaterialName input_name(mtl_name);

    static const MaterialName kVertexColor2D(inline_material::VertexColor2D);
    static const MaterialName kPureColor2D(inline_material::PureColor2D);
    static const MaterialName kPureTexture2D(inline_material::PureTexture2D);
    static const MaterialName kRectTexture2D(inline_material::RectTexture2D);
    static const MaterialName kRectTexture2DArray(inline_material::RectTexture2DArray);
    static const MaterialName kText2D(inline_material::Text2D);
    static const MaterialName kPureColor3D(inline_material::PureColor3D);
    static const MaterialName kVertexColor3D(inline_material::VertexColor3D);
    static const MaterialName kVertexLuminance3D(inline_material::VertexLuminance3D);
    static const MaterialName kVertexPattleColor3D(inline_material::VertexPattleColor3D);
    static const MaterialName kGizmo3D(inline_material::Gizmo3D);
    static const MaterialName kTextureBlinnPhong(inline_material::TextureBlinnPhong);
    static const MaterialName kTerrainGrid(inline_material::TerrainGrid);
    static const MaterialName kSkyMinimal(inline_material::SkyMinimal);
    static const MaterialName kBillboard2D(inline_material::Billboard2D);
    static const MaterialName kBasicLit(inline_material::BasicLit);

    if (input_name == kVertexColor2D)       { out_mtl_id = InlineMaterial::VertexColor2D; return true; }
    if (input_name == kPureColor2D)         { out_mtl_id = InlineMaterial::PureColor2D; return true; }
    if (input_name == kPureTexture2D)       { out_mtl_id = InlineMaterial::PureTexture2D; return true; }
    if (input_name == kRectTexture2D)       { out_mtl_id = InlineMaterial::RectTexture2D; return true; }
    if (input_name == kRectTexture2DArray)  { out_mtl_id = InlineMaterial::RectTexture2DArray; return true; }
    if (input_name == kText2D)              { out_mtl_id = InlineMaterial::Text2D; return true; }
    if (input_name == kPureColor3D)         { out_mtl_id = InlineMaterial::PureColor3D; return true; }
    if (input_name == kVertexColor3D)       { out_mtl_id = InlineMaterial::VertexColor3D; return true; }
    if (input_name == kVertexLuminance3D)   { out_mtl_id = InlineMaterial::VertexLuminance3D; return true; }
    if (input_name == kVertexPattleColor3D) { out_mtl_id = InlineMaterial::VertexPattleColor3D; return true; }
    if (input_name == kGizmo3D)             { out_mtl_id = InlineMaterial::Gizmo3D; return true; }
    if (input_name == kTextureBlinnPhong)   { out_mtl_id = InlineMaterial::TextureBlinnPhong; return true; }
    if (input_name == kTerrainGrid)         { out_mtl_id = InlineMaterial::TerrainGrid; return true; }
    if (input_name == kSkyMinimal)          { out_mtl_id = InlineMaterial::SkyMinimal; return true; }
    if (input_name == kBillboard2D)         { out_mtl_id = InlineMaterial::Billboard2D; return true; }
    if (input_name == kBasicLit)            { out_mtl_id = InlineMaterial::BasicLit; return true; }

    return false;
}

MaterialCreateInfo *CreateMaterialCreateInfo(const VulkanDevAttr *dev_attr,const InlineMaterial mtl_id,MaterialCreateConfig *cfg)
{
    const char *mtl_name=GetInlineMaterialName(mtl_id);

    if(!mtl_name)
        return(nullptr);

    MaterialName mtl_id_name(mtl_name);

    return CreateMaterialCreateInfo(dev_attr,mtl_id_name,cfg);
}

MaterialCreateInfo *CreateMaterialCreateInfo(const VulkanDevAttr *dev_attr,const MaterialName &name,MaterialCreateConfig *cfg)
{
    if(!cfg)
        return(nullptr);

    MaterialFactory *mf=GetMaterialFactory(name);

    if(!mf)
        return(nullptr);

    return mf->Create(dev_attr,cfg);
}

}//namespace hgl::graph::mtl
