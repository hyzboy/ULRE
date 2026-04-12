#include<hgl/graph/geo/FormatAwareWriter.h>
#include<hgl/graph/geo/GeometryCreater.h>

namespace hgl::graph::inline_geometry
{
    FormatAwareWriter::FormatAwareWriter(GeometryCreater *gc,
                                         const InlineGeoFormatPreset p)
        : creater(gc), preset(p)
    {
    }

    bool FormatAwareWriter::WritePosition(float x,float y,float z)
    {
        if(!creater)
            return false;

        auto pos = creater->GetBufferAccessor<BufferAccessor3f>(VAN::Position);

        if(!pos.IsValid())
            return false;

        pos->Write(x,y,z);
        return true;
    }

    bool FormatAwareWriter::WriteNormal(float x,float y,float z)
    {
        if(!creater)
            return false;

        // Commit 1: 默认 Legacy 透传
        auto nrm = creater->GetBufferAccessor<BufferAccessor3f>(VAN::Normal);

        if(!nrm.IsValid())
            return false;

        nrm->Write(x,y,z);
        return true;
    }

    bool FormatAwareWriter::WriteTangent(float x,float y,float z)
    {
        if(!creater)
            return false;

        // Commit 1: 默认 Legacy 透传
        auto tan = creater->GetBufferAccessor<BufferAccessor3f>(VAN::Tangent);

        if(!tan.IsValid())
            return false;

        tan->Write(x,y,z);
        return true;
    }

    bool FormatAwareWriter::WriteUV(float u,float v)
    {
        if(!creater)
            return false;

        // Commit 1: 默认 Legacy 透传
        auto uv = creater->GetBufferAccessor<BufferAccessor2f>(VAN::TexCoord);

        if(!uv.IsValid())
            return false;

        uv->Write(u,v);
        return true;
    }

    bool FormatAwareWriter::WriteNormalTangentUV(float nx,float ny,float nz,
                                                 float tx,float ty,float tz,
                                                 float u,float v)
    {
        // Commit 1: 保持 Legacy 逐属性写入
        const bool n_ok = WriteNormal(nx,ny,nz);
        const bool t_ok = WriteTangent(tx,ty,tz);
        const bool u_ok = WriteUV(u,v);

        return n_ok && t_ok && u_ok;
    }
}//namespace hgl::graph::inline_geometry
