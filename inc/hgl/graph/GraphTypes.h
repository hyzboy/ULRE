#pragma once

/**
 * Graph-level forward declarations that are not part of Vulkan-specific headers.
 */
namespace hgl::graph
{
    class GraphModule;

    class BufferManager;
    class RenderPassManager;
    class GeometryManager;
    class ShaderProgramManager;
    class RenderTargetManager;
    class TextureManager;


    class TileData;
    class TextRender;

    struct CameraInfo;
    struct Camera;

    class RenderCollector;
}
