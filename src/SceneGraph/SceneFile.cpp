#include<hgl/graph/SceneDB.h>
#include<hgl/graph/SceneNode.h>
#include<hgl/graph/Material.h>
#include<hgl/graph/Mesh.h>
#include<hgl/io/FileInputStream.h>
#include<hgl/io/DataInputStream.h>
#include<hgl/filesystem/FileSystem.h>
#include<hgl/LogInfo.h>

namespace hgl
{
    namespace graph
    {
        bool LoadMaterialFile(MaterialData &md,const OSString &filename);
        bool LoadMeshFile(MeshFileHeader &md,const OSString &filename,MaterialData *md_list);

        namespace
        {
            class SceneLoader
            {
                SceneDB *db;
                SceneNode *root_node;

                OSString main_filename;

                uint32 tex_count=0;

            private:

                uint32 mtl_count=0;
                MaterialData *md_list;

                void LoadMaterial()
                {
                    md_list=new MaterialData[mtl_count];

                    for(int i=0;i<mtl_count;i++)
                        LoadMaterialFile(md_list[i],main_filename+OS_TEXT(".")+OSString(i)+OS_TEXT(".material"));
                }

            private:

                uint32 mesh_count=0;
                MeshFileHeader *mesh_list;

                void LoadMesh()
                {
                    mesh_list=new MeshFileHeader[mesh_count];

                    for(int i=0;i<mesh_count;i++)
                        LoadMeshFile(mesh_list[i],main_filename+OS_TEXT(".")+OSString(i)+OS_TEXT(".mesh"),md_list);
                }

            private:

                void AddMesh(SceneNode *node,const uint32 *mesh_index,const uint32 count)
                {
                    //for(int i=0;i<count;i++)
                    //    node->Add(mesh_list[mesh_index[i]]);
                }

                bool LoadScene(SceneNode *node,io::DataInputStream *dis)
                {
                    UTF8String node_name;

                    if(!dis->ReadUTF8String(node_name))
                        RETURN_FALSE;

                    //bounding box
                    {
                        float box[6];

                        if(dis->ReadFloat(box,6)!=6)
                            RETURN_FALSE;

                        vec bb_min(box[0],box[1],box[2],1);
                        vec bb_max(box[3],box[4],box[5],1);

                        AABB bb(bb_min,bb_max);

                        node->SetBoundingBox(bb);
                    }

                    //matrix
                    {
                        Matrix4f mat;

                        if(dis->ReadFloat((float *)&mat,16)!=16)
                            RETURN_FALSE;

                        node->SetLocalMatrix(mat);
                    }

                    //mesh
                    {
                        uint32 count;

                        if(!dis->ReadUint32(count))
                            RETURN_FALSE;

                        uint32 *index=new uint32[count];

                        if(dis->ReadUint32(index,count)!=count)
                        {
                            delete[] index;
                            RETURN_FALSE;
                        }

                        AddMesh(node,index,count);

                        delete[] index;
                    }//mesh

                     //children
                    {
                        uint32 children_count;

                        if(!dis->ReadUint32(children_count))
                            RETURN_FALSE;

                        for(uint32 i=0;i<children_count;i++)
                        {
                            SceneNode *sub_node=new SceneNode;

                            if(!LoadScene(sub_node,dis))
                            {
                                delete sub_node;
                                RETURN_FALSE;
                            }

                            node->Add(sub_node);
                        }
                    }//children

                    return(true);
                }

            public:

                SceneLoader(SceneDB *sdb,SceneNode *rn)
                {
                    db=sdb;
                    root_node=rn;
                }

                bool Load(const OSString &mn,io::DataInputStream *dis)
                {
                    main_filename=mn;

                    uint8 flag[6],ver;

                    if(dis->ReadFully(flag,6)!=6)
                        RETURN_FALSE;

                    if(memcmp(flag,"SCENE\x1A",6))
                        RETURN_FALSE;

                    if(!dis->ReadUint8(ver))
                        RETURN_FALSE;

                    if(ver!=1)
                        RETURN_FALSE;

                    if(!dis->ReadUint32(mtl_count))
                        RETURN_FALSE;

                    if(!dis->ReadUint32(tex_count))
                        RETURN_FALSE;

//                    LoadTextures();
                    LoadMaterial();

                    if(!dis->ReadUint32(mesh_count))
                        RETURN_FALSE;

                    LoadMesh();

                    return LoadScene(root_node,dis);
                }
            };//class SceneLoader
        }//namespace

        SceneNode *LoadScene(const OSString &fullname,SceneDB *db)
        {
            io::OpenFileInputStream fis(fullname);

            if(!fis)
                return(nullptr);

            SceneNode *root=new SceneNode;
            io::LEDataInputStream *dis=new io::LEDataInputStream(&fis);

            SceneLoader sl(db,root);

            const OSString filename=filesystem::ClipFileMainname(fullname);

            if(!sl.Load(filename,dis))
            {
                delete root;
                root=nullptr;
            }

            delete dis;
            return root;
        }
    }//namespace graph
}//namespace hgl
