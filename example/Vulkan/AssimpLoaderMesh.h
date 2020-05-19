#pragma once
#include<hgl/type/BaseString.h>
#include<hgl/math/Math.h>
#include<hgl/type/List.h>
#include<hgl/type/Map.h>

using namespace hgl;

struct MeshData
{
    UTF8String name;

    List<Vector3f>  position,   //顶点
                    normal,     //法线
                    tangent,    //切线
                    bitangent;  //副切线

    List<uint8>     colors;     //颜色

    List<uint16>    indices16;  //16位索引
    List<uint32>    indices32;  //32位索引

    AABB bounding_box;
};//struct MeshData

struct ModelSceneNode
{
    UTF8String name;

    Matrix4f local_matrix;

    List<uint32> mesh_index;

    ObjectList<ModelSceneNode> children_node;   //子节点
};//struct MeshNode

struct ModelData
{
    ObjectList<MeshData> mesh_data;
    Map<UTF8String,MeshData *> mesh_by_name;

    ModelSceneNode *root_node;
    AABB bounding_box;

public:

    void Add(MeshData *md)
    {
        mesh_data.Add(md);
        mesh_by_name.Add(md->name,md);
    }
};//struct ModelData

ModelData *AssimpLoadModel(const OSString &filename);
