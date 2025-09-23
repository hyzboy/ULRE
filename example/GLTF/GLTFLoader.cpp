#include "GLTFLoader.h"

#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <optional>
#include <filesystem>

#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>

namespace {

struct Indent { int n=0; explicit Indent(int i):n(i){} };
std::ostream& operator<<(std::ostream& os, const Indent& id){ for(int i=0;i<id.n;++i) os<<' '; return os; }

const char* ToString(fastgltf::PrimitiveType t){
    using T=fastgltf::PrimitiveType;
    switch(t){
        case T::Points: return "Points";
        case T::Lines: return "Lines";
        case T::LineLoop: return "LineLoop";
        case T::LineStrip: return "LineStrip";
        case T::Triangles: return "Triangles";
        case T::TriangleStrip: return "TriangleStrip";
        case T::TriangleFan: return "TriangleFan";
        default: return "Unknown";
    }
}

const char* ToString(fastgltf::ComponentType t){
    using CT=fastgltf::ComponentType;
    switch(t){
        case CT::Byte: return "Byte";
        case CT::UnsignedByte: return "UnsignedByte";
        case CT::Short: return "Short";
        case CT::UnsignedShort: return "UnsignedShort";
        case CT::Int: return "Int";
        case CT::UnsignedInt: return "UnsignedInt";
        case CT::Float: return "Float";
        case CT::Double: return "Double";
        default: return "Invalid";
    }
}

void DumpHeader(const fastgltf::Asset& asset){
    std::cout << "=== glTF Summary ===\n";
    std::cout << "Scenes:     " << asset.scenes.size()     << "\n";
    std::cout << "Nodes:      " << asset.nodes.size()      << "\n";
    std::cout << "Meshes:     " << asset.meshes.size()     << "\n";
    std::cout << "Materials:  " << asset.materials.size()  << "\n";
    std::cout << "Textures:   " << asset.textures.size()   << "\n";
    std::cout << "Images:     " << asset.images.size()     << "\n";
    std::cout << "Samplers:   " << asset.samplers.size()   << "\n";
    std::cout << "Accessors:  " << asset.accessors.size()  << "\n";
    std::cout << "BufferViews:" << asset.bufferViews.size() << "\n";
    std::cout << "Buffers:    " << asset.buffers.size()    << "\n";
    std::cout << "Animations: " << asset.animations.size() << "\n";
    std::cout << "Skins:      " << asset.skins.size()      << "\n";
    std::cout << "Cameras:    " << asset.cameras.size()    << "\n";
    std::cout << "Lights:     " << asset.lights.size()     << "\n\n";
}

void DumpScenes(const fastgltf::Asset& asset,int indent=0){
    std::cout<<Indent(indent)<<"Scenes:\n";
    for(size_t i=0;i<asset.scenes.size();++i){
        const auto& sc=asset.scenes[i];
        std::cout<<Indent(indent+2)<<"#"<<i;
        if(!sc.name.empty()) std::cout<<" name=\""<<sc.name<<"\"";
        std::cout<<" nodes="<<sc.nodeIndices.size()<<"\n";
    }
    std::cout<<"\n";
}

void DumpNodes(const fastgltf::Asset& asset,int indent=0){
    std::cout<<Indent(indent)<<"Nodes:\n";
    for(size_t i=0;i<asset.nodes.size();++i){
        const auto& n=asset.nodes[i];
        std::cout<<Indent(indent+2)<<"#"<<i;
        if(!n.name.empty()) std::cout<<" name=\""<<n.name<<"\"";
        if(n.meshIndex) std::cout<<" mesh="<<*n.meshIndex;
        if(n.skinIndex) std::cout<<" skin="<<*n.skinIndex;
        if(n.cameraIndex) std::cout<<" camera="<<*n.cameraIndex;
        if(n.lightIndex) std::cout<<" light="<<*n.lightIndex;
        std::cout<<" children="<<n.children.size()<<" weights="<<n.weights.size()<<"\n";
    }
    std::cout<<"\n";
}

void DumpMeshes(const fastgltf::Asset& asset,int indent=0){
    std::cout<<Indent(indent)<<"Meshes:\n";
    for(size_t i=0;i<asset.meshes.size();++i){
        const auto& m=asset.meshes[i];
        std::cout<<Indent(indent+2)<<"#"<<i;
        if(!m.name.empty()) std::cout<<" name=\""<<m.name<<"\"";
        std::cout<<" primitives="<<m.primitives.size()<<" weights="<<m.weights.size()<<"\n";
        for(size_t p=0;p<m.primitives.size();++p){
            const auto& prim=m.primitives[p];
            std::cout<<Indent(indent+4)<<"- prim #"<<p<<" mode="<<ToString(prim.type);
            if(prim.materialIndex) std::cout<<" material="<<*prim.materialIndex;
            if(prim.indicesAccessor) std::cout<<" indicesAccessor="<<*prim.indicesAccessor;
            std::cout<<" attrs="<<prim.attributes.size()<<" targets="<<prim.targets.size()<<"\n";
            for(const auto& a: prim.attributes){
                std::cout<<Indent(indent+6)<<"attr '"<<a.name<<"' -> accessor #"<<a.accessorIndex<<"\n";
            }
        }
    }
    std::cout<<"\n";
}

void DumpMaterials(const fastgltf::Asset& asset,int indent=0){
    std::cout<<Indent(indent)<<"Materials:\n";
    for(size_t i=0;i<asset.materials.size();++i){
        const auto& m=asset.materials[i];
        std::cout<<Indent(indent+2)<<"#"<<i;
        if(!m.name.empty()) std::cout<<" name=\""<<m.name<<"\"";
        std::cout<<" doubleSided="<<(m.doubleSided?"true":"false")
                 <<" unlit="<<(m.unlit?"true":"false")
                 <<" alphaMode="<<(int)m.alphaMode
                 <<" emissiveStrength="<<m.emissiveStrength
                 <<" ior="<<m.ior
                 <<"\n";
    }
    std::cout<<"\n";
}

void DumpTexturesImages(const fastgltf::Asset& asset,int indent=0){
    std::cout<<Indent(indent)<<"Textures: "<<asset.textures.size()<<"\n";
    for(size_t i=0;i<asset.textures.size();++i){
        const auto& t=asset.textures[i];
        std::cout<<Indent(indent+2)<<"#"<<i<<" name=\""<<t.name<<"\"";
        if(t.imageIndex) std::cout<<" image="<<*t.imageIndex;
        if(t.samplerIndex) std::cout<<" sampler="<<*t.samplerIndex;
        std::cout<<"\n";
    }
    std::cout<<Indent(indent)<<"Images:   "<<asset.images.size()<<"\n";
    for(size_t i=0;i<asset.images.size();++i){
        const auto& img=asset.images[i];
        std::cout<<Indent(indent+2)<<"#"<<i<<" name=\""<<img.name<<"\"\n";
    }
    std::cout<<"\n";
}

void DumpBuffers(const fastgltf::Asset& asset,int indent=0){
    std::cout<<Indent(indent)<<"Buffers: "<<asset.buffers.size()<<"\n";
    for(size_t i=0;i<asset.buffers.size();++i){
        const auto& b=asset.buffers[i];
        std::cout<<Indent(indent+2)<<"#"<<i<<" byteLength="<<b.byteLength<<" name=\""<<b.name<<"\"\n";
    }
    std::cout<<Indent(indent)<<"BufferViews: "<<asset.bufferViews.size()<<"\n";
    for(size_t i=0;i<asset.bufferViews.size();++i){
        const auto& bv=asset.bufferViews[i];
        std::cout<<Indent(indent+2)<<"#"<<i<<" buffer="<<bv.bufferIndex
                 <<" offset="<<bv.byteOffset
                 <<" length="<<bv.byteLength
                 <<" stride="<<(bv.byteStride?std::to_string(*bv.byteStride):std::string("0"))
                 <<" name=\""<<bv.name<<"\"\n";
    }
    std::cout<<"\n";
}

void DumpAccessors(const fastgltf::Asset& asset,int indent=0){
    std::cout<<Indent(indent)<<"Accessors: "<<asset.accessors.size()<<"\n";
    for(size_t i=0;i<asset.accessors.size();++i){
        const auto& a=asset.accessors[i];
        std::cout<<Indent(indent+2)<<"#"<<i
                 <<" bufferView="<<(a.bufferViewIndex?std::to_string(*a.bufferViewIndex):std::string("-"))
                 <<" offset="<<a.byteOffset
                 <<" count="<<a.count
                 <<" compType="<<ToString(a.componentType)
                 <<" elemSize="<<fastgltf::getElementByteSize(a.type, a.componentType)
                 <<" normalized="<<(a.normalized?"true":"false")
                 <<" name=\""<<a.name<<"\"\n";
    }
    std::cout<<"\n";
}

void DumpSamplers(const fastgltf::Asset& asset,int indent=0){
    std::cout<<Indent(indent)<<"Samplers: "<<asset.samplers.size()<<"\n";
    for(size_t i=0;i<asset.samplers.size();++i){
        const auto& s=asset.samplers[i];
        std::cout<<Indent(indent+2)<<"#"<<i<<" name=\""<<s.name<<"\"\n";
    }
    std::cout<<"\n";
}

void DumpAnimations(const fastgltf::Asset& asset,int indent=0){
    std::cout<<Indent(indent)<<"Animations: "<<asset.animations.size()<<"\n";
    for(size_t i=0;i<asset.animations.size();++i){
        const auto& a=asset.animations[i];
        std::cout<<Indent(indent+2)<<"#"<<i<<" channels="<<a.channels.size()<<" samplers="<<a.samplers.size()<<" name=\""<<a.name<<"\"\n";
    }
    std::cout<<"\n";
}

void DumpSkinsCamerasLights(const fastgltf::Asset& asset,int indent=0){
    std::cout<<Indent(indent)<<"Skins: "<<asset.skins.size()<<"\n";
    std::cout<<Indent(indent)<<"Cameras: "<<asset.cameras.size()<<"\n";
    std::cout<<Indent(indent)<<"Lights: "<<asset.lights.size()<<"\n\n";
}

} // namespace

namespace gltf {

bool LoadAndDumpGLTF(const std::filesystem::path& filePath)
{
    if (!std::filesystem::exists(filePath)) {
        std::cerr << "[ERROR] File not found: " << filePath << "\n";
        return false;
    }

    fastgltf::Parser parser{};

    auto data_res = fastgltf::GltfDataBuffer::FromPath(filePath);
    if (data_res.error() != fastgltf::Error::None) {
        std::cerr << "[ERROR] Failed to read file: " << fastgltf::getErrorMessage(data_res.error()) << "\n";
        return false;
    }
    auto data = std::move(data_res.get());

    constexpr fastgltf::Options options = fastgltf::Options::LoadExternalBuffers
                                        | fastgltf::Options::LoadGLBBuffers
                                        | fastgltf::Options::DecomposeNodeMatrices
                                        | fastgltf::Options::GenerateMeshIndices;

    auto asset_res = parser.loadGltf(data, filePath.parent_path(), options);
    if (asset_res.error() != fastgltf::Error::None) {
        std::cerr << "[ERROR] Failed to parse glTF: " << fastgltf::getErrorMessage(asset_res.error()) << "\n";
        return false;
    }

    fastgltf::Asset asset = std::move(asset_res.get());

    DumpHeader(asset);
    DumpScenes(asset);
    DumpNodes(asset);
    DumpMeshes(asset);
    DumpMaterials(asset);
    DumpTexturesImages(asset);
    DumpSamplers(asset);
    DumpBuffers(asset);
    DumpAccessors(asset);
    DumpAnimations(asset);
    DumpSkinsCamerasLights(asset);

    return true;
}

} // namespace gltf
