#include<hgl/graph/adapter/GeometryAdapter.h>
#include<hgl/graph/GeometryCreater.h>
#include<hgl/graph/VKGeometry.h>
#include<hgl/graph/VertexAttribDataAccess.h>
#include<vector>

namespace hgl::graph
{
    Geometry* ToVulkanGeometry(
        const data::GeometryData &geom_data,
        GeometryCreater *pc,
        const AnsiString &name)
    {
        if(!pc)
            return nullptr;

        if(!geom_data.Validate())
            return nullptr;

        const auto &layout = geom_data.layout;
        const size_t vertex_count = geom_data.GetVertexCount();
        const size_t index_count = geom_data.GetIndexCount();

        if(vertex_count == 0)
            return nullptr;

        // Initialize GeometryCreater
        if(!pc->Init(name, vertex_count, index_count))
            return nullptr;

        // Write position data (required)
        if(layout.has_position)
        {
            std::vector<float> positions;
            positions.reserve(vertex_count * 3);
            
            for(const auto &v : geom_data.vertices)
            {
                positions.push_back(v.position.x);
                positions.push_back(v.position.y);
                positions.push_back(v.position.z);
            }
            
            if(!pc->WriteVAB(VAN::Position, VF_V3F, positions.data()))
                return nullptr;
        }

        // Write normal data (optional)
        if(layout.has_normal)
        {
            std::vector<float> normals;
            normals.reserve(vertex_count * 3);
            
            for(const auto &v : geom_data.vertices)
            {
                normals.push_back(v.normal.x);
                normals.push_back(v.normal.y);
                normals.push_back(v.normal.z);
            }
            
            pc->WriteVAB(VAN::Normal, VF_V3F, normals.data());
        }

        // Write tangent data (optional)
        if(layout.has_tangent)
        {
            std::vector<float> tangents;
            tangents.reserve(vertex_count * 3);
            
            for(const auto &v : geom_data.vertices)
            {
                tangents.push_back(v.tangent.x);
                tangents.push_back(v.tangent.y);
                tangents.push_back(v.tangent.z);
            }
            
            pc->WriteVAB(VAN::Tangent, VF_V3F, tangents.data());
        }

        // Write texture coordinate data (optional)
        if(layout.has_texcoord)
        {
            std::vector<float> texcoords;
            texcoords.reserve(vertex_count * 2);
            
            for(const auto &v : geom_data.vertices)
            {
                texcoords.push_back(v.texcoord.x);
                texcoords.push_back(v.texcoord.y);
            }
            
            pc->WriteVAB(VAN::TexCoord, VF_V2F, texcoords.data());
        }

        // Write color data (optional)
        if(layout.has_color)
        {
            std::vector<float> colors;
            colors.reserve(vertex_count * 4);
            
            for(const auto &v : geom_data.vertices)
            {
                colors.push_back(v.color.r);
                colors.push_back(v.color.g);
                colors.push_back(v.color.b);
                colors.push_back(v.color.a);
            }
            
            pc->WriteVAB(VAN::Color, VF_V4F, colors.data());
        }

        // Write index data
        if(index_count > 0)
        {
            pc->WriteIBO(geom_data.indices.data());
        }

        // Create Geometry object
        Geometry *geom = pc->Create();
        if(geom)
        {
            // Convert AABB to BoundingVolumes
            BoundingVolumes bv;
            bv.SetFromAABB(geom_data.bounds);
            geom->SetBoundingVolumes(bv);
        }

        return geom;
    }
}
