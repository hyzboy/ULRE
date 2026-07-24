#include<hgl/graph/mesh/Primitive.h>
#include<hgl/vk/VKMaterialInstance.h>
#include<hgl/vk/VKMaterial.h>
#include<hgl/vk/VKVertexAttribBuffer.h>
#include<hgl/vk/VKIndexBuffer.h>
#include<hgl/graph/geo/GeometryVertexFormat.h>
#include<hgl/vk/VKFormat.h>

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

Primitive::Primitive(Geometry *r,MaterialInstance *mi,Pipeline *p,GeometryDataBuffer *gdb)
{
    geometry=r;
    pipeline=p;
    mat_inst=mi;
    binding_set=nullptr;

    data_buffer=gdb;
    draw_range.Set(geometry);
}

Primitive::Primitive(Geometry *r,DescriptorBindingSet *dbs,Pipeline *p,GeometryDataBuffer *gdb)
{
    geometry=r;
    pipeline=p;
    mat_inst=nullptr;
    binding_set=dbs;

    data_buffer=gdb;
    draw_range.Set(geometry);
}

bool Primitive::UpdateGeometry()
{
    draw_range.Set(geometry);

    // Clamp draw counts if previously set larger than new data counts
    if(draw_range.vertex_count>draw_range.data_vertex_count)
        draw_range.vertex_count = draw_range.data_vertex_count;

    if(draw_range.index_count>draw_range.data_index_count)
        draw_range.index_count = draw_range.data_index_count;

    const VIL *vil = mat_inst ? mat_inst->GetVIL() : (binding_set ? binding_set->GetVIL() : nullptr);
    return data_buffer->Update(geometry, vil ? vil->GetVIFList() : nullptr, vil ? vil->GetVertexAttribCount() : 0);
}

Primitive *DirectCreatePrimitive(Geometry *geom,MaterialInstance *mi,Pipeline *p)
//用Direct这个前缀是为了区别于MeshManager/WorkObject等路径上的CreateMesh()
{
    if(!geom||!mi||!p)return(nullptr);

    const VIL *vil=mi->GetVIL();

    if(*vil!=*p->GetVIL())
        return(nullptr);

    const uint32_t input_count=vil->GetVertexAttribCount();
    const AnsiString &mtl_name=mi->GetMaterial()->GetName();
    const GeometryVertexFormatMatch match_result=MatchGeometryVertexFormat(
                                                    geom->GetGeometryVertexFormat(),
                                                    vil->GetVIFList(),
                                                    vil->GetVertexAttribCount());

    if(geom->GetVABCount()<input_count)        //小于材质要求的数量？那自然是不行的
    {
        GLogError("[FATAL ERROR] input buffer count of Primitive lesser than Material, Material name: "+mtl_name);

        return(nullptr);
    }

    if(!match_result.IsDirectBindSatisfied())
    {
        const GeometryVertexFailureSummary failure_summary = match_result.BuildFailureSummary();
        const GeometryVertexAttributeMatch *first_issue = failure_summary.first_failure;

        // R09 behavioral boundary: ALL non-Exact paths return nullptr.
        // ExplicitHandling (Compatible/Unsupported) is still a hard failure here because no
        // conversion executor has been wired in. A future R10+ path may handle Compatible
        // by forking to an explicit pre-processing step, but only after an explicit sign-off.
        if(first_issue)
        {
            GLogError(  "[FATAL ERROR] GeometryVertexFormat can't satisfy Material vertex input, Material(")+mtl_name+
                        AnsiString(") Attribute(")+AnsiString(GetVertexSemanticName(first_issue->semantic))+
                        AnsiString(") Match(")+GetGeometryVertexMatchKindName(first_issue->kind)+
                        AnsiString(") Semantic(")+GetVertexSemanticName(first_issue->semantic)+
                        AnsiString(") MaterialFormat(")+(GetVulkanFormatName(first_issue->material_format)?GetVulkanFormatName(first_issue->material_format):"Unknown")+
                        AnsiString(") GeometryFormat(")+(GetVulkanFormatName(first_issue->geometry_format)?GetVulkanFormatName(first_issue->geometry_format):"Unknown")+
                        AnsiString(") HasCompatibilityRule(")+(first_issue->has_compatibility_rule?"true":"false")+
                        AnsiString(") AutoApply(")+(first_issue->compatibility_allow_auto_apply?"true":"false")+
                        AnsiString(") Lossless(")+(first_issue->compatibility_lossless?"true":"false")+
                        AnsiString(") Precision(")+GetAttributePrecisionGradeName(first_issue->compatibility_precision)+
                        AnsiString(") RuntimeCost(")+GetAttributeRuntimeCostName(first_issue->compatibility_runtime_cost)+
                        AnsiString(") RequiresExplicitHandling(")+(failure_summary.requires_explicit_handling?"true":"false")+
                        AnsiString(") HandlingPath(")+failure_summary.GetHandlingPathName()+
                        AnsiString(") HasOnlyRegisteredCompatibilityDifferences(")+(failure_summary.has_only_registered_compatibility_differences?"true":"false")+
                        AnsiString(") FailureKind(")+failure_summary.GetFailureKindName()+
                        AnsiString(") HasMismatch(")+(failure_summary.has_mismatch?"true":"false")+
                        AnsiString(") HasUnsupported(")+(failure_summary.has_unsupported?"true":"false")+
                        AnsiString(") HasCompatible(")+(failure_summary.has_compatible?"true":"false")+
                        AnsiString(") FirstMismatchSemantic(")+(failure_summary.first_mismatch?GetVertexSemanticName(failure_summary.first_mismatch->semantic):"None")+
                        AnsiString(") FirstUnsupportedSemantic(")+(failure_summary.first_unsupported?GetVertexSemanticName(failure_summary.first_unsupported->semantic):"None")+
                        AnsiString(") FirstCompatibleSemantic(")+(failure_summary.first_compatible?GetVertexSemanticName(failure_summary.first_compatible->semantic):"None")+
                        AnsiString(") HasAnyRegisteredCompatibility(")+(failure_summary.first_registered_compatibility?"true":"false")+
                        AnsiString(")");
        }
        else
        {
            GLogError("[FATAL ERROR] GeometryVertexFormat can't satisfy Material vertex input, Material: "+mtl_name);
        }

        return(nullptr);
    }

    const VertexInputFormat *vif=vil->GetVIFList();

    uint32_t max_binding=0;
    for(uint i=0;i<input_count;i++)
    {
        if(vif[i].binding>max_binding)
            max_binding=vif[i].binding;
    }

    GeometryDataBuffer *geom_data_buffer=new GeometryDataBuffer(max_binding+1,geom->GetIBO(),geom->GetVDM());

    VAB *vab;

    for(uint i=0;i<input_count;i++)
    {
        //注: VIF来自于材质，但VAB来自于Geometry。
        //    两个并不一定一样，排序也不一定一样。所以不能让PRIMTIVE直接提供BUFFER_LIST/OFFSET来搞一次性绑定。

        vab=geom->GetVAB(vif->semantic);

        if(!vab)
        {
            GLogError("[FATAL ERROR] not found VAB \""+AnsiString(GetVertexSemanticName(vif->semantic))+"\" in Material: "+mtl_name);
            return(nullptr);
        }

        const uint32_t bind_index=vif->binding;
        geom_data_buffer->vab_list[bind_index]=vab->GetVkBuffer();
        geom_data_buffer->vab_offset[bind_index]=0;
        ++vif;
    }

    return(new Primitive(geom,mi,p,geom_data_buffer));
}

Primitive *DirectCreatePrimitive(Geometry *geom,DescriptorBindingSet *dbs,Pipeline *p)
{
    return DirectCreatePrimitive(geom, dbs ? dbs->GetMaterial() : nullptr, dbs, p);
}

Primitive *DirectCreatePrimitive(Geometry *geom,Material *material,DescriptorBindingSet *dbs,Pipeline *p)
{
    if(!geom||!material||!dbs||!p)return(nullptr);

    const VIL *vil = dbs->GetVIL();

    if(!material||!vil)
        return(nullptr);

    if(!dbs->HasRequiredContractBindings(material->GetBindingContract(), material->GetName().c_str()))
        return(nullptr);

    if(*vil!=*p->GetVIL())
        return(nullptr);

    const uint32_t input_count=vil->GetVertexAttribCount();
    const AnsiString &mtl_name=material->GetName();
    const GeometryVertexFormatMatch match_result=MatchGeometryVertexFormat(
                                                    geom->GetGeometryVertexFormat(),
                                                    vil->GetVIFList(),
                                                    vil->GetVertexAttribCount());

    if(geom->GetVABCount()<input_count)
    {
        GLogError("[FATAL ERROR] input buffer count of Primitive lesser than Material, Material name: "+mtl_name);
        return(nullptr);
    }

    if(!match_result.IsDirectBindSatisfied())
    {
        const GeometryVertexFailureSummary failure_summary = match_result.BuildFailureSummary();
        const GeometryVertexAttributeMatch *first_issue = failure_summary.first_failure;

        if(first_issue)
        {
            GLogError(  "[FATAL ERROR] GeometryVertexFormat can't satisfy Material vertex input, Material(")+mtl_name+
                        AnsiString(") Attribute(")+AnsiString(GetVertexSemanticName(first_issue->semantic))+
                        AnsiString(") Match(")+GetGeometryVertexMatchKindName(first_issue->kind)+
                        AnsiString(") Semantic(")+GetVertexSemanticName(first_issue->semantic)+
                        AnsiString(") MaterialFormat(")+(GetVulkanFormatName(first_issue->material_format)?GetVulkanFormatName(first_issue->material_format):"Unknown")+
                        AnsiString(") GeometryFormat(")+(GetVulkanFormatName(first_issue->geometry_format)?GetVulkanFormatName(first_issue->geometry_format):"Unknown")+
                        AnsiString(") HasCompatibilityRule(")+(first_issue->has_compatibility_rule?"true":"false")+
                        AnsiString(") AutoApply(")+(first_issue->compatibility_allow_auto_apply?"true":"false")+
                        AnsiString(") Lossless(")+(first_issue->compatibility_lossless?"true":"false")+
                        AnsiString(") Precision(")+GetAttributePrecisionGradeName(first_issue->compatibility_precision)+
                        AnsiString(") RuntimeCost(")+GetAttributeRuntimeCostName(first_issue->compatibility_runtime_cost)+
                        AnsiString(") RequiresExplicitHandling(")+(failure_summary.requires_explicit_handling?"true":"false")+
                        AnsiString(") HandlingPath(")+failure_summary.GetHandlingPathName()+
                        AnsiString(") HasOnlyRegisteredCompatibilityDifferences(")+(failure_summary.has_only_registered_compatibility_differences?"true":"false")+
                        AnsiString(") FailureKind(")+failure_summary.GetFailureKindName()+
                        AnsiString(") HasMismatch(")+(failure_summary.has_mismatch?"true":"false")+
                        AnsiString(") HasUnsupported(")+(failure_summary.has_unsupported?"true":"false")+
                        AnsiString(") HasCompatible(")+(failure_summary.has_compatible?"true":"false")+
                        AnsiString(") FirstMismatchSemantic(")+(failure_summary.first_mismatch?GetVertexSemanticName(failure_summary.first_mismatch->semantic):"None")+
                        AnsiString(") FirstUnsupportedSemantic(")+(failure_summary.first_unsupported?GetVertexSemanticName(failure_summary.first_unsupported->semantic):"None")+
                        AnsiString(") FirstCompatibleSemantic(")+(failure_summary.first_compatible?GetVertexSemanticName(failure_summary.first_compatible->semantic):"None")+
                        AnsiString(") HasAnyRegisteredCompatibility(")+(failure_summary.first_registered_compatibility?"true":"false")+
                        AnsiString(")");
        }
        else
        {
            GLogError("[FATAL ERROR] GeometryVertexFormat can't satisfy Material vertex input, Material: "+mtl_name);
        }

        return(nullptr);
    }

    const VertexInputFormat *vif=vil->GetVIFList();
    uint32_t max_binding=0;
    for(uint i=0;i<input_count;i++)
    {
        if(vif[i].binding>max_binding)
            max_binding=vif[i].binding;
    }

    GeometryDataBuffer *geom_data_buffer=new GeometryDataBuffer(max_binding+1,geom->GetIBO(),geom->GetVDM());

    VAB *vab;
    for(uint i=0;i<input_count;i++)
    {
        vab=geom->GetVAB(vif->semantic);

        if(!vab)
        {
            GLogError("[FATAL ERROR] not found VAB \""+AnsiString(GetVertexSemanticName(vif->semantic))+"\" in Material: "+mtl_name);
            return(nullptr);
        }

        const uint32_t bind_index=vif->binding;
        geom_data_buffer->vab_list[bind_index]=vab->GetVkBuffer();
        geom_data_buffer->vab_offset[bind_index]=0;
        ++vif;
    }

    return(new Primitive(geom,dbs,p,geom_data_buffer));
}

GeometryVertexFormatMatch QueryGeometryVertexCompatibility(const Geometry *geom, const MaterialInstance *mi)
{
    if (!geom || !mi)
        return GeometryVertexFormatMatch{};

    const VIL *vil = mi->GetVIL();
    if (!vil)
        return GeometryVertexFormatMatch{};

    return MatchGeometryVertexFormat(
        geom->GetGeometryVertexFormat(),
        vil->GetVIFList(),
        vil->GetVertexAttribCount());
}


bool GeometryDataBuffer::Update(const Geometry *geom,const VertexInputFormat *vif_list,uint32_t vif_count)
{
    if(!geom)
        return(false);

    if(vif_count>0 && !vif_list)
        return(false);

    ibo=geom->GetIBO();
    vdm=geom->GetVDM();

    for(uint i=0;i<vab_count;i++)
    {
        vab_list[i]=VK_NULL_HANDLE;
        vab_offset[i]=0;
    }

    for(uint32_t i=0;i<vif_count;i++)
    {
        const VertexInputFormat &vif = vif_list[i];
        if(vif.binding>=0 && uint32_t(vif.binding)<vab_count)
        {
            vab_list[vif.binding]=geom->GetVkBuffer(vif.semantic);
            vab_offset[vif.binding]=0;
        }
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
