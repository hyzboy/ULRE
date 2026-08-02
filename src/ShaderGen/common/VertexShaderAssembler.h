#pragma once

#include <hgl/mtl/VertexShaderNodeConfig.h>
#include <vulkan/vulkan.h>
#include <string>

namespace hgl::graph::mtl
{
    // VertexVaryingConfig — declares what varying outputs the generated VS emits.
    // These must match the FS template inputs.
    struct VertexVaryingConfig
    {
        bool emit_data_index_id    = false;  // location=0 flat out uint fragDataIndexID
        bool emit_texture_layer_id = false;  // location=1 flat out uint fragTextureLayerID
        bool texture_layer_id_uses_data_index = false; // true: fragTextureLayerID = fragDataIndexID (legacy 3D bindless row semantics)
        bool emit_vertex_color     = false;  // location=2 out vec4 fragVertexColor
        bool emit_uv0              = false;  // location=3 out vec2 fragUV0
        // Extended: for materials that need world-space outputs (e.g. Gizmo3D)
        // These are appended after the standard varying slots above.
        bool emit_world_pos        = false;  // out vec3 fragWorldPos
        bool emit_world_normal     = false;  // out vec3 fragWorldNormal
    };

    // GenerateVertexShader — generates a complete, standalone VS GLSL string.
    //
    //  node_cfg:          Stage 1/2/3 configuration
    //  varying_cfg:       Which varying outputs to emit and pass to FS
    //  position_format:   Vulkan format of the position vertex attribute.
    //                     Used to select between vec2/vec3 Stage-1 files.
    //                     VK_FORMAT_UNDEFINED → fall back to node_cfg.input.
    //  extra_attr_glsl:   Optional additional vertex attribute declarations
    //                     (beyond position), emitted verbatim after Stage-1 include.
    //  shader_lib_path:   Path prefix for #include resolution (usually "ShaderLibrary").
    //
    // Returns the full GLSL source string. If a Stage is not yet implemented,
    // it emits a // TODO comment and a fallback passthrough so the shader at
    // least compiles.
    inline std::string GenerateVertexShader(
        const VertexShaderNodeConfig &node_cfg,
        const VertexVaryingConfig    &varying_cfg,
        VkFormat                      position_format,
        const std::string            &extra_attr_glsl,
        const std::string            &shader_lib_path)
    {
        std::string vs;
        vs.reserve(2048);

        // ── Helpers ──────────────────────────────────────────────────────────
        auto append = [&](const char *s) { vs += s; };

        // Determine effective vertex input mode from position_format or node config.
        VertexInputMode effective_input = node_cfg.input;
        if (position_format == VK_FORMAT_R32G32_SINT || position_format == VK_FORMAT_R32G32_UINT)
            effective_input = VertexInputMode::Vec2IntPosition;
        else if (position_format == VK_FORMAT_R32G32_SFLOAT)
            effective_input = VertexInputMode::Vec2Position;
        else if (position_format == VK_FORMAT_R32G32B32_SFLOAT ||
                 position_format == VK_FORMAT_R32G32B32A32_SFLOAT)
            effective_input = VertexInputMode::Vec3Position;

        // Determine which descriptor resources Stage 3 needs.
        bool needs_camera = (node_cfg.projection == ProjectionMode::WorldCameraVP ||
                             node_cfg.orientation == OrientationMode::CameraFacingFree ||
                             node_cfg.orientation == OrientationMode::CameraFacingAxisY);
        bool needs_viewport = (node_cfg.projection == ProjectionMode::OrthoViewport ||
                               node_cfg.projection == ProjectionMode::OrthoThenLocalToWorld ||
                               node_cfg.scale     == ScaleMode::FixedPixelSize);
        bool needs_l2w = (node_cfg.orientation == OrientationMode::World ||
                          node_cfg.orientation == OrientationMode::CameraFacingFree ||
                          node_cfg.orientation == OrientationMode::CameraFacingAxisY);
        bool needs_mi  = varying_cfg.emit_data_index_id || varying_cfg.emit_texture_layer_id;

        // ── Header ───────────────────────────────────────────────────────────
        append("#version 450\n\n");

        // ── Descriptor macros ─────────────────────────────────────────────────
        vs += "#include \"common/descriptor_macros.glsl\"\n";

        if (needs_camera || needs_viewport)
        {
            vs += "#include \"common/scene_ubo.glsl\"\n";
            if (needs_camera)  vs += "SCENE_CAMERA_UBO;\n";
            if (needs_viewport) vs += "SCENE_VIEWPORT_UBO;\n";
        }

        if (needs_l2w)
        {
            vs += "#include \"common/l2w_ssbo.glsl\"\n";
            vs += "L2W_SSBO;\n";
            vs += "#include \"common/instance_rows_ssbo.glsl\"\n";
            vs += "L2W_INDEX_ROWS_SSBO;\n";
        }

        if (needs_mi)
        {
            if (!needs_l2w)
            {
                vs += "#include \"common/instance_rows_ssbo.glsl\"\n";
            }
            if (varying_cfg.emit_data_index_id) vs += "DATA_INDEX_ROWS_SSBO;\n";
            if (varying_cfg.emit_texture_layer_id && !varying_cfg.texture_layer_id_uses_data_index)
                vs += "TEXTURE_LAYER_ROWS_SSBO;\n";
        }

        vs += "\n";

        // ── Stage 1: Vertex input ─────────────────────────────────────────────
        switch (effective_input)
        {
        case VertexInputMode::Vec2Position:
            vs += "layout(location=0) in vec2 Position;\n";
            break;
        case VertexInputMode::Vec2IntPosition:
            vs += "layout(location=0) in ivec2 Position;\n";
            break;
        case VertexInputMode::Procedural:
            vs += "#include \"vertex/s1_input_procedural.glsl\"\n";
            break;
        case VertexInputMode::Vec3Position:
        default:
            vs += "layout(location=0) in vec3 Position;\n";
            break;
        }

        // ── Extra vertex attributes (color, UV, etc.) ─────────────────────────
        if (!extra_attr_glsl.empty())
        {
            vs += extra_attr_glsl;
            if (extra_attr_glsl.back() != '\n') vs += "\n";
        }

        vs += "\n";

        // ── Stage 2: Position mapping ─────────────────────────────────────────
        switch (node_cfg.position_mapping)
        {
        case PositionMappingMode::LiftXY_XY0:
            vs += "#include \"vertex/s2_lift_xy0.glsl\"\n";
            break;
        case PositionMappingMode::LiftXY_X0Y:
            vs += "#include \"vertex/s2_lift_x0y.glsl\"\n";
            break;
        case PositionMappingMode::LiftXY_0XY:
            vs += "#include \"vertex/s2_lift_0xy.glsl\"\n";
            break;
        case PositionMappingMode::NDCLift:
            vs += "#include \"vertex/s2_ndc_lift.glsl\"\n";
            break;
        case PositionMappingMode::ZeroOneToNDC:
            vs += "#include \"vertex/s2_zeroone_to_ndc.glsl\"\n";
            break;
        case PositionMappingMode::PixelToLocal:
            vs += "#include \"vertex/s2_pixel_to_local.glsl\"\n";
            break;
        case PositionMappingMode::TerrainGrid:
            vs += "// TODO: TerrainGrid Stage2 not yet implemented\n";
            vs += "vec4 GetLocalPos() { return vec4(0.0); }\n";
            break;
        case PositionMappingMode::Passthrough3D:
        default:
            vs += "#include \"vertex/s2_passthrough3d.glsl\"\n";
            break;
        }

        vs += "\n";

        // ── Stage 3: Transform strategy ───────────────────────────────────────
        // Select based on orientation × scale × projection.
        if (node_cfg.orientation == OrientationMode::CameraFacingFree ||
            node_cfg.orientation == OrientationMode::CameraFacingAxisY)
        {
            if (node_cfg.scale == ScaleMode::FixedPixelSize)
                vs += "#include \"vertex/s3_camera_facing_fixed_pixels.glsl\"\n";
            else
                vs += "#include \"vertex/s3_camera_facing_world.glsl\"\n";
        }
        else // OrientationMode::World
        {
            switch (node_cfg.projection)
            {
            case ProjectionMode::LocalToWorldOnly:
                vs += "#include \"vertex/s3_l2w_only.glsl\"\n";
                break;
            case ProjectionMode::OrthoViewport:
                vs += "#include \"vertex/s3_ortho_viewport.glsl\"\n";
                break;
            case ProjectionMode::OrthoThenLocalToWorld:
                vs += "#include \"vertex/s3_ortho_then_l2w.glsl\"\n";
                break;
            case ProjectionMode::ClipPassthrough:
                vs += "#include \"vertex/s3_clip_passthrough.glsl\"\n";
                break;
            case ProjectionMode::WorldCameraVP:
            default:
                vs += "#include \"vertex/s3_world_camera_vp.glsl\"\n";
                break;
            }
        }

        vs += "\n";

        // ── Varying outputs (emission order must match FS input locations) ─────
        // Standard layout:
        //   loc=0  fragDataIndexID    (flat, if MI)
        //   loc=1  fragTextureLayerID (flat, if MI + texture)
        //   loc=2  fragWorldPos       (if world-space outputs)
        //   loc=3  fragWorldNormal    (if world-space outputs)
        //   loc=4  fragUV0            (if UV)
        //   loc=2  fragVertexColor    (if vertex-color; only when no MI, so no conflict)
        uint32_t loc = 0;
        if (varying_cfg.emit_data_index_id)
            vs += "layout(location=" + std::to_string(loc++) + ") flat out uint fragDataIndexID;\n";
        if (varying_cfg.emit_texture_layer_id)
            vs += "layout(location=" + std::to_string(loc++) + ") flat out uint fragTextureLayerID;\n";
        if (varying_cfg.emit_world_pos)
            vs += "layout(location=" + std::to_string(loc++) + ") out vec3 fragWorldPos;\n";
        if (varying_cfg.emit_world_normal)
            vs += "layout(location=" + std::to_string(loc++) + ") out vec3 fragWorldNormal;\n";
        if (varying_cfg.emit_uv0)
            vs += "layout(location=" + std::to_string(loc++) + ") out vec2 fragUV0;\n";
        if (varying_cfg.emit_vertex_color)
            vs += "layout(location=" + std::to_string(loc++) + ") out vec4 fragVertexColor;\n";

        vs += "\nvoid main()\n{\n";

        // Standard varying assignments (MI index IDs, color, UV).
        if (varying_cfg.emit_data_index_id)
            vs += "    fragDataIndexID = ResolveDataIndexID(gl_InstanceIndex);\n";
        if (varying_cfg.emit_texture_layer_id)
        {
            if (varying_cfg.texture_layer_id_uses_data_index)
                vs += "    fragTextureLayerID = fragDataIndexID;\n";
            else
                vs += "    fragTextureLayerID = ResolveTextureLayerID(gl_InstanceIndex);\n";
        }
        if (varying_cfg.emit_vertex_color)
            vs += "    fragVertexColor = Color;\n";
        if (varying_cfg.emit_uv0)
            vs += "    fragUV0 = TexCoord;\n";

        // World-space outputs: compute L2W once and use for both transform and normal.
        if (varying_cfg.emit_world_pos || varying_cfg.emit_world_normal)
        {
            vs += "    mat4 _l2w = GetL2W();\n";
            vs += "    vec4 _world_pos = _l2w * GetLocalPos();\n";
            if (varying_cfg.emit_world_pos)
                vs += "    fragWorldPos = _world_pos.xyz;\n";
            if (varying_cfg.emit_world_normal)
                vs += "    fragWorldNormal = normalize(mat3(_l2w) * Normal);\n";
            // For world-normal path, projection is always WorldCameraVP.
            vs += "    gl_Position = camera.vp * _world_pos;\n";
        }
        else
        {
            vs += "    gl_Position = GetClipPos(GetLocalPos());\n";
        }

        vs += "}\n";

        return vs;
    }

} // namespace hgl::graph::mtl
