#include<hgl/graph/material/Material.h>
#include<hgl/io/FileInputStream.h>
#include<hgl/io/DataInputStream.h>
#include<hgl/log/LogInfo.h>

BEGIN_MATERIAL_NAMESPACE
    //bool LoadMaterialFile(MaterialData &md,const OSString &filename)//,Texture **tex_list)
    //{
    //    io::FileInputStream fis;
    //    io::LEDataInputStream dis(&fis);

    //    if(!fis.Open(filename))
    //        RETURN_FALSE;

    //    uint8 flag[9];

    //    if(dis.ReadFully(flag,9)!=9)
    //        RETURN_FALSE;

    //    if(memcmp(flag,"Material\x1A",9))
    //        RETURN_FALSE;

    //    uint8 ver;

    //    if(!dis.ReadUint8(ver))RETURN_FALSE;

    //    if(ver!=1)RETURN_FALSE;

    //    if(dis.ReadFloat(md.diffuse,3)!=3)RETURN_FALSE;
    //    if(dis.ReadFloat(md.specular,3)!=3)RETURN_FALSE;
    //    if(dis.ReadFloat(md.ambient,3)!=3)RETURN_FALSE;
    //    if(dis.ReadFloat(md.emission,3)!=3)RETURN_FALSE;

    //    if(!dis.ReadFloat(md.shininess))RETURN_FALSE;
    //    if(!dis.ReadBool(md.wireframe))RETURN_FALSE;
    //    if(!dis.ReadBool(md.two_sided))RETURN_FALSE;

    //    if(!dis.ReadUint8(md.tex_count))RETURN_FALSE;

    //    md.Init(md.tex_count);

    //    for(uint8 i=0;i<md.tex_count;i++)
    //    {
    //        MaterialTextureData *mtd=&(md.tex_list[i]);

    //        uint8 tt;
    //        if(!dis.ReadUint8(tt))RETURN_FALSE;
    //        mtd->type=(TextureType)tt;

    //        if(!dis.ReadInt32(mtd->tex_id))RETURN_FALSE;
    //        if(!dis.ReadUint8(mtd->uvindex))RETURN_FALSE;
    //        if(!dis.ReadFloat(mtd->blend))RETURN_FALSE;
    //        if(!dis.ReadUint8(mtd->op))RETURN_FALSE;

    //        uint8 wm[2];
    //        if(!dis.ReadUint8(wm,2))RETURN_FALSE;
    //        hgl_typecpy(mtd->wrap_mode,wm,2);

    //        //mtl.SetTexture(mtd->type,tex_list[mtd->tex_id]);
    //    }

    //    return(true);
    //}
END_MATERIAL_NAMESPACE
