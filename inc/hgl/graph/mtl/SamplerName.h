#pragma once

namespace hgl
{
    namespace graph
    {
        namespace mtl
        {
            namespace SamplerName 
            {
                constexpr const char BaseColor[] = "TextureBaseColor";
                constexpr const char Text[] = "TextureText";
                
                // Terrain heightmap samplers
                constexpr const char Heightmap[] = "HeightmapTexture";
                constexpr const char BlendWeights[] = "BlendWeightsTexture";
                constexpr const char ColorArray[] = "ColorTextureArray";
                constexpr const char NormalArray[] = "NormalTextureArray";
            }//namespace SamplerName
        }//namespace mtl
    }//namespace graph
}//namespace hgl
