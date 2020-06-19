#include<hgl/graph/data/SceneNodeData.h>
#include<hgl/graph/NTB.h>
#include<hgl/filesystem/FileSystem.h>
#include<hgl/log/LogInfo.h>

namespace hgl
{
    namespace graph
    {
        namespace
        {
            constexpr char FILE_HEADER_STRING[]="StaticMesh\x1A";
            constexpr uint FILE_HEADER_STRLEN=sizeof(FILE_HEADER_STRING)-1;
        }//namespace

        namespace
        {
            struct LoadModelData
            {
                const uint8 *sp;
                ModelData *md;

            private:

                bool CheckFileHeader()
                {
                    if(memcmp(sp,FILE_HEADER_STRING,FILE_HEADER_STRLEN))     //比较文件头
                        return(false);

                    sp+=FILE_HEADER_STRLEN;

                    md->version=*sp++;

                    if(md->version!=1)
                        return(false);

                    LOG_INFO(OS_TEXT("Load StaticMesh file begin, ver:")+OSString::valueOf(version));
                    return(true);
                }

                bool LoadMeshNames(const uint32 mesh_count)
                {
                    uint8 len;
                    UTF8String str;

                    const uint32 total_bytes=*(uint32 *)sp;
                    sp+=sizeof(uint32);

                    for(uint32 i=0;i<mesh_count;i++)
                    {
                        len=*sp++;

                        str.SetString((char *)sp,len);

                        md->mesh_name.Add(str);

                        LOG_INFO(U8_TEXT("\tMesh ")+UTF8String::valueOf(i)+U8_TEXT(" : ")+str);

                        sp+=len;
                    }

                    return(true);
                }

                void Load(AABB &box)
                {
                    box.minPoint.x=*(float *)sp;sp+=sizeof(float);
                    box.minPoint.y=*(float *)sp;sp+=sizeof(float);
                    box.minPoint.z=*(float *)sp;sp+=sizeof(float);
                    box.maxPoint.x=*(float *)sp;sp+=sizeof(float);
                    box.maxPoint.y=*(float *)sp;sp+=sizeof(float);
                    box.maxPoint.z=*(float *)sp;sp+=sizeof(float);
                }

                bool LoadMesh(MeshData *mesh)
                {
                    LOG_INFO(U8_TEXT("Load Mesh ")+UTF8String::valueOf(mesh->index)+U8_TEXT(" : ")+md->mesh_name[mesh->index]);

                    mesh->ntb=*sp++;

                    mesh->vertex_count=*(uint32 *)sp;
                    sp+=sizeof(uint32);
                    
                    LOG_INFO(OS_TEXT("\tVertex: ")+OSString::valueOf(mesh->vertex_count));

                    mesh->position=(float *)sp;
                    sp+=sizeof(float)*mesh->vertex_count;

                    if(mesh->ntb&NTB_BIT_NORMAL)
                    {
                        mesh->normal=(float *)sp;
                        sp+=sizeof(float)*mesh->vertex_count;

                        LOG_INFO(OS_TEXT("\thas normal"));
                    }

                    if(mesh->ntb&NTB_BIT_TANGENT)
                    {
                        mesh->tangent=(float *)sp;
                        sp+=sizeof(float)*mesh->vertex_count;

                        LOG_INFO(OS_TEXT("\thas tangent"));
                    }

                    if(mesh->ntb&NTB_BIT_BINORMAL)
                    {
                        mesh->binormal=(float *)sp;
                        sp+=sizeof(float)*mesh->vertex_count;

                        LOG_INFO(OS_TEXT("\thas bitangent/binormal"));
                    }

                    {
                        mesh->color_count=*sp++;
                        LOG_INFO(OS_TEXT("\tColor: ")+OSString::valueOf(mesh->vertex_count));

                        mesh->colors=new uint8 *[mesh->color_count];
                        for(uint i=0;i<mesh->color_count;i++)
                        {
                            mesh->colors[i]=(uint8 *)sp;
                            sp+=4*mesh->vertex_count;
                        }
                    }

                    {
                        mesh->uv_count=*sp++;
                        LOG_INFO(OS_TEXT("\tUV: ")+OSString::valueOf(mesh->uv_count));
                        mesh->uv_component=sp;
                        sp+=mesh->uv_count;

                        mesh->uv=new float *[mesh->uv_count];
                        for(uint i=0;i<mesh->uv_count;i++)
                        {
                            mesh->uv[i]=(float *)sp;
                            sp+=sizeof(float)*mesh->uv_component[i]*mesh->vertex_count;

                            LOG_INFO(OS_TEXT("\t\tUV ")+OSString::valueOf(i)+OS_TEXT(" : ")+OSString::valueOf(mesh->uv_component[i]));
                        }
                    }

                    {
                        mesh->indices_count=*(uint32 *)sp;
                        sp+=sizeof(uint32);

                        LOG_INFO(OS_TEXT("\tFaces: ")+OSString::valueOf(mesh->indices_count/3));

                        mesh->indices=(void *)sp;
                        
                        if(mesh->vertex_count>0xFFFF)
                            sp+=sizeof(uint32)*mesh->indices_count;
                        else
                            sp+=sizeof(uint16)*mesh->indices_count;
                    }

                    Load(mesh->bounding_box);
                    return(true);
                }

            public:

                LoadModelData(uint8 *source,const uint8 *s)
                {
                    sp=s;
                    md=new ModelData;
                    md->source_filedata=(uint8 *)source;
                }

                bool Load()
                {
                    if(!CheckFileHeader())
                        return(false);

                    const uint32 mesh_count=*(uint32 *)sp;
                    sp+=sizeof(uint32);

                    LOG_INFO(OS_TEXT("Mesh Count: ")+OSString::valueOf(mesh_count));

                    if(!LoadMeshNames(mesh_count))
                        return(false);

                    for(uint32 i=0;i<mesh_count;i++)
                    {
                        MeshData *mesh=new MeshData;

                        mesh->index=i;

                        if(!LoadMesh(mesh))
                        {
                            delete mesh;
                            return(false);
                        }

                        md->mesh_list.Add(mesh);
                    }

                    return(true);
                }
            };//struct LoadModelData
        }//namespace

        ModelData *LoadModelFromFile(const OSString &filename)
        {
            if(filename.IsEmpty())
                return(nullptr);

            int64 filelength;
            uint8 *filedata=(uint8 *)filesystem::LoadFileToMemory(filename,filelength);
            const uint8 *sp=filedata;

            if(!filedata)
                return(nullptr);

            LoadModelData lmd(filedata,sp);

            if(!lmd.Load())
            {
                delete lmd.md;
                return nullptr;
            }

            return(lmd.md);
        }
    }//namespace graph
}//namespace hgl
