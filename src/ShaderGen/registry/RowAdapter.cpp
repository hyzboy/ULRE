#include <hgl/shadergen/RegistryQuery.h>
#include <hgl/shadergen/ColorSource.h>
#include <hgl/shadergen/ColorSourcePipeline.h>
#include <cstdio>

namespace hgl::graph::mtl
{

    MaterialVariantRow ComposeMaterialVariantRow(const VertexProgramTemplate *vertex,
                                             const SurfaceFragmentTemplate *fragment,
                                             const PipelineStateRow *pipeline,
                                             const MaterialVariantKey &key,
                                             const char *debug_name)
    {
        MaterialVariantRow row;

        row.name = debug_name ? debug_name : "ComposedRow";
        row.preset = fragment ? fragment->preset : (vertex ? vertex->preset : MaterialPreset::PureColor);
        row.factory_type = row.preset;
        row.surface_type = key.surface_type;
        row.geometry_mode = key.geometry_mode;
        row.position_provider = key.position_provider;
        row.blend = key.blend_mode;
        row.pass = key.pass_hint;
        // lighting_model / sky_ambient_model are ECS-injected via MaterialVariantKey; not stored in row.resources.

        if (vertex)
        {
            row.vertex_input = vertex->vertex_input;
            row.vertex_policy = vertex->vertex_policy;
            row.vs_template_path = vertex->vs_template_path;
            row.vs_features = vertex->vs_features;
            row.resources = vertex->resource_contract;
            row.def_hint = vertex->def_hint;
        }

        if (fragment)
        {
            row.preset = fragment->preset;
            row.factory_type = fragment->preset;
            row.surface_type = fragment->surface_type;
            row.surface_model = fragment->surface_model;
            row.fs_template_path = fragment->fs_template_path;
            row.surface_path = fragment->surface_path;
            row.fs_features = fragment->fs_features;
            row.resources.enable_lighting = fragment->resource_contract.enable_lighting;
            // needs_sky and lighting_model are SFM-driven / ECS-injected; not propagated from fragment contract.
            row.resources.needs_material_instance = row.resources.needs_material_instance || fragment->resource_contract.needs_material_instance;
            row.def_hint = fragment->def_hint != StaticMaterialDefIdHint::None ? fragment->def_hint : row.def_hint;
            if (fragment->schema != ShaderDataSchema::None)
                row.schema = fragment->schema;
        }

        if (pipeline)
            row.primitive = pipeline->primitive;

        // Build color_sources directly from the key's texture source modes.
        for (uint32 i = 0; i < uint32(SamplerSlot::RANGE_SIZE); ++i)
        {
            const auto slot = static_cast<SamplerSlot>(i);
            const TextureSourceMode mode = key.GetTextureSourceMode(slot);
            if (mode == TextureSourceMode::None)
                continue;
            if (mode == TextureSourceMode::Array)
                row.color_sources.push_back(graph::ColorSource::MakeSampler2DArray(slot));
            else
                row.color_sources.push_back(graph::ColorSource::MakeSampler2D(slot));
        }

        // G1 + G2 校验：通过统一管道验证槽位合法性并确认 binding 无冲突
        const auto pipe = graph::FinalizeColorSources(row.color_sources, row.name);
        if (!pipe.ok)
        {
            for (const auto &d : pipe.diags)
                fprintf(stderr, "[ColorSource] %s\n", d.message.c_str());
        }

        return row;
    }
}
