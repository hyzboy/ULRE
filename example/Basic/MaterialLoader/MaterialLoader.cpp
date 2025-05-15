#include<hgl/graph/mtl/MaterialLibrary.h>
#include<hgl/util/JsonTool.h>

/*

    material json example:

    {
        "name":"material_name",
        "type":"2d",
        "parent":"parent_material_name",

        "std2d":
        {
            "coordinate_system":"NDC",
            "local_to_world:"false",
            "position_format":"vec2"
        }
    }
*/

STD_MTL_NAMESPACE_BEGIN

MaterialCreateInfo *Load2DMaterialFromJson(const Json::Value& json_root)
{

}

MaterialCreateInfo *LoadMaterialFromJson(const Json::Value &json_root)
{

}

STD_MTL_NAMESPACE_END
