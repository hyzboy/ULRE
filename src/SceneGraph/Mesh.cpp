#include<hgl/graph/Mesh.h>
#include<hgl/graph/Material.h>
#include<hgl/graph/VertexBuffer.h>
#include<hgl/io/FileInputStream.h>
#include<hgl/io/DataInputStream.h>
#include<hgl/LogInfo.h>

namespace hgl
{
    namespace graph
    {
        bool LoadMeshFile(MeshFileHeader &md,const OSString &filename,MaterialData *md_list)
        {
            MeshFileHeader mfh;

            io::FileInputStream fis;
            io::LEDataInputStream dis(&fis);

            if(!fis.Open(filename))
                RETURN_FALSE;

            if(dis.ReadFully(&mfh,sizeof(mfh))!=sizeof(mfh))
                RETURN_FALSE;

            if(mfh.texcoord_channels<=0)
                RETURN_FALSE;

            MaterialData *mtd=&(md_list[mfh.material_index]);

            uint8 *uv_comp=nullptr;
            float **uv_data=nullptr;
            VertexBufferCreater **uv_vb=nullptr;

            md.va=new VertexArray(mfh.primitive_type);

            {
                VB3f *vertex=new VB3f(mfh.vertices_number);
                dis.ReadFully(vertex->Begin(),vertex->GetBytes());vertex->End();
                md.va->SetVertex(vertex);
            }

            if(mfh.ntb)
            {
                const uint size=mfh.vertices_number*3*sizeof(float);

                if(mtl->GetLightMode()==HGL_NONE_LIGHT)
                {
                    if(mfh.ntb==NTB_BIT_ALL)
                        dis.Seek(size*3,io::soCurrent);
                    else
                        dis.Seek(size,io::soCurrent);
                }
                else
                {
                    VB3f *normal=new VB3f(mfh.vertices_number);

                    dis.ReadFully(normal->Begin(),normal->GetBytes());normal->End();

                    md.va->SetNormal(normal);

                    if(mfh.ntb==NTB_BIT_ALL)
                        dis.Seek(normal->GetBytes()*2,io::soCurrent);                   //跳过tangent和bitangent
                }
            }

            if(mfh.color_channels)
            {
                VB4ub *color=new VB4ub(mfh.vertices_number);

                dis.ReadFully(color->Begin(),color->GetBytes());color->End();

                md.va->SetColor(color,HGL_PC_RGBA);

                dis.Seek(mfh.vertices_number*4*(mfh.color_channels-1),io::soCurrent);       //跳过vertex color
            }

            if(mfh.texcoord_channels)
            {
                uv_comp=new uint8[mfh.texcoord_channels];
                uv_data=new float *[mfh.texcoord_channels];
                uv_vb=new VertexBufferBase *[mfh.texcoord_channels];

                dis.ReadUint8(uv_comp,mfh.texcoord_channels);

                uint32 uv_total=0;
                for(int i=0;i<mfh.texcoord_channels;i++)
                {
                    uv_total+=uv_comp[i];
                    uv_data[i]=new float[uv_comp[i]*mfh.vertices_number*sizeof(float)];
                }

                for(int i=0;i<mfh.texcoord_channels;i++)
                {
                    dis.ReadFloat(uv_data[i],uv_comp[i]*mfh.vertices_number);

                    if(uv_comp[i]==1)
                        uv_vb[i]=new VB1f(mfh.vertices_number,uv_data[i]);
                    else
                        if(uv_comp[i]==2)
                            uv_vb[i]=new VB2f(mfh.vertices_number,uv_data[i]);
                        else
                            uv_vb[i]=nullptr;
                }
            }

            if(mfh.vertices_number>0xFFFF)
            {
                VB4u32 *face=new VB4u32(mfh.faces_number*3);

                dis.ReadFully(face->Begin(),face->GetBytes());face->End();

                md.va->SetIndex(face);
            }
            else
            {
                VB4u16 *face=new VB4u16(mfh.faces_number*3);

                dis.ReadFully(face->Begin(),face->GetBytes());face->End();

                md.va->SetIndex(face);
            }

            fis.Close();

            for(int i=0;i<mtd->tex_count;i++)
            {
                MaterialTextureData *mt=&(mtd->tex_list[i]);

                md.va->SetVertexBuffer(VertexBufferType(mt->type-mtcDiffuse+vbtDiffuseTexCoord),uv_vb[mt->uvindex]);
            }

            md.r=new Renderable(md.va,mtl);

            for(int i=0;i<mtd->tex_count;i++)
            {
                MaterialTextureData *mt=&(mtd->tex_list[i]);

                md.r->SetTexCoord(mt->type,VertexBufferType(mt->type-mtcDiffuse+vbtDiffuseTexCoord));
            }

            SAFE_CLEAR_OBJECT_ARRAY(uv_data,mfh.texcoord_channels);
            //SAFE_CLEAR_OBJECT_ARRAY(uv_vb,mfh.texcoord_channels);
            SAFE_CLEAR_ARRAY(uv_comp);

            md.r->AutoCreateShader(true,nullptr,filename);

            return(true);
        }
    }//namespace graph
}//namespace hgl
