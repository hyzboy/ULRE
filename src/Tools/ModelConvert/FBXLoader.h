#ifndef HGL_TOOL_MODEL_CONVERT_FBX_LOADER_INCLUDE
#define HGL_TOOL_MODEL_CONVERT_FBX_LOADER_INCLUDE

#include<hgl/math/Math.h>
#include<hgl/type/List.h>
#include<hgl/type/Map.h>
#include<hgl/type/StringList.h>
#include<hgl/graph/VertexBuffer.h>
#include<hgl/graph/SceneNode.h>
#include<hgl/graph/NTB.h>
#include<hgl/graph/Mesh.h>
#include<hgl/io/FileOutputStream.h>
#include<vector>
#include<cstdint>

// FBX SDK forward declarations
class FbxManager;
class FbxScene;
class FbxNode;
class FbxMesh;
class FbxSurfaceMaterial;
class FbxTexture;

using namespace hgl;
using namespace hgl::graph;

const Color4f pure_black_color(0,0,0,1);
const Color4f pure_white_color(1,1,1,1); 

/**
 * FBX材质数据结构（暂时为占位符，供未来实现）
 */
struct FBXMaterialData
{
    UTF8String name;
    Color4f ambient_color = pure_black_color;
    Color4f diffuse_color = pure_white_color;
    Color4f specular_color = pure_white_color;
    Color4f emissive_color = pure_black_color;
    float shininess = 0.0f;
    float transparency = 0.0f;
    
    // 贴图信息（占位符）
    UTF8String diffuse_texture;
    UTF8String normal_texture;
    UTF8String specular_texture;
};

/**
 * FBX贴图数据结构（暂时为占位符，供未来实现）
 */
struct FBXTextureData
{
    UTF8String name;
    UTF8String filename;
    UTF8String uv_set;
};

/**
 * FBX灯光数据结构（暂时为占位符，供未来实现）
 */
struct FBXLightData
{
    UTF8String name;
    Vector3f position;
    Vector3f direction;
    Color4f color = pure_white_color;
    float intensity = 1.0f;
    int type = 0; // 0=directional, 1=point, 2=spot
};

/**
 * FBX动画数据结构（暂时为占位符，供未来实现）
 */
struct FBXAnimationData
{
    UTF8String name;
    float start_time = 0.0f;
    float end_time = 0.0f;
    float frame_rate = 30.0f;
};

class FBXLoader
{
    OSString main_filename;
    uint total_file_bytes;
    
    FbxManager* fbx_manager;
    FbxScene* fbx_scene;

public:
    Vector3f scene_min, scene_max, scene_center;

private:
    // 材质相关（占位符）
    UTF8StringList tex_list;
    int material_count;
    FBXMaterialData *material_list;
    
    // 贴图相关（占位符）
    int texture_count;
    FBXTextureData *texture_list;
    
    // 灯光相关（占位符）
    int light_count;
    FBXLightData *light_list;
    
    // 动画相关（占位符）
    int animation_count;
    FBXAnimationData *animation_list;

private:
    void GetBoundingBoxForNode(FbxNode* node, Vector3f* min, Vector3f* max);
    void GetBoundingBox(FbxNode* node, Vector3f* min, Vector3f* max);

    void LoadMaterials();    // 占位符函数
    void LoadTextures();     // 占位符函数  
    void LoadLights();       // 占位符函数
    void LoadAnimations();   // 占位符函数
    void LoadMeshes();
    void LoadScene(const UTF8String& front, io::DataOutputStream* dos, FbxNode* node);

    void SaveFile(const void* data, const uint& size, const OSString& filename);
    void SaveFile(void** data_array, const int64* size_array, const int& count, const OSString& filename);

    void SaveTextures();     // 占位符函数

    Vector3f ConvertPosition(const FbxVector4& fbx_pos);
    Vector3f ConvertNormal(const FbxVector4& fbx_normal);
    void ProcessMeshData(FbxMesh* fbx_mesh);
    
#ifdef HAVE_FBX_SDK
    void CountMeshes(FbxNode* node, int& mesh_count, int& vertex_count, int& triangle_count);
    void ProcessNodeForMeshes(FbxNode* node);
    void CreateEngineMetaData(FbxMesh* fbx_mesh, FbxVector4* control_points, 
                             int vertex_count, int polygon_count, bool has_normals);
    void ExtractNormals(FbxGeometryElementNormal* normal_element, 
                       std::vector<Vector3f>& normals, int vertex_count);
    void ExtractIndices(FbxMesh* fbx_mesh, std::vector<uint32_t>& indices, int polygon_count);
#endif

public:
    FBXLoader();
    ~FBXLoader();

    bool LoadFile(const OSString& filename);
    bool SaveFile(const OSString& filename);
    
    // 调试信息输出
    void PrintSceneInfo();
};//class FBXLoader

#endif//HGL_TOOL_MODEL_CONVERT_FBX_LOADER_INCLUDE