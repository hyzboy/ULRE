#include<hgl/graph/mesh/Primitive.h>
#include<hgl/vk/VKMaterialBindingInstance.h>
#include<hgl/vk/VKShaderMaterialProgram.h>
#include<hgl/vk/VKVertexAttribBuffer.h>
#include<hgl/vk/VKIndexBuffer.h>

#include<hgl/graph/module/MaterialBindingInstanceInternalAccess.h>
#include<hgl/graph/geo/GeometryVertexFormat.h>

namespace hgl::graph{
GeometryDataBuffer::GeometryDataBuffer(const uint32_t c,IndexBuffer *ib,VertexDataManager *_vdm)
{
    vab_count=c;

    vab_list=zero_new<VkBuffer>(vab_count);
    vab_offset=zero_new<VkDeviceSize>(vab_count);

    ibo=ib;
    vdm=_vdm;
}

GeometryDataBuffer::~GeometryDataBuffer()
{
    delete[] vab_offset;
    delete[] vab_list;
}

void GeometryDrawRange::Set(const Geometry *geometry)
{
    if(!geometry)
    {
        data_vertex_count = 0;
        data_index_count = 0;
        vertex_count = 0;
        index_count = 0;
        vertex_offset = 0;
        first_index = 0;
        return;
    }

    // data counts come from geometry (buffer capacity)
    data_vertex_count = (uint32_t)geometry->GetVertexCount();
    data_index_count  = geometry->GetIndexCount();

    // initialize draw counts to data counts by default
    vertex_count    = data_vertex_count;
    index_count     = data_index_count;

    vertex_offset   = geometry->GetVertexOffset();
    first_index     = geometry->GetFirstIndex();
}

Primitive::Primitive(Geometry *r,MaterialBindingInstance *mi,GraphicsPipelinePreRaster *p,GeometryDataBuffer *gdb,const VIL *v)
{
    geometry=r;
    mat_inst=mi;
    vil=v;

    data_buffer=gdb;
    draw_range.Set(geometry);
}

Primitive::~Primitive()
{
    SAFE_CLEAR(data_buffer);
}

bool Primitive::UpdateGeometry()
{
    draw_range.Set(geometry);

    // Clamp draw counts if previously set larger than new data counts
    if(draw_range.vertex_count>draw_range.data_vertex_count)
        draw_range.vertex_count = draw_range.data_vertex_count;

    if(draw_range.index_count>draw_range.data_index_count)
        draw_range.index_count = draw_range.data_index_count;

    // Pulling path does not depend on VAB compatibility checks.
    if (mat_inst)
    {
        if (auto *material = MaterialBindingInstanceInternalAccess::GetShaderMaterialProgram(mat_inst))
        {
            const bool pulling_enabled =
                material->IsPullingEnabled() || material->hasSet(SET_TYPE_VERTEX_STREAMS);

            if (pulling_enabled)
            {
                if (data_buffer)
                {
                    data_buffer->ibo = geometry ? geometry->GetIBO() : nullptr;
                    data_buffer->vdm = geometry ? geometry->GetVDM() : nullptr;
                }
                return true;
            }
        }
    }

    return data_buffer->Update(geometry,vil);
}

bool Primitive::SetVertexStreamSource(VertexAttrib attrib, const IGPUBuffer *gpu, VkDeviceSize offset, VkDeviceSize stride)
{
    if (!gpu)
        return false;

    if(!RangeCheck(attrib))
        return(false);

    auto &slot = vertex_stream_sources[uint32_t(attrib)];
    slot.buffer = gpu;
    slot.offset = offset;
    slot.stride = stride;
    slot.has_override = true;
    return true;
}

bool Primitive::ClearVertexStreamSource(VertexAttrib attrib)
{
    if(!RangeCheck(attrib))
        return(false);

    vertex_stream_sources[uint32_t(attrib)] = VertexStreamSource{};
    return true;
}

bool Primitive::ResolveVertexStreamSource(VertexAttrib attrib, const IGPUBuffer *&gpu, VkDeviceSize &offset, VkDeviceSize &stride) const
{
    gpu = nullptr;
    offset = 0;
    stride = 0;

    if(!RangeCheck(attrib))
        return(false);

    const auto &slot = vertex_stream_sources[uint32_t(attrib)];
    if (slot.has_override)
    {
        if (!slot.buffer)
            return false;

        gpu = slot.buffer;
        offset = slot.offset;
        stride = slot.stride;
        return true;
    }

    if (!geometry)
        return false;

    VAB *vab = geometry->GetVAB(attrib);
    if (!vab)
        return false;

    gpu = vab->GetGPUBuffer();
    if (!gpu)
        return false;

    offset = 0;
    stride = vab->GetStride();
    return true;
}

Primitive *DirectCreatePrimitive(Geometry *geom,MaterialBindingInstance *mi,GraphicsPipelinePreRaster *p,const VIL *explicit_vil)
//用Direct这个前缀是为了区别于MeshManager/WorkObject等路径上的CreateMesh()
{
    if(!geom||!mi)return(nullptr);

    auto *material = MaterialBindingInstanceInternalAccess::GetShaderMaterialProgram(mi);
    if(!material)return(nullptr);

    const bool mesh_pipeline = false;

    const VIL *vil = explicit_vil
                   ? explicit_vil
                   : (p ? p->GetVIL() : nullptr);

    const bool pulling_enabled =
        material->IsPullingEnabled() || material->hasSet(SET_TYPE_VERTEX_STREAMS);

    const VIL *pipeline_vil = p ? p->GetVIL() : nullptr;

    if(p && explicit_vil && pipeline_vil && *explicit_vil!=*pipeline_vil)
        return(nullptr);

    if(p && !explicit_vil && pipeline_vil && *vil!=*pipeline_vil)
        return(nullptr);

    const uint32_t input_count = (pulling_enabled || !vil) ? 0u : vil->GetVertexAttribCount();
    const AnsiString &mtl_name=material->GetName();
    const GeometryVertexFormat &geometry_vertex_format = geom->GetGeometryVertexFormat();

    if(geom->GetVABCount()<input_count)        //小于材质要求的数量？那自然是不行的
    {
        GLogError("[FATAL ERROR] input buffer count of Primitive lesser than ShaderMaterialProgram, ShaderMaterialProgram name: "+mtl_name);
        GLogError("  Geometry VAB count: "+AnsiString::numberOf(geom->GetVABCount())+", ShaderMaterialProgram VIL attrib count: "+AnsiString::numberOf(input_count));

        // 输出材质需求的所有顶点属性
        {
            const VertexInputFormat *vif_list=vil->GetVIFList();
            GLogError("  ShaderMaterialProgram requires vertex attribs:");
            for(uint32_t i=0;i<input_count;i++)
            {
                const char *name=GetVertexAttribName(vif_list[i].attrib);
                GLogError("    ["+AnsiString::numberOf(i)+"] "+AnsiString(name)
                         +" format="+GetVulkanFormatName(vif_list[i].format)
                         +" binding="+AnsiString::numberOf(vif_list[i].binding));
            }
        }

        // 输出Geometry能提供的所有顶点属性
        {
            GLogError("  Geometry provides vertex attribs:");
            uint32_t slot_index=0;
            for(uint8_t va=static_cast<uint8_t>(VertexAttrib::BEGIN_RANGE);va<=static_cast<uint8_t>(VertexAttrib::END_RANGE);++va)
            {
                const VertexAttrib attrib=static_cast<VertexAttrib>(va);
                if(geometry_vertex_format.Has(attrib))
                {
                    const char *name=GetVertexAttribName(attrib);
                    GLogError("    ["+AnsiString::numberOf(slot_index)+"] "+AnsiString(name)
                             +" format="+GetVulkanFormatName(geometry_vertex_format.GetFormat(attrib)));
                    ++slot_index;
                }
            }
        }

        return(nullptr);
    }

    const VertexInputFormat *vif = (input_count > 0 && vil) ? vil->GetVIFList() : nullptr;

    uint32_t max_binding=0;
    for(uint i=0;i<input_count;i++)
    {
        if(vif[i].binding>max_binding)
            max_binding=vif[i].binding;
    }

    const uint32_t bind_count = (input_count > 0) ? (max_binding + 1) : 0;
    GeometryDataBuffer *geom_data_buffer=new GeometryDataBuffer(bind_count,geom->GetIBO(),geom->GetVDM());

    VAB *vab;

    for(uint i=0;i<input_count;i++)
    {
        //注: VIF来自于材质，但VAB来自于Geometry。
        //    两个并不一定一样，排序也不一定一样。所以不能让PRIMTIVE直接提供BUFFER_LIST/OFFSET来搞一次性绑定。

        const char *vab_name=GetVertexAttribName(vif->attrib);

        if(!geometry_vertex_format.Has(vif->attrib))
        {
            GLogError("[FATAL ERROR] Geometry missing attrib \""+AnsiString(vab_name)+
                      AnsiString("\" required by ShaderMaterialProgram: ")+mtl_name);
            return(nullptr);
        }

        if(geometry_vertex_format.GetFormat(vif->attrib)!=vif->format)
        {
            GLogError(  "[FATAL ERROR] Geometry attrib format mismatch for \""+AnsiString(vab_name)+
                        AnsiString("\", ShaderMaterialProgram(")+mtl_name+
                        AnsiString(") Format(")+GetVulkanFormatName(vif->format)+
                        AnsiString(") , Geometry Format(")+GetVulkanFormatName(geometry_vertex_format.GetFormat(vif->attrib))+
                        ")");
            return(nullptr);
        }

        vab=geom->GetVAB(vif->attrib);

        if(!vab)
        {
            GLogError("[FATAL ERROR] not found VAB \""+AnsiString(vab_name)+"\" in ShaderMaterialProgram: "+mtl_name);
            return(nullptr);
        }

        if(vab->GetFormat()!=vif->format)
        {
            GLogError(  "[FATAL ERROR] VAB \""+AnsiString(vab_name)+
                        AnsiString("\" format can't match Primitive, ShaderMaterialProgram(")+mtl_name+
                        AnsiString(") Format(")+GetVulkanFormatName(vif->format)+
                        AnsiString(") , VAB Format(")+GetVulkanFormatName(vab->GetFormat())+
                        ")");
            return(nullptr);
        }

        if(vab->GetStride()!=vif->stride)
        {
            GLogError(  "[FATAL ERROR] VAB \""+AnsiString(vab_name)+
                        AnsiString("\" stride can't match Primitive, ShaderMaterialProgram(")+mtl_name+
                        AnsiString(") stride(")+AnsiString::numberOf(vif->stride)+
                        AnsiString(") , VAB stride(")+AnsiString::numberOf(vab->GetStride())+
                        ")");
            return(nullptr);
        }

        const uint32_t bind_index=vif->binding;
        geom_data_buffer->vab_list[bind_index]=vab->GetVkBuffer();
        geom_data_buffer->vab_offset[bind_index]=0;
        ++vif;
    }

    return(new Primitive(geom,mi,p,geom_data_buffer,vil));
}

bool GeometryDataBuffer::Update(const Geometry *geom,const VIL *vil)
{
    if(!geom||!vil)
        return(false);

    const GeometryVertexFormat &geometry_vertex_format = geom->GetGeometryVertexFormat();

    ibo=geom->GetIBO();
    vdm=geom->GetVDM();

    if(vab_count==0)
        return(true);

    for(uint i=0;i<vab_count;i++)
    {
        vab_list[i]=VK_NULL_HANDLE;
        vab_offset[i]=0;
    }

    const uint32_t input_count=vil->GetVertexAttribCount();
    const VertexInputFormat *vif=vil->GetVIFList();

    for(uint i=0;i<input_count;i++)
    {
        if(!geometry_vertex_format.Has(vif->attrib))
            return(false);

        if(geometry_vertex_format.GetFormat(vif->attrib)!=vif->format)
            return(false);

        VAB *vab = geom->GetVAB(vif->attrib);
        if(!vab)
            return(false);

        if(vab->GetFormat()!=vif->format)
            return(false);

        if(vab->GetStride()!=vif->stride)
            return(false);

        if(vif->binding<vab_count)
        {
            vab_list[vif->binding]=vab->GetVkBuffer();
            vab_offset[vif->binding]=0;
        }

        ++vif;
    }

    return(true);
}

// Primitive draw control APIs
bool Primitive::SetDrawCounts(uint32_t draw_vertex_count,uint32_t draw_index_count)
{
    // only clamp, do not change offsets
    if(draw_vertex_count>draw_range.data_vertex_count)
        draw_vertex_count = draw_range.data_vertex_count;

    if(draw_index_count>draw_range.data_index_count)
        draw_index_count = draw_range.data_index_count;

    draw_range.vertex_count = draw_vertex_count;
    draw_range.index_count  = draw_index_count;

    return true;
}

bool Primitive::SetDrawRange(int32_t vertex_offset,uint32_t first_index,uint32_t draw_vertex_count,uint32_t draw_index_count)
{
    // set offsets
    draw_range.vertex_offset = vertex_offset;
    draw_range.first_index   = first_index;

    // clamp counts to data counts
    if(draw_vertex_count>draw_range.data_vertex_count)
        draw_vertex_count = draw_range.data_vertex_count;

    if(draw_index_count>draw_range.data_index_count)
        draw_index_count = draw_range.data_index_count;

    draw_range.vertex_count = draw_vertex_count;
    draw_range.index_count  = draw_index_count;

    return true;
}

}//namespace hgl::graph
