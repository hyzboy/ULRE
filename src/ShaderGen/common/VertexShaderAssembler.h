#pragma once

#include <hgl/mtl/VertexShaderNodeConfig.h>
#include <hgl/mtl/MaterialTransformGraph.h>
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
        bool texture_layer_id_uses_data_index = false; // true: fragTextureLayerID = fragDataIndexID (bindless row semantics)
        bool emit_vertex_color     = false;  // location=2 out vec4 fragVertexColor
        bool emit_uv0              = false;  // location=3 out vec2 fragUV0
        // Extended: for materials that need world-space outputs (e.g. DebugNormalColor)
        // These are appended after the standard varying slots above.
        bool emit_world_pos        = false;  // out vec3 fragWorldPos
        bool emit_world_normal     = false;  // out vec3 fragWorldNormal
        // Extended: special-purpose varying outputs (appended last).
        bool emit_luminance        = false;  // out float fragLuminance (= Luminance vertex attr)
        bool emit_frag_direction   = false;  // out vec3 fragDirection (= normalize(Position)), sky dome
        // Palette-color materials: GetL2W reads the TransformID vertex attribute
        // directly (l2w.mats[TransformID]) instead of resolving gl_InstanceIndex
        // through the l2w_index_rows table.
        bool use_transform_id_attr = false;
        // Vertex color sourced from a MaterialColorPalette UBO instead of a Color
        // vertex attribute: fragVertexColor = color_palette.color[ColorIndex].
        bool emit_vertex_color_from_palette = false;
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
    //  resolved_input_glsl: Full resolver-derived input declarations. When
    //                     non-empty, replaces the legacy Stage-1 declaration.
    //  provider_glsl:      Selected provider module source, emitted after
    //                     vertex declarations and before stage helpers.
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
        const std::string            &shader_lib_path,
        const std::string            &resolved_input_glsl = {},
        const std::string            &provider_glsl = {})
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

        // ── Header ───────────────────────────────────────────────────────────
        append("#version 450\n");
        if (varying_cfg.emit_vertex_color_from_palette)
            append("#extension GL_EXT_scalar_block_layout : require\n");
        append("\n");

        // ── Descriptor macros ─────────────────────────────────────────────────
        vs += "#include \"common/descriptor_macros.glsl\"\n";

        if (needs_camera || needs_viewport)
        {
            if (needs_camera)
            {
                vs += "#include \"ubo/camera_info.glsl\"\n";
                vs += "SCENE_CAMERA_UBO;\n";
            }
            if (needs_viewport)
            {
                vs += "#include \"ubo/viewport_info.glsl\"\n";
                vs += "SCENE_VIEWPORT_UBO;\n";
            }
        }

        if (needs_l2w)
        {
            vs += "#include \"common/l2w_ssbo.glsl\"\n";
            vs += "L2W_SSBO;\n";
        }

        // 行表 SSBO 声明（l2w_index_rows / mtl_data_index_rows / mtl_texture_layer_rows）
        // 不再在此展开：由 CompileCompositorMaterial 依据 descriptor_info 统一生成并
        // 注入到 #version 之后（ResolveTransformID / ResolveDataIndexID / ResolveTextureLayerID
        // 同由该处提供定义）。

        vs += "\n";

        // ── Stage 1: Vertex input ─────────────────────────────────────────────
        if (!resolved_input_glsl.empty())
        {
            vs += resolved_input_glsl;
            if (resolved_input_glsl.back() != '\n')
                vs += "\n";
        }
        else switch (effective_input)
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
        if (!provider_glsl.empty())
        {
            vs += provider_glsl;
            if (provider_glsl.back() != '\n') vs += "\n";
        }

        // MaterialColorPalette UBO (palette-color materials).
        if (varying_cfg.emit_vertex_color_from_palette)
            vs += "layout(scalar, set=MATERIAL_SET, binding=0) uniform ColorPalette { vec4 color[256]; } color_palette;\n";

        vs += "\n";

        // ── Stage 2: Position mapping ─────────────────────────────────────────
        const MaterialTransformGraph transform_graph =
            MaterialTransformGraph::FromNodeConfig(node_cfg);
        const char *stage2_module = transform_graph.GetMappingModulePath();
        if (stage2_module)
        {
            vs += "#include \"" + std::string(stage2_module) + "\"\n";
        }
        else
        {
            vs += "// TODO: TerrainGrid Stage2 not yet implemented\n";
            vs += "vec4 GetLocalPos() { return vec4(0.0); }\n";
        }

        vs += "\n";

        // ── Stage 3: Transform strategy ───────────────────────────────────────
        // Select based on orientation × scale × projection.
        // Palette-color materials resolve L2W from the TransformID vertex
        // attribute instead of the gl_InstanceIndex table; orient_world.glsl
        // branches on this macro.
        if (varying_cfg.use_transform_id_attr)
            vs += "#define HGL_L2W_FROM_VERTEX_ATTR\n";

        vs += "#include \"" + std::string(transform_graph.GetStage3ModulePath()) + "\"\n";

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
        if (varying_cfg.emit_vertex_color || varying_cfg.emit_vertex_color_from_palette)
            vs += "layout(location=" + std::to_string(loc++) + ") out vec4 fragVertexColor;\n";
        if (varying_cfg.emit_frag_direction)
            vs += "layout(location=" + std::to_string(loc++) + ") out vec3 fragDirection;\n";
        if (varying_cfg.emit_luminance)
            vs += "layout(location=" + std::to_string(loc++) + ") out float fragLuminance;\n";

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
        if (varying_cfg.emit_vertex_color || varying_cfg.emit_vertex_color_from_palette)
        {
            if (varying_cfg.emit_vertex_color_from_palette)
                vs += "    fragVertexColor = color_palette.color[ColorIndex];\n";
            else
                vs += "    fragVertexColor = Color;\n";
        }
        if (varying_cfg.emit_uv0)
            vs += "    fragUV0 = TexCoord;\n";
        if (varying_cfg.emit_frag_direction)
            vs += "    fragDirection = normalize(Position);\n";
        if (varying_cfg.emit_luminance)
            vs += "    fragLuminance = Luminance;\n";

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
