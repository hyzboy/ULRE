#include"AssimpLoader.h"
#include<assimp/postprocess.h>
#include<assimp/cimport.h>
#include<hgl/filesystem/FileSystem.h>
#include<hgl/type/List.h>
#include<hgl/io/MemoryOutputStream.h>
#include<hgl/graph/TextureType.h>

using namespace hgl;
using namespace hgl::graph;

namespace
{
    #define LOG_BR  LOG_INFO(OS_TEXT("------------------------------------------------------------"));

    #define aisgl_min(x,y) (x<y?x:y)
    #define aisgl_max(x,y) (y>x?y:x)

    const Color4f COLOR_PURE_BLACK(0,0,0,1);
    const Color4f COLOR_PURE_WHITE(1,1,1,1);
}

void AssimpLoader::get_bounding_box_for_node (const aiNode* nd,
                                                aiVector3D* min,
                                                aiVector3D* max,
                                                aiMatrix4x4* trafo)
{
    aiMatrix4x4 prev;
    unsigned int n = 0, t;

    prev = *trafo;
    aiMultiplyMatrix4(trafo,&nd->mTransformation);

    for (; n < nd->mNumMeshes; ++n)
    {
        const aiMesh* mesh = scene->mMeshes[nd->mMeshes[n]];

        for (t = 0; t < mesh->mNumVertices; ++t)
        {
            aiVector3D tmp = mesh->mVertices[t];
            aiTransformVecByMatrix4(&tmp,trafo);

            min->x = aisgl_min(min->x,tmp.x);
            min->y = aisgl_min(min->y,tmp.y);
            min->z = aisgl_min(min->z,tmp.z);

            max->x = aisgl_max(max->x,tmp.x);
            max->y = aisgl_max(max->y,tmp.y);
            max->z = aisgl_max(max->z,tmp.z);
        }
    }

    for (n = 0; n < nd->mNumChildren; ++n)
        get_bounding_box_for_node(nd->mChildren[n],min,max,trafo);

    *trafo = prev;
}

void AssimpLoader::get_bounding_box (const aiNode *node,aiVector3D* min, aiVector3D* max)
{
    aiMatrix4x4 trafo;

    aiIdentityMatrix4(&trafo);

    min->x = min->y = min->z =  1e10f;
    max->x = max->y = max->z = -1e10f;

    get_bounding_box_for_node(node,min,max,&trafo);
}

AssimpLoader::AssimpLoader()
{
    scene=0;
    material_list=nullptr;
}

AssimpLoader::~AssimpLoader()
{
    delete[] material_list;

    if(scene)
        aiReleaseImport(scene);
}

//void AssimpLoader::SaveFile(const void *data,const uint &size,const OSString &ext_name)
//{
//    if(!data||size<=0)return;
//
//    OSString filename=main_filename+OS_TEXT(".")+ext_name;
//
//    if(filesystem::SaveMemoryToFile(filename,data,size)==size)
//    {
//        LOG_INFO(OS_TEXT("SaveFile: ")+filename+OS_TEXT(" , size: ")+OSString(size));
//
//        total_file_bytes+=size;
//    }
//}
//
//void AssimpLoader::SaveFile(void **data,const int64 *size,const int &count,const OSString &ext_name)
//{
//    if(!data||size<=0)return;
//
//    OSString filename=main_filename+OS_TEXT(".")+ext_name;
//
//    int64 result=filesystem::SaveMemoryToFile(filename,data,size,count);
//
//    if(result>0)
//    {
//        LOG_INFO(OS_TEXT("SaveFile: ")+filename+OS_TEXT(" , size: ")+OSString(result));
//
//        total_file_bytes+=result;
//    }
//}
//
//void OutFloat3(const OSString &front,const Color4f &c)
//{
//    LOG_INFO(front+OS_TEXT(": ")+OSString(c.r)
//                  +OS_TEXT("," )+OSString(c.g)
//                  +OS_TEXT("," )+OSString(c.b));
//}

constexpr VkSamplerAddressMode vk_wrap[4]=              //对应aiTextureMapMode,注意最后一个其实不对
{
    VK_SAMPLER_ADDRESS_MODE_REPEAT,
    VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
    VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
    VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE        //这个是declas
};

//void OutMaterialTexture(const MaterialTextureData &mt,io::DataOutputStream *dos)
//{
//    dos->WriteUint8((uint8)mt.type);
//    dos->WriteInt32(mt.tex_id);
//    dos->WriteUint8(mt.uvindex);
//    dos->WriteFloat(mt.blend);
//    dos->WriteUint8(mt.op);
//    dos->WriteUint8(mt.wrap_mode,2);
//
//    LOG_INFO(OS_TEXT("\tTexture Type: ")+OSString((uint)mt.type));
//    LOG_INFO(OS_TEXT("\tTexture ID: ")+OSString(mt.tex_id));
//
//    LOG_INFO(OS_TEXT("\tuvindex: ")+OSString(mt.uvindex));
//    LOG_INFO(OS_TEXT("\tblend: ")+OSString(mt.blend));
//    LOG_INFO(OS_TEXT("\top: ")+OSString(mt.op));
//    LOG_INFO(OS_TEXT("\twrap_mode: ")+OSString(mt.wrap_mode[0])+OS_TEXT(",")+OSString(mt.wrap_mode[1]));
//}
//
//void OutMaterial(const MaterialData *ms,const OSString &filename)
//{
//    io::FileOutputStream fos;
//    io::LEDataOutputStream dos(&fos);
//
//    fos.CreateTrunc(filename);
//
//    dos.WriteFully("Material\x1A\x1",10);
//    dos.WriteFloat(ms->diffuse,3);
//    dos.WriteFloat(ms->specular,3);
//    dos.WriteFloat(ms->ambient,3);
//    dos.WriteFloat(ms->emission,3);
//    dos.WriteFloat(ms->shininess);
//    dos.WriteBool(ms->wireframe);
//    dos.WriteBool(ms->two_sided);
//    dos.WriteUint8(ms->tex_count);
//
//    OutFloat3(OSString(OS_TEXT("diffuse")),ms->diffuse);
//    OutFloat3(OSString(OS_TEXT("specular")),ms->specular);
//    OutFloat3(OSString(OS_TEXT("ambient")),ms->ambient);
//    OutFloat3(OSString(OS_TEXT("emission")),ms->emission);
//
//    LOG_INFO(OS_TEXT("shininess: ")+OSString(ms->shininess));
//
//    LOG_INFO(OS_TEXT("wireframe: ")+OSString(ms->wireframe?OS_TEXT("true"):OS_TEXT("false")));
//    LOG_INFO(OS_TEXT("two_sided: ")+OSString(ms->two_sided?OS_TEXT("true"):OS_TEXT("false")));
//
//    LOG_INFO(OS_TEXT("Material Texture Count ")+OSString(ms->tex_count));
//
//    for(int i=0;i<ms->tex_count;i++)
//        OutMaterialTexture(ms->tex_list[i],&dos);
//
//    fos.Close();
//}

bool GetMaterialBool(const aiMaterial *mtl,const char *key,unsigned int type,unsigned int index,const bool default_value)
{
    int value;

    if(aiGetMaterialInteger(mtl, key,type,index, &value)==AI_SUCCESS)
        return value;
    else
        return default_value;
}

uint GetMaterialInteger(const aiMaterial *mtl,const char *key,unsigned int type,unsigned int index,const uint default_value)
{
    int value;

    if(aiGetMaterialInteger(mtl, key,type,index, &value)==AI_SUCCESS)
        return value;
    else
        return default_value;
}

float GetMaterialFloat(const aiMaterial *mtl,const char *key,unsigned int type,unsigned int index,const float default_value)
{
    float value;

    if(aiGetMaterialFloat(mtl, key,type,index, &value)==AI_SUCCESS)
        return value;
    else
        return default_value;
}

Color4f GetMaterialColor(const aiMaterial *mtl,const char *key,unsigned int type,unsigned int index,const Color4f &default_value)
{
    unsigned int max = 1;
    aiColor4D color;

    if(aiGetMaterialColor(mtl, key,type,index, &color)==AI_SUCCESS)
        return Color4f(color.r,color.g,color.b,color.a);
    else
        return default_value;
}

//bool AssimpLoader::Convert(MaterialData &md,aiMaterial *mtl)
//{
//    uint tex_count=0;
//    uint tt_count=0;
//    int32 tex_id;
//    aiReturn found=AI_SUCCESS;
//
//    md.name=mtl->GetName().C_Str();
//
//    for(uint tt=aiTextureType_DIFFUSE;tt<aiTextureType_UNKNOWN;tt++)
//        tex_count+=mtl->GetTextureCount((aiTextureType)tt);
//
//    md.Init(tex_count);
//
//    MaterialTextureData *mtd=md.tex_list;
//
//    for(uint tt=aiTextureType_DIFFUSE;tt<aiTextureType_UNKNOWN;tt++)
//    {
//        tt_count=mtl->GetTextureCount((aiTextureType)tt);
//
//        for(int t=0;t<tt_count;t++)
//        {
//            aiString filename;
//            aiTextureMapping tm;
//            uint uvindex=0;
//            float blend;
//            aiTextureOp op;
//            aiTextureMapMode wrap_mode[2];
//
//            mtl->GetTexture((aiTextureType)tt,t,&filename,&tm,&uvindex,&blend,&op,wrap_mode);
//
//            mtd->type=(TextureType)tt;
//
//            md.uv_use.Add(uvindex);
//
//            {
//                char *fn=strrchr(filename.data,'\\');
//                char *ext;
//
//                if(!fn)
//                    fn=strrchr(filename.data,'/');
//                else
//                    ++fn;
//
//                if(fn)
//                    ++fn;
//                else
//                    fn=filename.data;
//
//                ext=strrchr(fn,'.');
//
//                if(ext)*ext=0;
//
//            #if HGL_OS == HGL_OS_Window
//                tex_id=tex_list.CaseFind(fn);
//            #else
//                tex_id=tex_list.Find(fn);
//            #endif//
//
//                if(tex_id==-1)
//                    tex_id=tex_list.Add(fn);
//
//                mtd->tex_id=tex_id;
//            }
//
//            mtd->uvindex=uvindex;
//            mtd->blend=blend;
//            mtd->op=op;
//
//            mtd->wrap_mode[0]=vk_wrap[wrap_mode[0]];
//            mtd->wrap_mode[1]=vk_wrap[wrap_mode[1]];
//
//            ++mtd;
//        }
//    }
//
//    md.two_sided           =GetMaterialBool    (mtl,AI_MATKEY_TWOSIDED,        true);
//    md.shading_model       =GetMaterialInteger (mtl,AI_MATKEY_SHADING_MODEL,   aiShadingMode_Phong);
//    md.wireframe           =GetMaterialBool    (mtl,AI_MATKEY_ENABLE_WIREFRAME,false);
//    //        md.blend_func          =
//    md.opacity             =GetMaterialFloat(mtl,AI_MATKEY_OPACITY,            0);
//    //        md.transparency_factor =
//
//    md.bump_scaling        =GetMaterialFloat(mtl,AI_MATKEY_BUMPSCALING,        0);
//    md.shininess           =GetMaterialFloat(mtl,AI_MATKEY_SHININESS,          0);
//    md.shininess_strength  =GetMaterialFloat(mtl,AI_MATKEY_SHININESS_STRENGTH, 0);
//    md.reflectivity        =GetMaterialFloat(mtl,AI_MATKEY_REFLECTIVITY,       0);
//    md.refracti            =GetMaterialFloat(mtl,AI_MATKEY_REFRACTI,           0);
//
//    md.diffuse             =GetMaterialColor(mtl,AI_MATKEY_COLOR_DIFFUSE,      COLOR_PURE_WHITE);
//    md.ambient             =GetMaterialColor(mtl,AI_MATKEY_COLOR_AMBIENT,      Color4f(0.2,0.2,0.2,1.0));
//    md.specular            =GetMaterialColor(mtl,AI_MATKEY_COLOR_SPECULAR,     COLOR_PURE_BLACK);
//    md.emission            =GetMaterialColor(mtl,AI_MATKEY_COLOR_EMISSIVE,     COLOR_PURE_BLACK);
//
//    md.transparent         =GetMaterialColor(mtl,AI_MATKEY_COLOR_TRANSPARENT,  COLOR_PURE_BLACK);
//    md.reflective          =GetMaterialColor(mtl,AI_MATKEY_COLOR_REFLECTIVE,   COLOR_PURE_BLACK);
//
//    //OutMaterial(ms,main_filename+OS_TEXT(".")+OSString(m)+OS_TEXT(".material"));
//    LOG_BR;
//}

//void AssimpLoader::LoadMaterial()
//{
//    if(!scene->HasMaterials())return;
//
//    LOG_BR;
//    LOG_INFO(OS_TEXT("Material Count ")+OSString(scene->mNumMaterials));
//    LOG_BR;
//
//    aiString filename;
//    aiTextureMapping tm;
//    uint uvindex=0;
//    float blend;
//    aiTextureOp op;
//    aiTextureMapMode wrap_mode[2];
//
//    material_count=scene->mNumMaterials;
//
//    material_list=new MaterialData[material_count];
//
//    for(uint m=0;m<scene->mNumMaterials;m++)
//        Convert(material_list[m],scene->mMaterials[m]);
//}

//void AssimpLoader::SaveTextures()
//{
//    const int tex_count=tex_list.GetCount();
//
//    LOG_INFO(OS_TEXT("TOTAL Texture Count: ")+OSString(tex_count));
//
//    if(tex_count<=0)
//        return;
//
//    for(int i=0;i<tex_count;i++)
//        LOG_INFO(U8_TEXT("\t")+UTF8String(i)+U8_TEXT("\t")+tex_list[i]);
//
//    LOG_BR;
//
//    io::MemoryOutputStream mos;
//    io::LEDataOutputStream dos(&mos);
//
//    SaveUTF8StringList(&dos,tex_list);
//
//    SaveFile(mos.GetData(),mos.Tell(),OS_TEXT("tex_list"));
//}
//
//template<typename T>
//void AssimpLoader::SaveFaces(io::FileOutputStream *fos,const aiFace *face,const T count)
//{
//    T *data=new T[count*3];
//
//    const aiFace *sp=face;
//    T *tp=data;
//
//    for(T i=0;i<count;i++)
//    {
//        *tp=sp->mIndices[0];++tp;
//        *tp=sp->mIndices[1];++tp;
//        *tp=sp->mIndices[2];++tp;
//
//        ++sp;
//    }
//
//    fos->WriteFully(data,sizeof(T)*3*count);
//
//    delete[] data;
//}
//
//void AssimpLoader::SaveTexCoord(float *tp,const aiVector3D *texcoord,const uint comp,const uint count)
//{
//    if(comp<=0||comp>3)
//        return;
//
//    const uint size=1+sizeof(float)*comp*count;
//
//    const aiVector3D *sp=texcoord;
//
//    if(comp==1)
//    {
//        for(uint i=0;i<count;i++)
//        {
//            *tp=sp->x;++tp;
//            ++sp;
//        }
//    }
//    else
//    if(comp==2)
//    {
//        for(uint i=0;i<count;i++)
//        {
//            *tp=sp->x;++tp;
//            *tp=sp->y;++tp;
//            ++sp;
//        }
//    }
//    else
//    if(comp==3)
//    {
//        for(uint i=0;i<count;i++)
//        {
//            *tp=sp->x;++tp;
//            *tp=sp->y;++tp;
//            *tp=sp->z;++tp;
//            ++sp;
//        }
//    }
//}

void c4f_to_4b(uint8 *tp,const aiColor4D *c4,const int count)
{
    for(int i=0;i<count;i++)
    {
        *tp=c4->r*255;++tp;
        *tp=c4->g*255;++tp;
        *tp=c4->b*255;++tp;
        *tp=c4->a*255;++tp;
    }
}

void AxisY2Z(aiVector3D *v,const uint32 count)
{
    ai_real t;

    for(uint32 i=0;i<count;i++)
    {
        t=v->y;
        v->y=-(v->z);
        v->z=t;

        ++v;
    }
}

void AssimpLoader::LoadMesh()
{
    if(!scene->HasMeshes())return;

    UTF8String mesh_name;

    total_file_bytes=0;

    LOG_BR;
    LOG_INFO(OS_TEXT("Mesh Count ")+OSString(scene->mNumMeshes));

    uint vertex_total=0;
    uint tri_total=0;
    uint bone_total=0;

    for(unsigned int i=0;i<scene->mNumMeshes;i++)
    {
        const aiMesh *mesh=scene->mMeshes[i];

        const uint v3fdata_size=mesh->mNumVertices*3*sizeof(float);
        const uint v4fdata_size=mesh->mNumVertices*4*sizeof(float);

        if(mesh->mName.length)
            mesh_name=U8_TEXT("Mesh [")+UTF8String(i)+U8_TEXT(":")+UTF8String(mesh->mName.C_Str())+U8_TEXT("]");
        else
            mesh_name=U8_TEXT("Mesh [")+UTF8String(i)+U8_TEXT("]");

        int pn=mesh->mFaces[0].mNumIndices;

        LOG_BR;

        LOG_INFO(mesh_name+U8_TEXT(" PrimitiveTypes is ")+UTF8String(mesh->mPrimitiveTypes));

        LOG_INFO(mesh_name+U8_TEXT(" vertex color count is ")+UTF8String(mesh->GetNumColorChannels()));
        LOG_INFO(mesh_name+U8_TEXT(" texcoord count is ")+UTF8String(mesh->GetNumUVChannels()));

        if(mesh->GetNumUVChannels()>0)
            LOG_INFO(mesh_name+U8_TEXT(" Material Index is ")+UTF8String(mesh->mMaterialIndex));

        if(mesh->HasPositions())LOG_INFO(mesh_name+U8_TEXT(" have Position, vertices count ")+UTF8String(mesh->mNumVertices));
        if(mesh->HasFaces())LOG_INFO(mesh_name+U8_TEXT(" have Faces,faces count ")+UTF8String(mesh->mNumFaces));
        if(mesh->HasNormals())LOG_INFO(mesh_name+U8_TEXT(" have Normals"));
        if(mesh->HasTangentsAndBitangents())LOG_INFO(mesh_name+U8_TEXT(" have Tangent & Bitangents"));
        if(mesh->HasBones())LOG_INFO(mesh_name+U8_TEXT(" have Bones, Bones Count ")+UTF8String(mesh->mNumBones));

        vertex_total+=mesh->mNumVertices;
        tri_total+=mesh->mNumFaces;
        bone_total+=mesh->mNumBones;

        LOG_INFO(mesh_name+U8_TEXT(" PN is ")+UTF8String(pn));

        if(pn!=3)
            continue;

        MaterialData *mtl=&(material_list[mesh->mMaterialIndex]);

        const uint uv_channels=mtl->uv_use.GetCount();

        LOG_INFO(mesh_name+U8_TEXT(" use UV Channels is ")+UTF8String(uv_channels));

        io::FileOutputStream fos;

        OSString mesh_filename=main_filename+OS_TEXT(".")+OSString(i)+OS_TEXT(".mesh");

        if(!fos.CreateTrunc(mesh_filename))
        {
            LOG_INFO(OS_TEXT("Create Mesh file Failed,filename: ")+mesh_filename);
            continue;
        }

        {
            MeshFileHeader ms;

            ms.primitive_type   =PRIM_TRIANGLES;
            ms.vertices_number  =mesh->mNumVertices;
            ms.faces_number     =mesh->mNumFaces;
            ms.color_channels   =mesh->GetNumColorChannels();
            ms.texcoord_channels=uv_channels;
            ms.material_index   =mesh->mMaterialIndex;

            if(mesh->HasNormals())
            {
                if(mesh->HasTangentsAndBitangents())
                    ms.ntb=NTB_BIT_ALL;
                else
                    ms.ntb=NTB_BIT_NORMAL;
            }

            ms.bones_number     =mesh->mNumBones;

            fos.WriteFully(&ms,sizeof(MeshFileHeader));
        }

        if(mesh->HasPositions())
        {
            AxisY2Z(mesh->mVertices,mesh->mNumVertices);
            fos.WriteFully(mesh->mVertices,v3fdata_size);
        }

        if(mesh->HasNormals())
        {
            AxisY2Z(mesh->mNormals,mesh->mNumVertices);
            fos.WriteFully(mesh->mNormals,v3fdata_size);

            if(mesh->HasTangentsAndBitangents())
            {
                AxisY2Z(mesh->mTangents,mesh->mNumVertices);
                AxisY2Z(mesh->mBitangents,mesh->mNumVertices);

                fos.WriteFully(mesh->mTangents,v3fdata_size);
                fos.WriteFully(mesh->mBitangents,v3fdata_size);
            }
        }

        if(mesh->GetNumColorChannels()>0)
        {
            const int64 vertex_color_size=mesh->mNumVertices*4*mesh->GetNumColorChannels();

            uint8 *vertex_color=new uint8[vertex_color_size];
            uint8 *tp=vertex_color;

            for(int c=0;c<mesh->GetNumColorChannels();c++)
            {
                c4f_to_4b(tp,mesh->mColors[c],mesh->mNumVertices);
                tp+=mesh->mNumVertices*4;
            }

            fos.WriteFully(vertex_color,vertex_color_size);

            delete[] vertex_color;
        }

        if(mtl->uv_use.GetCount()>0
         &&mesh->GetNumUVChannels())
        {
            int tc=0;
            int comp_total=0;
            uint *uv_use=mtl->uv_use.GetData();

            //这里要重新审视数据来源，并不是每一个纹理通道都有数据，并且和材质对应。
            //、、材质中的uv index具体对应啥 还不是很清楚

            for(int c=0;c<AI_MAX_NUMBER_OF_TEXTURECOORDS;c++)
            {
                if(uv_use[c]>=mesh->GetNumUVChannels())
                    uv_use[c]=0;

                comp_total+=mesh->mNumUVComponents[uv_use[c]];
            }

            comp_total*=mesh->mNumVertices;

            uint64 tc_size=mesh->GetNumUVChannels()+comp_total*sizeof(float);

            uint8 *tc_data=new uint8[tc_size];
            float *tp=(float *)(tc_data+mesh->GetNumUVChannels());
            for(int c=0;c<uv_channels;c++)
            {
                tc_data[c]=mesh->mNumUVComponents[uv_use[c]];

//                SaveTexCoord(tp,mesh->mTextureCoords[c],mesh->mNumUVComponents[c],mesh->mNumVertices);

                tp+=mesh->mNumUVComponents[c]*mesh->mNumVertices;
            }

            fos.WriteFully(tc_data,tc_size);

            delete[] tc_data;
        }

        if(mesh->HasFaces())
        {
            //if(mesh->mNumVertices>0xFFFF)
            //    SaveFaces<uint32>(&fos,mesh->mFaces,mesh->mNumFaces);
            //else
            //    SaveFaces<uint16>(&fos,mesh->mFaces,mesh->mNumFaces);
        }

        fos.Close();
    }

    LOG_BR;
    LOG_INFO(OS_TEXT("TOTAL Vertices Count ")+OSString(vertex_total));
    LOG_INFO(OS_TEXT("TOTAL Faces Count ")+OSString(tri_total));
    LOG_INFO(OS_TEXT("TOTAL Bones Count ")+OSString(bone_total));

    LOG_INFO(OS_TEXT("TOTAL File Size ")+OSString(total_file_bytes));
    LOG_BR;
}

void AssimpLoader::LoadScene(const UTF8String &front,io::DataOutputStream *dos,const aiNode *nd)
{
    aiVector3D bb_min,bb_max;
    float box[6];
    get_bounding_box(nd,&bb_min,&bb_max);

    box[0]=bb_min.x;box[1]=bb_min.y;box[2]=bb_min.z;
    box[3]=bb_max.x;box[4]=bb_max.y;box[5]=bb_max.z;

    dos->WriteUTF8String(nd->mName.C_Str());
    dos->WriteFloat(box,6);

    dos->WriteFloat((float *)&(nd->mTransformation),16);

    dos->WriteUint32(nd->mNumMeshes);

    LOG_INFO(front+U8_TEXT("Node[")+UTF8String(nd->mName.C_Str())+U8_TEXT("][Mesh:")+UTF8String(nd->mNumMeshes)+U8_TEXT("][SubNode:")+UTF8String(nd->mNumChildren)+U8_TEXT("]"))

    const UTF8String new_front=U8_TEXT("  ")+front;

    if(nd->mNumMeshes>0)
    {
        dos->WriteUint32(nd->mMeshes,nd->mNumMeshes);

        for(unsigned int i=0;i<nd->mNumMeshes;i++)
        {
            const aiMesh *mesh=scene->mMeshes[nd->mMeshes[i]];

            LOG_INFO(new_front+U8_TEXT("Mesh[")+UTF8String(i)+U8_TEXT("] ")+UTF8String(mesh->mName.C_Str()));
        }
    }

    dos->WriteUint32(nd->mNumChildren);

    for(unsigned int n=0;n<nd->mNumChildren;++n)
        LoadScene(new_front,dos,nd->mChildren[n]);
}

bool AssimpLoader::LoadFile(const OSString &filename)
{
    uint filesize;
    char *filedata;

    filesize=filesystem::LoadFileToMemory(filename,(void **)&filedata);

    if(!filedata)
        return(false);

#ifdef _WIN32
    const UTF8String ext_name=ToUTF8String(filesystem::ClipFileExtName(filename));
#else
    const UTF8String ext_name=filesystem::ClipFileExtName(filename);
#endif//

    scene=aiImportFileFromMemory(filedata,filesize,aiProcessPreset_TargetRealtime_MaxQuality|aiProcess_FlipUVs,ext_name.c_str());

    delete[] filedata;

    if(!scene)
        return(false);

    filename.SubString(main_filename,0,filename.FindRightChar(OS_TEXT('.')));

    get_bounding_box(scene->mRootNode,&scene_min,&scene_max);

    scene_center.x = (scene_min.x + scene_max.x) / 2.0f;
    scene_center.y = (scene_min.y + scene_max.y) / 2.0f;
    scene_center.z = (scene_min.z + scene_max.z) / 2.0f;

    //这个顺序是假设了所有的材质和模型都有被使用
    //aiProcessPreset_TargetRealtime_Quality已经指定了会删除多余材质，所以这里不用处理。
    //但多余模型并不确定是否存在

    io::FileOutputStream fos;
    io::LEDataOutputStream dos(&fos);

    OSString scene_filename=main_filename+OS_TEXT(".scene");

    if(!fos.CreateTrunc(scene_filename))
    {
        LOG_INFO(OS_TEXT("Create Scene file Failed,filename: ")+scene_filename);
        return(false);
    }

    dos.WriteFully("SCENE\x1a\x01",7);

    dos.WriteUint32(scene->mNumMaterials);
    dos.WriteUint32(scene->mNumMeshes);

    LoadMaterial();                                     //载入所有材质

    dos.WriteUint32(tex_list.GetCount());

    LoadMesh();                                         //载入所有mesh
    LoadScene(UTF8String(""),&dos,scene->mRootNode);    //载入场景节点

    fos.Close();

    //SaveTextures();

    return(true);
}
