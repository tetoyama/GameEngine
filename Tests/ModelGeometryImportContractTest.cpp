#include "Engine/Resources/Loader/ModelGeometryFile.h"
#include <iostream>
#include <stdexcept>
static void Require(bool value,const char* message) { if(!value) throw std::runtime_error(message); }
int main(int argc,char** argv) {
    try {
        aiMesh mesh;
        mesh.mNumVertices=12;
        mesh.mVertices=new aiVector3D[12];
        mesh.mNormals=new aiVector3D[12];
        mesh.mColors[0]=new aiColor4D[12];
        mesh.mTextureCoords[0]=new aiVector3D[12];
        mesh.mNumUVComponents[0]=2;
        for(unsigned int i=0;i<12;++i) {
            mesh.mVertices[i]={float(i),2,3}; mesh.mNormals[i]={0,1,0};
            mesh.mColors[0][i]={float(i)/12,0,1,1}; mesh.mTextureCoords[0][i]={float(i)/12,.75f,0};
        }
        mesh.mNumFaces=1; mesh.mFaces=new aiFace[1];
        mesh.mFaces[0].mNumIndices=3; mesh.mFaces[0].mIndices=new unsigned int[3]{0,9,11};
        ModelGeometryImport::Geometry geometry;
        Require(ModelGeometryImport::ExtractMesh(mesh,false,geometry),"Valid triangle rejected");
        Require(geometry.vertices[9].Diffuse.x==.75f && geometry.vertices[9].Diffuse.z==1,
            "Vertex index was used as color-channel index");
        Require(geometry.vertices[9].TexCoord.x==.75f && geometry.vertices[9].TexCoord.y==.75f,
            "UV extraction changed");
        Require(ModelGeometryImport::ExtractMesh(mesh,true,geometry) && geometry.vertices[9].Position.y==-3 &&
            geometry.vertices[9].Position.z==2 && geometry.vertices[9].Normal.z==1,"Blender basis changed");
        mesh.mFaces[0].mIndices[2]=12;
        Require(!ModelGeometryImport::ExtractMesh(mesh,false,geometry) && geometry.vertices[9].Position.y==-3,
            "Out-of-range index published invalid/partial geometry");
        mesh.mFaces[0].mIndices[2]=11; mesh.mFaces[0].mNumIndices=2;
        Require(!ModelGeometryImport::ExtractMesh(mesh,false,geometry),"Non-triangle face accepted");
        mesh.mFaces[0].mNumIndices=3;
        delete[] mesh.mNormals; mesh.mNormals=nullptr;
        delete[] mesh.mColors[0]; mesh.mColors[0]=nullptr;
        Require(ModelGeometryImport::ExtractMesh(mesh,false,geometry) && geometry.vertices[9].Normal.x==1 &&
            geometry.vertices[9].Diffuse.x==1,"Missing attributes lack existing defaults");
        Require(argc==2,"Supply the existing cube.obj fixture");
        const auto imported=ModelGeometryImport::LoadFile(argv[1]);
        Require(!imported.empty(),"Existing model did not import");
        for(const auto& source:imported) Require(source.IsValid() && source.indices.size()%3==0,
            "Imported geometry invalid after Assimp scene release");
        bool rejected=false;
        try { (void)ModelGeometryImport::LoadFile(std::string(argv[1])+".missing"); }
        catch(const std::runtime_error&) { rejected=true; }
        Require(rejected,"Missing model silently accepted");
        std::cout<<"Shared model extraction: color channel, UV, basis, invalid input, ownership and file import passed\n";
        return 0;
    } catch(const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; }
}
