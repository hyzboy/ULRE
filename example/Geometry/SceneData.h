#pragma once
#include<hgl/type/String.h>
#include<hgl/graph/VK.h>
#include<hgl/io/MiniPack.h>

VK_NAMESPACE_BEGIN

namespace scene_file
{
    #pragma pack(push,1)
    struct HeaderData
    {
        uint32_t version;
        uint32_t rootCount;
        uint32_t matrixCount;
        uint32_t trsCount;
        uint32_t subMeshCount;
        uint32_t nodeCount;
    };
    #pragma pack(pop)

    struct TRSData
    {
        float translation[3];
        float rotation[4];
        float scale[3];
    };

    struct MatrixPairData
    {
        Matrix4f local;
        Matrix4f world;
    };

    struct NodeData
    {
                std::string_view    name;                           // points into mapped data
                uint32_t            matrixIndexPlusOne  = 0;
                uint32_t            trsIndexPlusOne     = 0;
        const   uint32_t *          childrenPtr         = nullptr;  // pointer into mapped data
                uint32_t            childrenCount       = 0;
        const   uint32_t *          subMeshesPtr        = nullptr;  // pointer into mapped data
                uint32_t            subMeshesCount      = 0;
    };

    struct SceneData
    {
        SceneData() = default;
        ~SceneData()
        {
            if(mpm)
                delete mpm; // keep mapped memory alive until SceneData destroyed
        }

        uint32_t version = 0;
        std::string_view sceneName; // points into mapped data

        // roots
        const uint32_t *rootsPtr = nullptr;
        uint32_t rootsCount = 0;

        // matrices
        const MatrixPairData *matricesPtr = nullptr;
        uint32_t matricesCount = 0;

        // trs
        const TRSData *trsPtr = nullptr;
        uint32_t trsCount = 0;

        // submeshes block (raw data, length-prefixed strings)
        const uint8_t *subMeshesPtr = nullptr;
        uint32_t subMeshesSize = 0;
        uint32_t subMeshCount = 0;

        // nodes
        std::vector<NodeData> nodes;

        hgl::io::minipack::MiniPackMemory *mpm = nullptr; // owned
    };
}//namespace scene_file
VK_NAMESPACE_END
