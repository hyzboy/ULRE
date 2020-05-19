﻿#ifndef HGL_TOOL_MODEL_CONVERT_ASSIMP_LOADER_INCLUDE
#define HGL_TOOL_MODEL_CONVERT_ASSIMP_LOADER_INCLUDE

#include<hgl/math/Math.h>
#include<hgl/type/List.h>
#include<hgl/type/Map.h>
#include<hgl/type/StringList.h>
#include<hgl/graph/VertexBuffer.h>
#include<hgl/graph/SceneNode.h>
#include<hgl/graph/NTB.h>
//#include<hgl/graph/Material/Material.h>
#include<hgl/graph/Mesh.h>
#include<hgl/io/FileOutputStream.h>
#include<assimp/Importer.hpp>
#include<assimp/scene.h>

using namespace hgl;
using namespace hgl::graph;

const Color4f pure_black_color(0,0,0,1);
const Color4f pure_white_color(1,1,1,1); 

class AssimpLoader
{
    OSString main_filename;

    uint total_file_bytes;

    const aiScene *scene;

public:

    aiVector3D scene_min, scene_max, scene_center;

private:

    UTF8StringList tex_list;

    int material_count;
    MaterialData *material_list;

private:

    void get_bounding_box_for_node(const aiNode *,aiVector3D *,aiVector3D *,aiMatrix4x4 *);
    void get_bounding_box(const aiNode *,aiVector3D *,aiVector3D *);

    void LoadMaterial();
    void LoadMesh();
    void LoadScene(const UTF8String &,io::DataOutputStream *,const aiNode *);

    void SaveFile(const void *,const uint &,const OSString &);
    void SaveFile(void **,const int64 *,const int &,const OSString &);

    void SaveTextures();

    template<typename T>
    void SaveFaces(io::FileOutputStream *,const aiFace *,const T);

    void SaveTexCoord(float *,const aiVector3D *,const uint,const uint);

public:

    AssimpLoader();
    ~AssimpLoader();

    bool LoadFile(const OSString &);
    bool SaveFile(const OSString &);
};//class AssimpLoader
#endif//HGL_TOOL_MODEL_CONVERT_ASSIMP_LOADER_INCLUDE
