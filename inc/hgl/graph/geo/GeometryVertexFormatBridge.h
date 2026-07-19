#pragma once

#include <hgl/graph/geo/GeometryVertexFormat.h>

namespace hgl::graph
{
    inline GeometryVertexFormat BuildGeometryVertexFormatFromVIFList(const VertexInputFormat *vif_list,const uint32_t vif_count)
    {
        GeometryVertexFormat result;

        if(!vif_list||vif_count==0)
            return result;

        for(uint32_t i=0;i<vif_count;++i)
        {
            const VertexInputFormat *vif=vif_list+i;
            result.Add(vif->semantic,vif->format,uint8_t(vif->vec_size),uint32_t(vif->stride));
        }

        return result;
    }
}
