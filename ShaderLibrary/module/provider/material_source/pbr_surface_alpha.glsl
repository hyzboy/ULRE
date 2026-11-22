#ifndef HGL_PBR_SURFACE_ALPHA_GLSL
#define HGL_PBR_SURFACE_ALPHA_GLSL

float EvalMaterialAlpha(MaterialSourceInput source_input)
{
    PBRSurfaceRow material_data = MTL_ROW(source_input.dataIndex);

    return clamp(material_data.base_color.a, 0.0, 1.0)
         * SampleOptional(
               MTL_TEX(source_input.dataIndex).tex_opacity_mask,
               LinearSampler,
               source_input.surface.uv0,
               vec4(1.0)).r;
}

#endif
