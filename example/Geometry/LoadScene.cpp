#include<hgl/io/FileInputStream.h>
#include<hgl/type/String.h>
#include<hgl/log/Log.h>
#include<hgl/graph/VKGeometry.h>
#include<hgl/graph/VKPrimitiveType.h>
#include<hgl/graph/VKVertexInputLayout.h>
#include<hgl/filesystem/Filename.h>
#include<hgl/graph/Bounds.h>
#include<hgl/io/MiniPack.h>
#include<hgl/io/MemoryInputStream.h>

#include <vector>
#include <string>
#include <string_view>
#include <cstdint>
#include <iostream>
#include <cstring>

DEFINE_LOGGER_MODULE(LoadScene)

VK_NAMESPACE_BEGIN
namespace
{
#pragma pack(push,1)
    struct SceneHeader
    {
        uint32_t version;
        uint32_t rootCount;
        uint32_t matrixCount;
        uint32_t trsCount;
        uint32_t subMeshCount;
        uint32_t nodeCount;
    };
#pragma pack(pop)

    struct SceneTRS
    {
        float translation[3];
        float rotation[4];
        float scale[3];
    };

    struct SceneMatrixPair
    {
        float local[16];
        float world[16];
    };

    struct SceneNode
    {
        std::string_view name; // points into mapped data
        uint32_t matrixIndexPlusOne = 0;
        uint32_t trsIndexPlusOne = 0;
        const uint32_t *childrenPtr = nullptr; // pointer into mapped data
        uint32_t childrenCount = 0;
        const uint32_t *subMeshesPtr = nullptr; // pointer into mapped data
        uint32_t subMeshesCount = 0;
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
        const SceneMatrixPair *matricesPtr = nullptr;
        uint32_t matricesCount = 0;

        // trs
        const SceneTRS *trsPtr = nullptr;
        uint32_t trsCount = 0;

        // submeshes block (raw data, length-prefixed strings)
        const uint8_t *subMeshesPtr = nullptr;
        uint32_t subMeshesSize = 0;
        uint32_t subMeshCount = 0;

        // nodes
        std::vector<SceneNode> nodes;

        hgl::io::minipack::MiniPackMemory *mpm = nullptr; // owned
    };

    bool ReadSceneHeader(hgl::io::minipack::MiniPackMemory *mpm, SceneHeader &header, const OSString &filename)
    {
        const int32 header_index = mpm->FindFile(AnsiStringView("SceneHeader"));
        if(header_index < 0)
        {
            MLogError(LoadScene,OS_TEXT("SceneHeader not found in file ") + filename);
            return false;
        }

        if(mpm->GetFileLength(header_index) != sizeof(SceneHeader))
        {
            MLogError(LoadScene,OS_TEXT("SceneHeader size mismatch in file ") + filename);
            return false;
        }

        const void *mapped = mpm->Map(header_index);
        if(!mapped)
        {
            MLogError(LoadScene,OS_TEXT("Cannot map SceneHeader from file ") + filename);
            return false;
        }

        header = *reinterpret_cast<const SceneHeader *>(mapped);

        if(header.version != 1)
        {
            MLogError(LoadScene,OS_TEXT("Unsupported scene version in file ") + filename);
            return false;
        }

        return true;
    }

    bool MapSceneName(hgl::io::minipack::MiniPackMemory *mpm, SceneData *scene, const OSString &filename)
    {
        const int32 name_index = mpm->FindFile(AnsiStringView("SceneName"));
        if(name_index < 0)
        {
            scene->sceneName = std::string_view();
            return true;
        }

        const uint32_t len = mpm->GetFileLength(name_index);
        if(len < 4)
        {
            MLogError(LoadScene,OS_TEXT("SceneName entry too small in file ") + filename);
            return false;
        }

        const uint8_t *mapped = reinterpret_cast<const uint8_t *>(mpm->Map(name_index));
        if(!mapped)
        {
            MLogError(LoadScene,OS_TEXT("Cannot map SceneName from file ") + filename);
            return false;
        }

        const uint32_t name_len = *reinterpret_cast<const uint32_t *>(mapped);
        if(name_len > 0)
        {
            if(4 + name_len > len)
            {
                MLogError(LoadScene,OS_TEXT("SceneName length exceeds entry size in file ") + filename);
                return false;
            }
            scene->sceneName = std::string_view(reinterpret_cast<const char *>(mapped + 4), name_len);
        }
        else
            scene->sceneName = std::string_view();

        return true;
    }

    bool MapRoots(hgl::io::minipack::MiniPackMemory *mpm, SceneData *scene, const OSString &filename)
    {
        const int32 roots_index = mpm->FindFile(AnsiStringView("Roots"));
        if(roots_index < 0)
        {
            MLogError(LoadScene,OS_TEXT("Roots not found in file ") + filename);
            return false;
        }

        const uint32_t size = mpm->GetFileLength(roots_index);
        if(size < 4)
        {
            MLogError(LoadScene,OS_TEXT("Roots entry too small in file ") + filename);
            return false;
        }

        const uint8_t *mapped = reinterpret_cast<const uint8_t *>(mpm->Map(roots_index));
        if(!mapped)
        {
            MLogError(LoadScene,OS_TEXT("Cannot map Roots from file ") + filename);
            return false;
        }

        const uint32_t count = *reinterpret_cast<const uint32_t *>(mapped);
        if(size != 4 + count*4)
        {
            MLogError(LoadScene,OS_TEXT("Roots size mismatch in file ") + filename);
            return false;
        }

        scene->rootsPtr = reinterpret_cast<const uint32_t *>(mapped + 4);
        scene->rootsCount = count;

        return true;
    }

    bool MapMatrices(hgl::io::minipack::MiniPackMemory *mpm, SceneData *scene, uint32_t expectedCount, const OSString &filename)
    {
        const int32 mats_index = mpm->FindFile(AnsiStringView("Matrices"));
        if(mats_index < 0)
        {
            if(expectedCount==0) return true;
            MLogError(LoadScene,OS_TEXT("Matrices not found in file ") + filename);
            return false;
        }

        const uint32_t size = mpm->GetFileLength(mats_index);
        const uint32_t entrySize = static_cast<uint32_t>(sizeof(SceneMatrixPair));
        if(size != expectedCount * entrySize)
        {
            MLogError(LoadScene,OS_TEXT("Matrices size mismatch in file ") + filename);
            return false;
        }

        const void *mapped = mpm->Map(mats_index);
        if(!mapped)
        {
            MLogError(LoadScene,OS_TEXT("Cannot map Matrices from file ") + filename);
            return false;
        }

        scene->matricesPtr = reinterpret_cast<const SceneMatrixPair *>(mapped);
        scene->matricesCount = expectedCount;

        return true;
    }

    bool MapTRS(hgl::io::minipack::MiniPackMemory *mpm, SceneData *scene, uint32_t expectedCount, const OSString &filename)
    {
        const int32 trs_index = mpm->FindFile(AnsiStringView("TRS"));
        if(trs_index < 0)
        {
            if(expectedCount==0) return true;
            MLogError(LoadScene,OS_TEXT("TRS not found in file ") + filename);
            return false;
        }

        const uint32_t size = mpm->GetFileLength(trs_index);
        const uint32_t entrySize = static_cast<uint32_t>(sizeof(SceneTRS));
        if(size != expectedCount * entrySize)
        {
            MLogError(LoadScene,OS_TEXT("TRS size mismatch in file ") + filename);
            return false;
        }

        const void *mapped = mpm->Map(trs_index);
        if(!mapped)
        {
            MLogError(LoadScene,OS_TEXT("Cannot map TRS from file ") + filename);
            return false;
        }

        scene->trsPtr = reinterpret_cast<const SceneTRS *>(mapped);
        scene->trsCount = expectedCount;

        return true;
    }

    bool MapSubMeshes(hgl::io::minipack::MiniPackMemory *mpm, SceneData *scene, uint32_t expectedCount, const OSString &filename)
    {
        const int32 sm_index = mpm->FindFile(AnsiStringView("SubMeshes"));
        if(sm_index < 0)
        {
            if(expectedCount==0) return true;
            MLogError(LoadScene,OS_TEXT("SubMeshes not found in file ") + filename);
            return false;
        }

        const uint32_t size = mpm->GetFileLength(sm_index);
        const void *mapped = mpm->Map(sm_index);
        if(!mapped)
        {
            MLogError(LoadScene,OS_TEXT("Cannot map SubMeshes from file ") + filename);
            return false;
        }

        scene->subMeshesPtr = reinterpret_cast<const uint8_t *>(mapped);
        scene->subMeshesSize = size;
        scene->subMeshCount = expectedCount;

        return true;
    }

    bool MapNodes(hgl::io::minipack::MiniPackMemory *mpm, SceneData *scene, uint32_t expectedCount, const OSString &filename)
    {
        const int32 nodes_index = mpm->FindFile(AnsiStringView("Nodes"));
        if(nodes_index < 0)
        {
            if(expectedCount==0) return true;
            MLogError(LoadScene,OS_TEXT("Nodes not found in file ") + filename);
            return false;
        }

        const uint32_t size = mpm->GetFileLength(nodes_index);
        const uint8_t *mapped = reinterpret_cast<const uint8_t *>(mpm->Map(nodes_index));
        if(!mapped)
        {
            MLogError(LoadScene,OS_TEXT("Cannot map Nodes from file ") + filename);
            return false;
        }

        uint32_t offset = 0;
        scene->nodes.clear();
        scene->nodes.reserve(expectedCount);

        for(uint32_t i=0;i<expectedCount;++i)
        {
            if(offset + 4 > size)
            {
                MLogError(LoadScene,OS_TEXT("Cannot read node name length from file ") + filename);
                return false;
            }

            const uint32_t name_len = *reinterpret_cast<const uint32_t *>(mapped + offset);
            offset += 4;

            SceneNode node{};
            if(name_len>0)
            {
                if(offset + name_len > size)
                {
                    MLogError(LoadScene,OS_TEXT("Cannot read node name from file ") + filename);
                    return false;
                }
                node.name = std::string_view(reinterpret_cast<const char *>(mapped + offset), name_len);
                offset += name_len;
            }
            else
                node.name = std::string_view();

            if(offset + 8 > size)
            {
                MLogError(LoadScene,OS_TEXT("Cannot read node indices from file ") + filename);
                return false;
            }

            node.matrixIndexPlusOne = *reinterpret_cast<const uint32_t *>(mapped + offset); offset += 4;
            node.trsIndexPlusOne = *reinterpret_cast<const uint32_t *>(mapped + offset); offset += 4;

            if(offset + 4 > size)
            {
                MLogError(LoadScene,OS_TEXT("Cannot read node children count from file ") + filename);
                return false;
            }

            const uint32_t childCount = *reinterpret_cast<const uint32_t *>(mapped + offset); offset += 4;
            if(childCount>0)
            {
                if(offset + childCount*4 > size)
                {
                    MLogError(LoadScene,OS_TEXT("Cannot read node children list from file ") + filename);
                    return false;
                }
                node.childrenPtr = reinterpret_cast<const uint32_t *>(mapped + offset);
                node.childrenCount = childCount;
                offset += childCount*4;
            }

            if(offset + 4 > size)
            {
                MLogError(LoadScene,OS_TEXT("Cannot read node subMesh count from file ") + filename);
                return false;
            }

            const uint32_t smCount = *reinterpret_cast<const uint32_t *>(mapped + offset); offset += 4;
            if(smCount>0)
            {
                if(offset + smCount*4 > size)
                {
                    MLogError(LoadScene,OS_TEXT("Cannot read node subMesh list from file ") + filename);
                    return false;
                }
                node.subMeshesPtr = reinterpret_cast<const uint32_t *>(mapped + offset);
                node.subMeshesCount = smCount;
                offset += smCount*4;
            }

            scene->nodes.push_back(node);
        }

        if(offset != size)
            MLogNotice(LoadScene,OS_TEXT("Nodes entry size mismatch (parsed bytes differ) in file ") + filename);

        return true;
    }
}

// LoadScene: reads a .scene minipack file using MiniPackMemory and returns allocated SceneData (caller deletes). Returns nullptr on error.
SceneData *LoadScene(const OSString &filename)
{
    using namespace hgl::io::minipack;

    MiniPackMemory *mpm = GetMiniPackMemory(filename);
    if(!mpm)
    {
        MLogError(LoadScene,OS_TEXT("Cannot open scene minipack file in memory mode ") + filename);
        return nullptr;
    }

    SceneHeader sh{};
    if(!ReadSceneHeader(mpm, sh, filename))
    {
        delete mpm;
        return nullptr;
    }

    SceneData *scene = new SceneData();
    scene->version = sh.version;
    scene->mpm = mpm; // keep mapped memory alive

    if(!MapSceneName(mpm, scene, filename))
    {
        delete scene;
        return nullptr;
    }

    if(!MapRoots(mpm, scene, filename))
    {
        delete scene;
        return nullptr;
    }

    if(!MapMatrices(mpm, scene, sh.matrixCount, filename))
    {
        delete scene;
        return nullptr;
    }

    if(!MapTRS(mpm, scene, sh.trsCount, filename))
    {
        delete scene;
        return nullptr;
    }

    if(!MapSubMeshes(mpm, scene, sh.subMeshCount, filename))
    {
        delete scene;
        return nullptr;
    }

    if(!MapNodes(mpm, scene, sh.nodeCount, filename))
    {
        delete scene;
        return nullptr;
    }

    MLogNotice(LoadScene, ToOSString(scene->sceneName.data()));

    return scene;
}

VK_NAMESPACE_END
