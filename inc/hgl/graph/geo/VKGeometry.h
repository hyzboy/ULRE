#pragma once

#include<hgl/type/String.h>
#include<hgl/math/geometry/BoundingVolumes.h>
#include<hgl/vk/VK.h>
#include<hgl/log/Log.h>
#include<hgl/type/BlockAllocator.h>
#include<hgl/common/VertexAttribDef.h>

namespace hgl::graph{

// forward declare GeometryData to avoid including heavy headers
class GeometryData;

/**
 * 几何体数据访问接口<br>
 * Geometry的存为是为了屏蔽GeometryData的初始化之类的访问接口，以便于更好的管理和使用
 */
class Geometry
{
    OBJECT_LOGGER

protected:

    AnsiString      geometry_name;

    GeometryData *  geometry_data;

protected:

    BoundingVolumes bounding_volumes;    ///<包围体

    BlockAllocator::UserNode *ssbo_vtx_node=nullptr;    ///<SSBO 顶点分配节点
    BlockAllocator::UserNode *ssbo_idx_node=nullptr;    ///<SSBO 索引分配节点

public:

    Geometry(const AnsiString &pn,GeometryData *pd);
    virtual ~Geometry();

          void             SetBoundingVolumes(const BoundingVolumes &bv){bounding_volumes=bv;}

    const BoundingVolumes &GetBoundingVolumes()const{return bounding_volumes;}

public:

    const   AnsiString &    GetName         ()const{ return geometry_name; }

    const   bool            IsValid         ()const{ return geometry_data!=nullptr; }///<是否有效

    virtual
    const   VkDeviceSize    GetVertexCount  ()const;

    const   uint32_t        GetVABCount     ()const;
    const   int             GetVABIndex     (const AnsiString &name)const;
    const   int             GetVABIndex     (const VertexAttrib attrib)const;

            VAB *           GetVAB          (const int)const;
            VkBuffer        GetVkBuffer     (const int index)const;

            VAB *           GetVAB          (const AnsiString &name)const{return GetVAB(GetVABIndex(name));}
            VAB *           GetVAB          (const VertexAttrib attrib)const{return GetVAB(GetVABIndex(attrib));}
            VkBuffer        GetVkBuffer     (const AnsiString &name)const;

    const   int32_t         GetVertexOffset ()const;                        ///<取得顶点偏移(注意是顶点不是字节)

    const   uint32_t        GetIndexCount   ()const;
            IndexBuffer *   GetIBO          ()const;
    const   uint32_t        GetFirstIndex   ()const;                        ///<取得第一个索引

    VertexDataManager *     GetVDM          ()const;                        ///<取得顶点数据管理器

public: // SSBO allocation (for SSBO vertex fetch path)

    void SetSSBOVertexNode(BlockAllocator::UserNode *n){ssbo_vtx_node=n;}
    void SetSSBOIndexNode(BlockAllocator::UserNode *n){ssbo_idx_node=n;}

    BlockAllocator::UserNode *GetSSBOVertexNode()const{return ssbo_vtx_node;}
    BlockAllocator::UserNode *GetSSBOIndexNode()const{return ssbo_idx_node;}

    int32_t GetSSBOVertexOffset()const{return ssbo_vtx_node?ssbo_vtx_node->GetStart():-1;}
    int32_t GetSSBOIndexOffset()const{return ssbo_idx_node?ssbo_idx_node->GetStart():-1;}
};//class Geometry
}//namespace hgl::graph
