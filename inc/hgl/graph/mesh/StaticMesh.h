#pragma once

#include <hgl/type/ObjectList.h>
#include <hgl/type/SortedSet.h>
#include <hgl/type/ArrayList.h>
#include <hgl/graph/mesh/Mesh.h>
#include <hgl/graph/mesh/MeshNode.h>

VK_NAMESPACE_BEGIN

using PrimitivePtrSet       =SortedSet<Primitive *>;
using MaterialInstanceSet   =SortedSet<MaterialInstance *>;
using PipelinePtrSet        =SortedSet<Pipeline *>;

// glTF-compatible material schema (field names match glTF 2.0)
namespace gltf
{
    struct TextureInfo
    {
        int index = -1;         // textures[index]
        int texCoord = 0;       // set index, default 0
    };

    struct NormalTextureInfo: public TextureInfo
    {
        float scale = 1.0f;     // normal scale
    };

    struct OcclusionTextureInfo: public TextureInfo
    {
        float strength = 1.0f;  // occlusion strength
    };

    struct PBRMetallicRoughness
    {
        float baseColorFactor[4] = {1,1,1,1};
        TextureInfo baseColorTexture;          // optional

        float metallicFactor = 1.0f;
        float roughnessFactor = 1.0f;
        TextureInfo metallicRoughnessTexture;  // optional
    };

    enum class AlphaMode
    {
        OPAQUE,
        MASK,
        BLEND
    };

    struct Material
    {
        // Core
        PBRMetallicRoughness pbrMetallicRoughness;  // optional
        NormalTextureInfo    normalTexture;         // optional
        OcclusionTextureInfo occlusionTexture;      // optional
        TextureInfo          emissiveTexture;       // optional

        float emissiveFactor[3] = {0,0,0};

        AlphaMode    alphaMode   = AlphaMode::OPAQUE; // glTF uses string; map to enum
        float        alphaCutoff = 0.5f;              // used when alphaMode == MASK
        bool         doubleSided = false;

        AnsiString   name;                            // optional
    };
}// namespace gltf

/**
* StaticMesh
* 负责管理一组 Primitive 与其对应的 Mesh，同时集中跟踪使用到的 MaterialInstance 与 Pipeline（多实例）。
* 另外，Mesh 内部维护一个 MeshNode 树结构与一个 MeshNode 集合，并在构造时创建一个根节点。
* 注意：Mesh 拥有其内部创建的 Mesh 的生命周期；Primitive/MaterialInstance/Pipeline 仅持有引用不管理生命周期。
*/
class StaticMesh
{
    // Mesh / 资源集合
    ObjectList<Mesh>    submeshes;          ///< Mesh 列表（拥有对象）
    PrimitivePtrSet     primitives;         ///< 关联的 Primitive 集合（仅持引用）
    MaterialInstanceSet mat_inst_set;       ///< 使用到的材质实例集合（仅持引用）
    PipelinePtrSet      pipeline_set;       ///< 使用到的管线集合（仅持引用）

    // MeshNode 集合与根节点
    MeshNodeList        nodes;              ///< MeshNode 集合（拥有对象）
    MeshNode *          root_node = nullptr;///< 根节点（由 nodes 持有）

    AABB                bounding_box;       ///< 所有 Mesh 合并的本地包围盒

    // glTF-compatible materials list
public:
    ArrayList<gltf::Material> materials;    ///< glTF: materials[]

public:

    StaticMesh();
    virtual ~StaticMesh();

public: // MeshNode 管理

    MeshNode *                  GetRootNode         () const { return root_node; }
    const MeshNodeList &        GetNodes            () const { return nodes; }
    MeshNodeList &              GetNodes            ()       { return nodes; }

    // 将一个节点加入集合（Mesh 接管其生命周期）；若未设置父子关系，可自行将其挂到根节点下
    bool                        AddNode             (MeshNode *node) { return node ? nodes.Add(node) >= 0 : false; }

    // 从集合移除并销毁该节点（若为根节点会一并清空并重建新的空根节点）
    void                        RemoveNode          (MeshNode *node);

    // 清空所有节点并重建一个空根节点
    void                        ClearNodes          ();

public: // Mesh 管理

    const int                   GetSubMeshCount     ()                      const{ return submeshes.GetCount(); }
    const ObjectList<Mesh> &    GetSubMeshes        ()                      const{ return submeshes; }

    // 创建并添加一个 Mesh（为该 Mesh 指定 Primitive / MaterialInstance / Pipeline）
    Mesh *                      CreateMesh          (Primitive *prim, MaterialInstance *mi, Pipeline *p);

    // 添加一个已有的 Mesh（Mesh 将接管其生命周期）
    bool                        AddSubMesh          (Mesh *sm);

    // 从 StaticMesh 中移除并销毁一个 Mesh
    void                        RemoveSubMesh       (Mesh *sm);

    // 清空并销毁所有 Mesh
    void                        ClearSubMeshes      ();

public: // Primitive / MaterialInstance / Pipeline（仅保存引用，便于统计/查询）

    bool                        AttachPrimitive     (Primitive *prim);
    void                        DetachPrimitive     (Primitive *prim);
    const PrimitivePtrSet &     GetPrimitives       () const { return primitives; }

    const MaterialInstanceSet & GetMaterialInstances() const { return mat_inst_set; }
    const PipelinePtrSet &      GetPipelines        () const { return pipeline_set; }

public: // 数据更新

    // 当 Primitive/VIL 数据发生变化时，更新所有 Mesh 的渲染数据
    void                        UpdateAllSubMeshes  ();

public: // 包围盒

    void                        RefreshBoundingBox  ();
    const AABB &                GetBoundingBox      () const { return bounding_box; }

private:

    void                        RebuildResourceSets ();
};

VK_NAMESPACE_END
