#pragma once

#include<hgl/graph/module/GraphModule.h>
#include<hgl/graph/VKMaterial.h>
#include<hgl/graph/VKMaterialInstance.h>
#include<hgl/graph/VKShaderModule.h>
#include<hgl/type/Map.h>
#include<hgl/type/ObjectManage.h>
#include<hgl/shadergen/MaterialCreateInfo.h>

VK_NAMESPACE_BEGIN

namespace mtl
{
    struct Material2DCreateConfig;
    struct Material3DCreateConfig;
    class MaterialCreateInfo;
}//namespace mtl

using MaterialID            = int;
using MaterialInstanceID    = int;
using ShaderModuleMapByName = ObjectMap<AnsiString,ShaderModule>;

constexpr const size_t VK_SHADER_STAGE_TYPE_COUNT = 20;//GetBitOffset((uint32_t)VK_SHADER_STAGE_CLUSTER_CULLING_BIT_HUAWEI)+1;

GRAPH_MODULE_CLASS(MaterialManager)
{
private:

    ShaderModuleMapByName shader_module_by_name[VK_SHADER_STAGE_TYPE_COUNT];
    Map<AnsiString,Material *> material_by_name;
    
    IDObjectManage<MaterialID,             Material>           rm_material;                ///<材质合集
    IDObjectManage<MaterialInstanceID,     MaterialInstance>   rm_material_instance;       ///<材质实例合集

private:

    MaterialManager(RenderFramework *);
    ~MaterialManager()=default;

    friend class GraphModuleManager;

public: //Add

    MaterialID              Add(Material *          mtl ){return rm_material.Add(mtl);}
    MaterialInstanceID      Add(MaterialInstance *  mi  ){return rm_material_instance.Add(mi);}

public: //Get

    Material *          GetMaterial         (const MaterialID           &id){return rm_material.Get(id);}
    MaterialInstance *  GetMaterialInstance (const MaterialInstanceID   &id){return rm_material_instance.Get(id);}

public: //Release

    void Release(Material *         mtl ){rm_material.Release(mtl);}
    void Release(MaterialInstance * mi  ){rm_material_instance.Release(mi);}

public: //Shader

    const ShaderModule *CreateShaderModule(const AnsiString &shader_module_name, const ShaderCreateInfo *);

public: //Material

    Material *          CreateMaterial  (const AnsiString &, const mtl::MaterialCreateInfo *);       ///<基于名称创建一个材质(一般用于程序内嵌材质)

    Material *          LoadMaterial    (const AnsiString &, mtl::Material2DCreateConfig *);         ///<基于资产名称加载一个材质(一般用于从文件加载材质)
    Material *          LoadMaterial    (const AnsiString &, mtl::Material3DCreateConfig *);

public: //MaterialInstance

    MaterialInstance *  CreateMaterialInstance(Material *, const VIL *vil);
    MaterialInstance *  CreateMaterialInstance(Material *, const VILConfig *vil_cfg=nullptr);

    MaterialInstance *  CreateMaterialInstance(Material *, const VIL *vil, const void *, const int);
    MaterialInstance *  CreateMaterialInstance(Material *, const VILConfig *vil_cfg, const void *, const int);

    template<typename T>
    MaterialInstance *  CreateMaterialInstance(Material *mtl, const VIL *vil, const T *data)
    {
        return CreateMaterialInstance(mtl, vil, data, sizeof(T));
    }

    template<typename T>
    MaterialInstance *  CreateMaterialInstance(Material *mtl, const VILConfig *vil_cfg, const T *data)
    {
        return CreateMaterialInstance(mtl, vil_cfg, data, sizeof(T));
    }

    MaterialInstance *  CreateMaterialInstance(const AnsiString &mtl_name, const mtl::MaterialCreateInfo *, const VILConfig *vil_cfg=nullptr);

};//class MaterialManager

VK_NAMESPACE_END
