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
