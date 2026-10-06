#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>
#include <assimp/mesh.h>

// CPU-only import boundary used by the existing DX11 loader and native runtime.
// Geometry owns its arrays; no aiMesh pointer or graphics object survives here.
namespace ModelGeometryImport {
struct Float2 { float x,y; };
struct Float3 { float x,y,z; };
struct Float4 { float x,y,z,w; };
struct Vertex {
    Float3 Position,Normal,Tangent;
    Float4 Diffuse;
    Float2 TexCoord;
};
static_assert(sizeof(Vertex)==60);
struct Geometry {
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
    bool IsValid() const noexcept { return !vertices.empty() && !indices.empty(); }
};
template<class GeometryData>
bool ExtractMesh(const aiMesh& mesh,bool isBlender,GeometryData& output) {
    if(!mesh.HasPositions() || !mesh.mNumVertices || !mesh.mFaces || !mesh.mNumFaces ||
        mesh.mNumFaces>(std::numeric_limits<std::uint32_t>::max)()/3) return false;
    // Validate before publishing the replacement, including triangulation and
    // bounds. Malformed geometry must not leave a partially replaced snapshot.
    for(unsigned int f=0;f<mesh.mNumFaces;++f) {
        const auto& face=mesh.mFaces[f];
        if(face.mNumIndices!=3 || !face.mIndices) return false;
        for(unsigned int i=0;i<3;++i) if(face.mIndices[i]>=mesh.mNumVertices) return false;
    }
    GeometryData replacement;
    replacement.vertices.resize(mesh.mNumVertices);
    replacement.indices.reserve(std::size_t(mesh.mNumFaces)*3);
    for(unsigned int v=0;v<mesh.mNumVertices;++v) {
        auto& vertex=replacement.vertices[v];
        const auto p=mesh.mVertices[v];
        const auto n=mesh.HasNormals()?mesh.mNormals[v]:aiVector3D{1,1,1};
        const auto t=mesh.HasTangentsAndBitangents()?mesh.mTangents[v]:aiVector3D{0,1,0};
        vertex.Position=isBlender?decltype(vertex.Position){p.x,-p.z,p.y}:decltype(vertex.Position){p.x,p.y,p.z};
        vertex.Normal=isBlender?decltype(vertex.Normal){n.x,-n.z,n.y}:decltype(vertex.Normal){n.x,n.y,n.z};
        vertex.Tangent=isBlender?decltype(vertex.Tangent){t.x,-t.z,t.y}:decltype(vertex.Tangent){t.x,t.y,t.z};
        if(mesh.HasVertexColors(0)) {
            const auto c=mesh.mColors[0][v]; vertex.Diffuse={c.r,c.g,c.b,c.a};
        } else vertex.Diffuse={1,1,1,1};
        if(mesh.HasTextureCoords(0)) { const auto uv=mesh.mTextureCoords[0][v]; vertex.TexCoord={uv.x,uv.y}; }
        else vertex.TexCoord={0,0};
    }
    for(unsigned int f=0;f<mesh.mNumFaces;++f)
        replacement.indices.insert(replacement.indices.end(),mesh.mFaces[f].mIndices,mesh.mFaces[f].mIndices+3);
    output=std::move(replacement);
    return true;
}
}
