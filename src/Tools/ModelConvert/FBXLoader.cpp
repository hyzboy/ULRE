#include "FBXLoader.h"
#include<hgl/log/LogInfo.h>
#include<hgl/io/FileInputStream.h>
#include<hgl/io/DataInputStream.h>
#include<hgl/filesystem/FileSystem.h>
#include<vector>
#include<cstdint>

// FBX SDK includes (假设已安装)
#ifdef HAVE_FBX_SDK
#include <fbxsdk.h>
#else
// 如果没有FBX SDK，定义空的占位符类型
class FbxManager {};
class FbxScene {};
class FbxNode {};
class FbxMesh {};
class FbxSurfaceMaterial {};
class FbxTexture {};
class FbxVector4 
{
public:
    double mData[4] = {0.0, 0.0, 0.0, 0.0};
    double& operator[](int index) { return mData[index]; }
    const double& operator[](int index) const { return mData[index]; }
};
#endif

using namespace hgl;
using namespace hgl::graph;

FBXLoader::FBXLoader()
{
    fbx_manager = nullptr;
    fbx_scene = nullptr;
    material_list = nullptr;
    texture_list = nullptr;
    light_list = nullptr;
    animation_list = nullptr;
    
    material_count = 0;
    texture_count = 0;
    light_count = 0;
    animation_count = 0;
    total_file_bytes = 0;
}

FBXLoader::~FBXLoader()
{
    delete[] material_list;
    delete[] texture_list;
    delete[] light_list;
    delete[] animation_list;

#ifdef HAVE_FBX_SDK
    if(fbx_scene)
        fbx_scene->Destroy();
    
    if(fbx_manager)
        fbx_manager->Destroy();
#endif
}

bool FBXLoader::LoadFile(const OSString& filename)
{
#ifdef HAVE_FBX_SDK
    main_filename = filename;
    
    // 初始化FBX Manager
    fbx_manager = FbxManager::Create();
    if(!fbx_manager)
    {
        LOG_ERROR(OS_TEXT("Failed to create FBX Manager"));
        return false;
    }
    
    // 创建FBX Scene
    fbx_scene = FbxScene::Create(fbx_manager, "");
    if(!fbx_scene)
    {
        LOG_ERROR(OS_TEXT("Failed to create FBX Scene"));
        return false;
    }
    
    // 创建导入器
    FbxImporter* importer = FbxImporter::Create(fbx_manager, "");
    if(!importer)
    {
        LOG_ERROR(OS_TEXT("Failed to create FBX Importer"));
        return false;
    }
    
    // 初始化导入器
    bool import_status = importer->Initialize(filename.c_str());
    if(!import_status)
    {
        LOG_ERROR(OS_TEXT("Failed to initialize FBX Importer: ") + UTF8String(importer->GetStatus().GetErrorString()));
        importer->Destroy();
        return false;
    }
    
    // 导入场景
    import_status = importer->Import(fbx_scene);
    if(!import_status)
    {
        LOG_ERROR(OS_TEXT("Failed to import FBX scene: ") + UTF8String(importer->GetStatus().GetErrorString()));
        importer->Destroy();
        return false;
    }
    
    importer->Destroy();
    
    // 三角化场景中的所有网格
    FbxGeometryConverter geometry_converter(fbx_manager);
    geometry_converter.Triangulate(fbx_scene, true);
    
    LOG_INFO(OS_TEXT("Successfully loaded FBX file: ") + filename);
    
    PrintSceneInfo();
    
    return true;
#else
    LOG_ERROR(OS_TEXT("FBX SDK not available. Cannot load FBX files."));
    return false;
#endif
}

void FBXLoader::PrintSceneInfo()
{
#ifdef HAVE_FBX_SDK
    if(!fbx_scene)
        return;
        
    FbxNode* root_node = fbx_scene->GetRootNode();
    if(!root_node)
        return;
    
    LOG_INFO(OS_TEXT("=== FBX Scene Information ==="));
    LOG_INFO(OS_TEXT("Scene name: ") + UTF8String(fbx_scene->GetName()));
    LOG_INFO(OS_TEXT("Root node: ") + UTF8String(root_node->GetName()));
    LOG_INFO(OS_TEXT("Child nodes: ") + UTF8String(root_node->GetChildCount()));
    
    // 统计网格数量
    int mesh_count = 0;
    int total_vertices = 0;
    int total_triangles = 0;
    
    for(int i = 0; i < root_node->GetChildCount(); i++)
    {
        FbxNode* child = root_node->GetChild(i);
        CountMeshes(child, mesh_count, total_vertices, total_triangles);
    }
    
    LOG_INFO(OS_TEXT("Total meshes: ") + UTF8String(mesh_count));
    LOG_INFO(OS_TEXT("Total vertices: ") + UTF8String(total_vertices));
    LOG_INFO(OS_TEXT("Total triangles: ") + UTF8String(total_triangles));
    LOG_INFO(OS_TEXT("=== End Scene Information ==="));
#endif
}

#ifdef HAVE_FBX_SDK
void FBXLoader::CountMeshes(FbxNode* node, int& mesh_count, int& vertex_count, int& triangle_count)
{
    if(!node)
        return;
        
    FbxNodeAttribute* attribute = node->GetNodeAttribute();
    if(attribute && attribute->GetAttributeType() == FbxNodeAttribute::eMesh)
    {
        FbxMesh* mesh = static_cast<FbxMesh*>(attribute);
        mesh_count++;
        vertex_count += mesh->GetControlPointsCount();
        triangle_count += mesh->GetPolygonCount();
    }
    
    // 递归处理子节点
    for(int i = 0; i < node->GetChildCount(); i++)
    {
        CountMeshes(node->GetChild(i), mesh_count, vertex_count, triangle_count);
    }
}
#endif

Vector3f FBXLoader::ConvertPosition(const FbxVector4& fbx_pos)
{
    // FBX使用右手坐标系，Y轴向上
    // 根据引擎需要可能需要坐标转换
    return Vector3f(static_cast<float>(fbx_pos[0]), 
                   static_cast<float>(fbx_pos[1]), 
                   static_cast<float>(fbx_pos[2]));
}

Vector3f FBXLoader::ConvertNormal(const FbxVector4& fbx_normal)
{
    return Vector3f(static_cast<float>(fbx_normal[0]), 
                   static_cast<float>(fbx_normal[1]), 
                   static_cast<float>(fbx_normal[2]));
}

void FBXLoader::ProcessMeshData(FbxMesh* fbx_mesh)
{
#ifdef HAVE_FBX_SDK
    if(!fbx_mesh)
        return;
        
    LOG_INFO(OS_TEXT("Processing mesh: ") + UTF8String(fbx_mesh->GetName()));
    
    int control_points_count = fbx_mesh->GetControlPointsCount();
    int polygon_count = fbx_mesh->GetPolygonCount();
    
    LOG_INFO(OS_TEXT("  Control points: ") + UTF8String(control_points_count));
    LOG_INFO(OS_TEXT("  Polygons: ") + UTF8String(polygon_count));
    
    // 获取顶点位置
    FbxVector4* control_points = fbx_mesh->GetControlPoints();
    
    // 检查是否有法线
    bool has_normals = fbx_mesh->GetElementNormalCount() > 0;
    LOG_INFO(OS_TEXT("  Has normals: ") + UTF8String(has_normals ? "Yes" : "No"));
    
    if(has_normals)
    {
        FbxGeometryElementNormal* normal_element = fbx_mesh->GetElementNormal(0);
        if(normal_element)
        {
            LOG_INFO(OS_TEXT("  Normal mapping mode: ") + UTF8String((int)normal_element->GetMappingMode()));
            LOG_INFO(OS_TEXT("  Normal reference mode: ") + UTF8String((int)normal_element->GetReferenceMode()));
        }
    }
    
    // 创建引擎网格数据结构
    CreateEngineMetaData(fbx_mesh, control_points, control_points_count, polygon_count, has_normals);
#endif
}

#ifdef HAVE_FBX_SDK
void FBXLoader::CreateEngineMetaData(FbxMesh* fbx_mesh, FbxVector4* control_points, 
                                    int vertex_count, int polygon_count, bool has_normals)
{
    LOG_INFO(OS_TEXT("Creating engine mesh data..."));
    
    // 准备顶点数据数组
    std::vector<Vector3f> positions;
    std::vector<Vector3f> normals;
    std::vector<uint32_t> indices;
    
    positions.reserve(vertex_count);
    if(has_normals)
        normals.reserve(vertex_count);
    
    // 提取顶点位置
    for(int i = 0; i < vertex_count; i++)
    {
        FbxVector4& pos = control_points[i];
        positions.push_back(ConvertPosition(pos));
    }
    
    // 提取法线（如果存在）
    if(has_normals)
    {
        FbxGeometryElementNormal* normal_element = fbx_mesh->GetElementNormal(0);
        if(normal_element)
        {
            ExtractNormals(normal_element, normals, vertex_count);
        }
    }
    
    // 提取索引
    ExtractIndices(fbx_mesh, indices, polygon_count);
    
    LOG_INFO(OS_TEXT("  Final vertex count: ") + UTF8String(positions.size()));
    LOG_INFO(OS_TEXT("  Final normal count: ") + UTF8String(normals.size()));
    LOG_INFO(OS_TEXT("  Final index count: ") + UTF8String(indices.size()));
    
    // 这里可以创建引擎的Primitive/Mesh对象
    // 类似于inline geometry examples中的做法:
    /*
    mesh = CreateMesh(mesh_name, vertex_count, material_instance, pipeline,
                     {
                         {VAN::Position, VF_V3F, positions.data()},
                         {VAN::Normal,   VF_V3F, normals.data()}
                     },
                     indices.data(), indices.size());
    */
    
    LOG_INFO(OS_TEXT("Engine mesh data structure ready for Primitive creation"));
}

void FBXLoader::ExtractNormals(FbxGeometryElementNormal* normal_element, 
                              std::vector<Vector3f>& normals, int vertex_count)
{
    if(!normal_element)
        return;
        
    const FbxLayerElement::EMappingMode mapping_mode = normal_element->GetMappingMode();
    const FbxLayerElement::EReferenceMode reference_mode = normal_element->GetReferenceMode();
    
    const FbxLayerElementArrayTemplate<FbxVector4>& normal_array = normal_element->GetDirectArray();
    const FbxLayerElementArrayTemplate<int>& index_array = normal_element->GetIndexArray();
    
    for(int i = 0; i < vertex_count; i++)
    {
        FbxVector4 normal(0, 1, 0, 0); // 默认向上法线
        
        if(mapping_mode == FbxLayerElement::eByControlPoint)
        {
            if(reference_mode == FbxLayerElement::eDirect)
            {
                normal = normal_array.GetAt(i);
            }
            else if(reference_mode == FbxLayerElement::eIndexToDirect)
            {
                int index = index_array.GetAt(i);
                normal = normal_array.GetAt(index);
            }
        }
        
        normals.push_back(ConvertNormal(normal));
    }
}

void FBXLoader::ExtractIndices(FbxMesh* fbx_mesh, std::vector<uint32_t>& indices, int polygon_count)
{
    // 假设所有多边形都已经三角化
    indices.reserve(polygon_count * 3);
    
    for(int i = 0; i < polygon_count; i++)
    {
        int polygon_size = fbx_mesh->GetPolygonSize(i);
        
        if(polygon_size == 3)
        {
            // 三角形
            for(int j = 0; j < 3; j++)
            {
                int vertex_index = fbx_mesh->GetPolygonVertex(i, j);
                indices.push_back(static_cast<uint32_t>(vertex_index));
            }
        }
        else
        {
            LOG_ERROR(OS_TEXT("Non-triangular polygon found! Mesh should be triangulated first."));
        }
    }
}
#endif

void FBXLoader::LoadMeshes()
{
#ifdef HAVE_FBX_SDK
    if(!fbx_scene)
        return;
        
    FbxNode* root_node = fbx_scene->GetRootNode();
    if(!root_node)
        return;
        
    LOG_INFO(OS_TEXT("Loading meshes..."));
    
    // 遍历所有节点，查找网格
    ProcessNodeForMeshes(root_node);
#endif
}

#ifdef HAVE_FBX_SDK
void FBXLoader::ProcessNodeForMeshes(FbxNode* node)
{
    if(!node)
        return;
        
    // 检查当前节点是否包含网格
    FbxNodeAttribute* attribute = node->GetNodeAttribute();
    if(attribute && attribute->GetAttributeType() == FbxNodeAttribute::eMesh)
    {
        FbxMesh* mesh = static_cast<FbxMesh*>(attribute);
        ProcessMeshData(mesh);
    }
    
    // 递归处理子节点
    for(int i = 0; i < node->GetChildCount(); i++)
    {
        ProcessNodeForMeshes(node->GetChild(i));
    }
}
#endif

void FBXLoader::LoadScene(const UTF8String& front, io::DataOutputStream* dos, FbxNode* node)
{
#ifdef HAVE_FBX_SDK
    if(!node || !dos)
        return;
        
    // 输出节点信息到数据流（类似AssimpLoader的实现）
    UTF8String node_name = node->GetName();
    
    // 计算包围盒（占位符）
    float box[6] = {0, 0, 0, 0, 0, 0};
    
    dos->WriteUTF8String(node_name);
    dos->WriteFloat(box, 6);
    
    // 获取节点变换矩阵
    FbxAMatrix& transform = node->EvaluateGlobalTransform();
    float matrix[16];
    for(int i = 0; i < 4; i++)
    {
        for(int j = 0; j < 4; j++)
        {
            matrix[i * 4 + j] = static_cast<float>(transform.Get(i, j));
        }
    }
    dos->WriteFloat(matrix, 16);
    
    // 写入网格数量（目前为0，因为我们还没有实现网格导出）
    dos->WriteUint32(0);
    
    LOG_INFO(front + U8_TEXT("Node[") + node_name + U8_TEXT("][Mesh:0][SubNode:") + UTF8String(node->GetChildCount()) + U8_TEXT("]"));
    
    // 写入子节点数量
    dos->WriteUint32(node->GetChildCount());
    
    // 递归处理子节点
    for(int i = 0; i < node->GetChildCount(); i++)
    {
        LoadScene(front + U8_TEXT("  "), dos, node->GetChild(i));
    }
#endif
}

bool FBXLoader::SaveFile(const OSString& filename)
{
    // 占位符实现，类似AssimpLoader
    LOG_INFO(OS_TEXT("Saving FBX data to: ") + filename);
    
    // 这里应该实现保存到引擎格式的逻辑
    // 现在只是一个占位符
    
    return true;
}

// 占位符函数实现
void FBXLoader::LoadMaterials()
{
    LOG_INFO(OS_TEXT("Loading materials (placeholder)"));
}

void FBXLoader::LoadTextures()
{
    LOG_INFO(OS_TEXT("Loading textures (placeholder)"));
}

void FBXLoader::LoadLights()
{
    LOG_INFO(OS_TEXT("Loading lights (placeholder)"));
}

void FBXLoader::LoadAnimations()
{
    LOG_INFO(OS_TEXT("Loading animations (placeholder)"));
}

void FBXLoader::SaveTextures()
{
    LOG_INFO(OS_TEXT("Saving textures (placeholder)"));
}

void FBXLoader::GetBoundingBoxForNode(FbxNode* node, Vector3f* min, Vector3f* max)
{
    // 占位符实现
    *min = Vector3f(-1, -1, -1);
    *max = Vector3f(1, 1, 1);
}

void FBXLoader::GetBoundingBox(FbxNode* node, Vector3f* min, Vector3f* max)
{
    // 占位符实现
    *min = Vector3f(-1, -1, -1);
    *max = Vector3f(1, 1, 1);
}

void FBXLoader::SaveFile(const void* data, const uint& size, const OSString& filename)
{
    // 占位符实现
}

void FBXLoader::SaveFile(void** data_array, const int64* size_array, const int& count, const OSString& filename)
{
    // 占位符实现
}