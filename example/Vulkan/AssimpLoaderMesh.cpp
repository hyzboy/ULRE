﻿#include"AssimpLoaderMesh.h"
#include<assimp/postprocess.h>
#include<assimp/cimport.h>
#include<assimp/scene.h>
#include<hgl/filesystem/FileSystem.h>
#include<hgl/CodePage.h>
#include<hgl/type/Color4f.h>

using namespace hgl::filesystem;

namespace
{
    const aiScene *LoadSceneFromFile(const OSString &filename)
    {
        if(filename.IsEmpty())return(nullptr);

        uint filesize;
        char *filedata;

        filesize=filesystem::LoadFileToMemory(filename,(void **)&filedata);

        if(!filedata)
            return(nullptr);

    #ifdef _WIN32
        const UTF8String ext_name=ToUTF8String(filesystem::ClipFileExtName(filename,false));
    #else
        const UTF8String ext_name=filesystem::ClipFileExtName(filename);
    #endif//

        const aiScene *scene=aiImportFileFromMemory(filedata,filesize,aiProcessPreset_TargetRealtime_MaxQuality|aiProcess_FlipUVs,ext_name.c_str());

        delete[] filedata;

        if(!scene)
            return(nullptr);

        return scene;
    }

    #define LOG_BR  LOG_INFO(OS_TEXT("------------------------------------------------------------"));

    #define aisgl_min(x,y) (x<y?x:y)
    #define aisgl_max(x,y) (y>x?y:x)

    const Color4f COLOR_PURE_BLACK(0,0,0,1);
    const Color4f COLOR_PURE_WHITE(1,1,1,1);

    inline const vec pos_to_vec(const aiVector3D &v3d){return vec(v3d.x,-v3d.z,v3d.y,1.0);}
    inline const vec dir_to_vec(const aiVector3D &v3d){return vec(v3d.x,-v3d.z,v3d.y,0.0);}

    void VecY2Z(List<Vector3f> &target,const aiVector3D *source,const uint count)
    {
        target.SetCount(count);

        if(count<=0)return;

        Vector3f *tp=target.GetData();

        for(uint i=0;i<count;i++)
        {
            tp->x= source->x;
            tp->y=-source->z;
            tp->z= source->y;

            ++source;
            ++tp;
        }
    }

    uint8 *ColorF2B(uint8 *tp,const aiColor4D *c4,const int count)
    {
        for(int i=0;i<count;i++)
        {
            *tp=c4->r*255;++tp;
            *tp=c4->g*255;++tp;
            *tp=c4->b*255;++tp;
            *tp=c4->a*255;++tp;

            ++c4;
        }

        return tp;
    }

    template<typename T>
    void Face2Indices(List<T> &indices,const aiFace *face,const uint count)
    {
        indices.SetCount(count*3);

        T *tp=indices.GetData();

        for(uint i=0;i<count;i++)
        {
            *tp=face->mIndices[0];++tp;
            *tp=face->mIndices[1];++tp;
            *tp=face->mIndices[2];++tp;

            ++face;
        }
    }

    AABB GetAABB(const List<Vector3f> &pos)
    {
        const Vector3f *vertex=pos.GetData();
        const uint count=pos.GetCount();

        Vector3f minPoint=*vertex;
        Vector3f maxPoint=*vertex;

        ++vertex;
        for(uint i=1;i<count;i++)
        {
            minPoint.x = aisgl_min(minPoint.x,vertex->x);
            minPoint.y = aisgl_min(minPoint.y,vertex->y);
            minPoint.z = aisgl_min(minPoint.z,vertex->z);

            maxPoint.x = aisgl_max(maxPoint.x,vertex->x);
            maxPoint.y = aisgl_max(maxPoint.y,vertex->y);
            maxPoint.z = aisgl_max(maxPoint.z,vertex->z);

            ++vertex;
        }

        return AABB(POINT_VEC(minPoint),POINT_VEC(maxPoint));
    }

    Matrix4f MatrixRotate(const aiMatrix4x4 &s)
    {
        return Matrix4f(s.a1,s.a2,s.a3,s.a4,
                        s.b1,s.b2,s.b3,s.b4,
                        s.c1,s.c2,s.c3,s.c4,
                        s.d1,s.d2,s.d3,s.d4);
    }
}//namespace

class AssimpLoaderMesh
{
    OSString filename;
    const aiScene *scene;

    ModelData *model_data;

private:

    void GetBoundingBox(const   aiNode *    node,
                                aiVector3D *min_pos,
                                aiVector3D *max_pos,
                                const aiMatrix4x4 &up_matrix)
    {
        aiMatrix4x4 cur_matrix;
        uint n = 0, t;

        cur_matrix = up_matrix;

        aiMultiplyMatrix4(&cur_matrix,&node->mTransformation);

        for (; n < node->mNumMeshes; ++n)
        {
            const aiMesh *mesh=scene->mMeshes[node->mMeshes[n]];

            for (t = 0; t < mesh->mNumVertices; ++t)
            {
                aiVector3D tmp = mesh->mVertices[t];
                aiTransformVecByMatrix4(&tmp,&cur_matrix);

                min_pos->x = aisgl_min(min_pos->x,tmp.x);
                min_pos->y = aisgl_min(min_pos->y,tmp.y);
                min_pos->z = aisgl_min(min_pos->z,tmp.z);

                max_pos->x = aisgl_max(max_pos->x,tmp.x);
                max_pos->y = aisgl_max(max_pos->y,tmp.y);
                max_pos->z = aisgl_max(max_pos->z,tmp.z);
            }
        }

        for (n = 0; n < node->mNumChildren; ++n)
            GetBoundingBox(node->mChildren[n],min_pos,max_pos,cur_matrix);
    }

    void AssimpLoaderMesh::GetBoundingBox(const aiNode *node,aiVector3D *min_pos,aiVector3D *max_pos)
    {
        aiMatrix4x4 root_matrix;

        aiIdentityMatrix4(&root_matrix);

        min_pos->x = min_pos->y = min_pos->z =  1e10f;
        max_pos->x = max_pos->y = max_pos->z = -1e10f;

        GetBoundingBox(node,min_pos,max_pos,root_matrix);
    }

public:

    AssimpLoaderMesh(const OSString &fn,const aiScene *s):filename(fn),scene(s)
    {
        model_data=new ModelData;

        aiVector3D scene_min,scene_max;

        GetBoundingBox(scene->mRootNode,&scene_min,&scene_max);

        float tmp;
        vec smin=pos_to_vec(scene_min);
        vec smax=pos_to_vec(scene_max);

        if(smin.y>smax.y)
        {
            tmp=smin.y;
            smin.y=smax.y;
            smax.y=tmp;
        }

        if(smin.z>smax.z)
        {
            tmp=smin.z;
            smin.z=smax.z;
            smax.z=tmp;
        }

        model_data->bounding_box.minPoint=smin;
        model_data->bounding_box.maxPoint=smax;
    }

    ~AssimpLoaderMesh()=default;

    ModelData *GetModelData(){return model_data;}

private:

    bool Load(const aiMesh *mesh)
    {
        MeshData *md=new MeshData;

        md->name=mesh->mName.C_Str();

        if(mesh->HasPositions())
            VecY2Z(md->position,mesh->mVertices,mesh->mNumVertices);

        if(mesh->HasNormals())
        {
            VecY2Z(md->normal,mesh->mNormals,mesh->mNumVertices);

            if(mesh->HasTangentsAndBitangents())
            {
                VecY2Z(md->tangent,     mesh->mTangents,    mesh->mNumVertices);
                VecY2Z(md->bitangent,   mesh->mBitangents,  mesh->mNumVertices);
            }
        }

        const uint num_color_channels=mesh->GetNumColorChannels();

        if(num_color_channels>0)
        {
            md->colors.SetCount(num_color_channels*4*mesh->mNumVertices);

            uint8 *ctp=md->colors.GetData();

            for(uint i=0;i<num_color_channels;i++)
                ctp=ColorF2B(ctp,mesh->mColors[i],mesh->mNumVertices);
        }

        if(mesh->mNumVertices>0xFFFF)
            Face2Indices(md->indices32,mesh->mFaces,mesh->mNumFaces);
        else
            Face2Indices(md->indices16,mesh->mFaces,mesh->mNumFaces);

        md->bounding_box=GetAABB(md->position);
        model_data->Add(md);
        return(true);
    }

public:

    bool LoadMesh()
    {
        if(!scene->HasMeshes())return(false);

        for(uint i=0;i<scene->mNumMeshes;i++)
            if(!Load(scene->mMeshes[i]))
                return(false);

        return(true);
    }

private:

    bool Load(ModelSceneNode *msn,const aiNode *node)
    {
        msn->name=node->mName.C_Str();
        msn->local_matrix=MatrixRotate(node->mTransformation);

        if(node->mNumMeshes>0)
        {
            msn->mesh_index.SetCount(node->mNumMeshes);
            hgl_cpy(msn->mesh_index.GetData(),node->mMeshes,node->mNumMeshes);
        }

        if(node->mNumChildren<=0)
            return(true);

        for(uint i=0;i<node->mNumChildren;i++)
        {
            ModelSceneNode *sub=new ModelSceneNode;

            if(!Load(sub,node->mChildren[i]))
            {
                delete sub;
                return(false);
            }

            msn->children_node.Add(sub);
        }

        return(true);
    }

public:

    bool LoadScene()
    {
        model_data->root_node=new ModelSceneNode;

        return Load(model_data->root_node,scene->mRootNode);
    }
};//class AssimpLoaderMesh

ModelData *AssimpLoadModel(const OSString &filename)
{
    const aiScene *scene=LoadSceneFromFile(filename);

    if(!scene)return(nullptr);

    AssimpLoaderMesh *alm=new AssimpLoaderMesh(ClipFileMainname(filename),scene);

    ModelData *data=nullptr;

    if(alm->LoadMesh())
    {
        if(alm->LoadScene())
            data=alm->GetModelData();
    }

    delete alm;
    return data;
}