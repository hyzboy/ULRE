#pragma once

#include<hgl/type/String.h>

// glTF-compatible material schema (field names match glTF 2.0)
namespace gltf
{
    struct TextureInfo
    {
        int index = -1;         // textures[index]
        int texCoord = 0;       // set index, default 0
    };

    struct NormalTextureInfo: public TextureInfo
    {
        float scale = 1.0f;     // normal scale
    };

    struct OcclusionTextureInfo: public TextureInfo
    {
        float strength = 1.0f;  // occlusion strength
    };

    struct PBRMetallicRoughness
    {
        float baseColorFactor[4] = { 1,1,1,1 };
        TextureInfo baseColorTexture;          // optional

        float metallicFactor = 1.0f;
        float roughnessFactor = 1.0f;
        TextureInfo metallicRoughnessTexture;  // optional
    };

    enum class AlphaMode
    {
        Opaque,
        Mask,
        Blend
    };

    // Common KHR material extension payloads (field names follow spec)
    struct KHR_materials_clearcoat
    {
        float       clearcoatFactor = 0.0f;
        TextureInfo clearcoatTexture;               // optional
        float       clearcoatRoughnessFactor = 0.0f;
        TextureInfo clearcoatRoughnessTexture;      // optional
        NormalTextureInfo clearcoatNormalTexture;   // optional
    };

    struct KHR_materials_transmission
    {
        float       transmissionFactor = 0.0f;
        TextureInfo transmissionTexture;            // optional
    };

    struct KHR_materials_ior
    {
        float ior = 1.5f; // default per spec
    };

    struct KHR_materials_volume
    {
        float       thicknessFactor = 0.0f;
        TextureInfo thicknessTexture;               // optional
        float       attenuationDistance = std::numeric_limits<float>::infinity();
        float       attenuationColor[3] = { 1.0f,1.0f,1.0f };
    };

    struct KHR_materials_specular
    {
        float       specularFactor = 1.0f;
        TextureInfo specularTexture;                // optional
        float       specularColorFactor[3] = { 1.0f,1.0f,1.0f };
        TextureInfo specularColorTexture;           // optional
    };

    struct KHR_materials_sheen
    {
        float       sheenColorFactor[3] = { 0.0f,0.0f,0.0f };
        TextureInfo sheenColorTexture;              // optional
        float       sheenRoughnessFactor = 0.0f;
        TextureInfo sheenRoughnessTexture;          // optional
    };

    struct KHR_materials_emissive_strength
    {
        float emissiveStrength = 1.0f;
    };

    struct KHR_materials_iridescence
    {
        float       iridescenceFactor = 0.0f;
        TextureInfo iridescenceTexture;             // optional
        float       iridescenceIor = 1.3f;
        float       iridescenceThicknessMinimum = 100.0f;
        float       iridescenceThicknessMaximum = 400.0f;
        TextureInfo iridescenceThicknessTexture;    // optional
    };

    struct KHR_materials_anisotropy
    {
        float       anisotropyStrength = 0.0f;
        float       anisotropyRotation = 0.0f;
        TextureInfo anisotropyTexture;              // optional
    };

    // Deprecated but still used in the wild
    struct KHR_materials_pbrSpecularGlossiness
    {
        float       diffuseFactor[4] = { 1.0f,1.0f,1.0f,1.0f };
        TextureInfo diffuseTexture;                 // optional
        float       specularFactor[3] = { 1.0f,1.0f,1.0f };
        float       glossinessFactor = 1.0f;
        TextureInfo specularGlossinessTexture;      // optional
    };

    struct Material
    {
        // Core
        PBRMetallicRoughness pbrMetallicRoughness;  // optional
        NormalTextureInfo    normalTexture;         // optional
        OcclusionTextureInfo occlusionTexture;      // optional
        TextureInfo          emissiveTexture;       // optional

        float emissiveFactor[3] = { 0,0,0 };

        AlphaMode    alphaMode = AlphaMode::Opaque; // glTF uses string; map to enum
        float        alphaCutoff = 0.5f;              // used when alphaMode == MASK
        bool         doubleSided = false;

        hgl::AnsiString   name;                            // optional

        // Extension usage flags (presence markers)
        bool has_KHR_materials_clearcoat = false;
        bool has_KHR_materials_transmission = false;
        bool has_KHR_materials_ior = false;
        bool has_KHR_materials_volume = false;
        bool has_KHR_materials_specular = false;
        bool has_KHR_materials_sheen = false;
        bool has_KHR_materials_emissive_strength = false;
        bool has_KHR_materials_iridescence = false;
        bool has_KHR_materials_anisotropy = false;
        bool has_KHR_materials_unlit = false;  // presence only
        bool has_KHR_materials_pbrSpecularGlossiness = false;  // deprecated

        // Extension payloads
        KHR_materials_clearcoat            KHR_clearcoat;            // valid iff has_...
        KHR_materials_transmission         KHR_transmission;
        KHR_materials_ior                  KHR_ior;
        KHR_materials_volume               KHR_volume;
        KHR_materials_specular             KHR_specular;
        KHR_materials_sheen                KHR_sheen;
        KHR_materials_emissive_strength    KHR_emissive_strength;
        KHR_materials_iridescence          KHR_iridescence;
        KHR_materials_anisotropy           KHR_anisotropy;
        KHR_materials_pbrSpecularGlossiness KHR_pbrSpecularGlossiness; // deprecated
    };
}// namespace gltf
