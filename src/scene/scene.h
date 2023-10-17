
#ifndef SCENE_H
#define SCENE_H

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <queue>
#include <functional>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <stb_image.h>

#include "../common/Shader.h"
#include "../common/AssimpHelpers.h"
#include "mesh.h"
#include "env.h"


//All textures and samplers must follow the naming convention:
/*
    uniform sampler2D texture_diffuse1;
    uniform sampler2D texture_diffuse2;
    uniform sampler2D texture_diffuse3;
    uniform sampler2D texture_specular1;
    uniform sampler2D texture_specular2;
    .
    .
    .
 */

enum TextureType
{
    OP_TEXTURE_DIFFUSE,
    OP_TEXTURE_SPECULAR,
    OP_TEXTURE_NORMAL
};

struct Texture {
    unsigned int id;
    TextureType type;
};  

struct Material
{
    // albedoColor will only be used for simple materials and simple shaders where we dont
    // sample textures
    glm::vec3 albedoColor;
    glm::vec3 emissiveColor;
    std::vector<std::string> texturePaths;

    Material()
    {

    }
};


struct Object
{
    //currently each object can only have one mesh
    std::shared_ptr<Mesh> mesh;
    Material *material;

    glm::mat4 objToWorld;

    Object(std::shared_ptr<Mesh> srcMesh, Material *srcMaterial, glm::mat4 srcTransform)
    {
        this->mesh = srcMesh;
        this->material = srcMaterial;
        this->objToWorld = srcTransform;
    }
};


class Scene
{
    public:
        //std::string scenePath
        Scene()
        {
            //currently, each scene can have many object files, each object file can actualy have more than one 
            //object, one for each mesh. Currently, each of these objects will have the same overridden properties
            //that was set on the scene file for the whole object file, but otherwise they will be treated as
            //independent objects (no hierarchy)

            //add property overrides in order to set some material special properties like albedo and emissive
            //and also set the root transform for the object
            glm::mat4 rootTransform = glm::mat4(1.0f);
            AssimpLoadFile(BASE_DIR"/data/models/sphere.obj", rootTransform);
            std::cout << "Loaded scene" << "\n";
            std::cout << "Materials: " << materials.size() << std::endl;
            std::cout << "Meshes: " << meshes.size() << std::endl;
            std::cout << "Objects: " << objects.size() << std::endl;
        }

        //original:
        //using ObjectCallback = std::function<void(glm::mat4 objectToWorld, glm::vec3 albedoColor, glm::vec3 emissiveColor, vk::Buffer vertexBuffer, vk::Buffer indexBuffer, uint32_t verticesCount, uint32_t indicesCount)>;
        using ObjectCallback = std::function<void(glm::mat4 objectToWorld, Material* material, std::shared_ptr<Mesh> mesh, unsigned int verticesCount, unsigned int indicesCount)>;
        void IterateObjects(ObjectCallback objectCallback)
        {
           
            for (auto object : objects)
            {
                objectCallback(object.objToWorld, object.material, object.mesh, object.mesh->verticesCount, object.mesh->indicesCount);
            }
        }

        bool HasTexture(std::string &path)
        {
            return loadedTextures.find(path) != loadedTextures.end();
        }

        Texture GetTexture(std::string &path)
        {
            return loadedTextures[path];
        }


        std::vector<Object> objects;
    private:

        ////////////////////////////////////////////////////////////////////////////////////////////////////
        // STOP DUPLICATED MESHES FROM FROM DIFFERENT OBJECTS IN THE SAME SCENE:
        // -currently, if the same object file is referenced in the scene file, the meshes are loaded twice:
        //  use the file path to identify unique meshes
        // ADD A TRANSFORM HIERARCHY
        ////////////////////////////////////////////////////////////////////////////////////////////////////

        //load/instantiation order: textures -> materials -> meshes -> objects
        std::unordered_map<std::string,Texture> loadedTextures;
        std::vector<Material> materials;
        std::vector<std::shared_ptr<Mesh>> meshes;
        //std::vector<Object> objects;

        
        void AssimpLoadFile(std::string objectFile, glm::mat4 &rootTransform)
        {
            Assimp::Importer import;
            const aiScene *scene = import.ReadFile(objectFile, aiProcess_Triangulate | aiProcess_FlipUVs);	
            
            if(!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) 
            {
                std::cout << "ERROR::ASSIMP::" << import.GetErrorString() << std::endl;
                return;
            }

            std::string directory = objectFile.substr(0, objectFile.find_last_of('/'));
            
            

            for(unsigned int i = 0; i < scene->mNumMaterials; i++)
            {
                aiMaterial *mMaterial = scene->mMaterials[i];
                std::vector<std::string> mTextures;

                std::vector<std::string> diffuseMaps = LoadMaterialTextures(mMaterial, 
                                                    aiTextureType_DIFFUSE, OP_TEXTURE_DIFFUSE, directory);
                mTextures.insert(mTextures.end(), diffuseMaps.begin(), diffuseMaps.end());
                std::vector<std::string> specularMaps = LoadMaterialTextures(mMaterial, 
                                                    aiTextureType_SPECULAR, OP_TEXTURE_SPECULAR, directory);
                mTextures.insert(mTextures.end(), specularMaps.begin(), specularMaps.end());
                std::vector<std::string> normalMaps = LoadMaterialTextures(mMaterial,
                                                    aiTextureType_NORMALS, OP_TEXTURE_NORMAL, directory);
                mTextures.insert(mTextures.end(), normalMaps.begin(), normalMaps.end());
                
                Material material = Material();
                material.texturePaths = mTextures;
                materials.push_back(material);
            }  


            aiMatrix4x4 identity = aiMatrix4x4();

            //recursively retrieve every mesh on the scene
            ProcessAssimpNode(scene->mRootNode, scene, identity, rootTransform);


        }  

        void ProcessAssimpNode(aiNode *node, const aiScene *scene, aiMatrix4x4 &parentTransform, glm::mat4 &rootTransform)
        {
            aiMatrix4x4 globalTransform = parentTransform * (node->mTransformation);
            for(unsigned int i = 0; i < node->mNumMeshes; i++)
            {
                aiMesh *mMesh = scene->mMeshes[node->mMeshes[i]]; 

                std::vector<MeshData::Vertex> mVertices;
                std::vector<unsigned int> mIndices;

                // process vertex positions, normals and texture coordinates for each vertex in the mesh
                for(unsigned int i = 0; i < mMesh->mNumVertices; i++)
                {
                    MeshData::Vertex vertex;
                    vertex.Position = AssimpHelpers::GetGLMVec3(mMesh->mVertices[i]);
                    //std::cout << "vx: " << vertex.Position.x << " vy: " << vertex.Position.y << " vz: " << vertex.Position.z << "\n";
                    if (mMesh->HasNormals())
                    {
                        vertex.Normal = AssimpHelpers::GetGLMVec3(mMesh->mNormals[i]);
                    }
                    //there can be up to 8 texcoords
                    if(mMesh->mTextureCoords[0]) // check if the mesh contain texture coordinates
                    {
                        glm::vec2 vec;
                        vec.x = mMesh->mTextureCoords[0][i].x; 
                        vec.y = mMesh->mTextureCoords[0][i].y;
                        vertex.TexCoords = vec;
                    }
                    else
                        vertex.TexCoords = glm::vec2(0.0f, 0.0f); 

                    mVertices.push_back(vertex);
                }
                // process indices
                for(unsigned int i = 0; i < mMesh->mNumFaces; i++)
                {
                    aiFace face = mMesh->mFaces[i];
                    for(unsigned int j = 0; j < face.mNumIndices; j++)
                        mIndices.push_back(face.mIndices[j]);
                }  

                //auto meshData = MeshData(mVertices, mIndices);
                auto meshptr = std::make_shared<Mesh>(mVertices, mIndices);
                meshes.push_back(meshptr);		


                Material material;

                // link to the saved material
                if(mMesh->mMaterialIndex >= 0)
                {
                    material = materials[mMesh->mMaterialIndex];
                }


                glm::mat4 objectTransform = rootTransform * AssimpHelpers::ConvertMatrixToGLMFormat(globalTransform);

                Object object = Object(meshptr, &material, objectTransform);
                objects.push_back(object);
            }

            for(unsigned int i = 0; i < node->mNumChildren; i++)
            {
                ProcessAssimpNode((node->mChildren[i]), scene, globalTransform, rootTransform);
            }
        }  

        std::vector<std::string> LoadMaterialTextures(aiMaterial *mMaterial, aiTextureType type, TextureType OPtype, std::string &directory)
        {
            std::vector<std::string> texturePaths;
            for(unsigned int i = 0; i < mMaterial->GetTextureCount(type); i++)
            {
                aiString aiPath;
                mMaterial->GetTexture(type, i, &aiPath);
                bool skip = false;


                if (loadedTextures.find(aiPath.C_Str()) != loadedTextures.end())
                {
                    texturePaths.push_back(aiPath.C_Str());
                    skip = true; 
                }
                if(!skip)
                {   
                    Texture texture;
                    texture.id = TextureFromFile(aiPath.C_Str(), directory);
                    texture.type = OPtype;
                    loadedTextures[aiPath.C_Str()] = texture;
                    texturePaths.push_back(aiPath.C_Str());
                }
            }

            return texturePaths;
        }
        
        unsigned int TextureFromFile(const char *path, const std::string &directory)
        {
            std::string filename = std::string(path);
            filename = directory + '/' + filename;

            unsigned int textureID;
            glGenTextures(1, &textureID);

            int width, height, nrComponents;
            unsigned char *data = stbi_load(filename.c_str(), &width, &height, &nrComponents, 0);
            if (data)
            {
                GLenum format;
                if (nrComponents == 1)
                    format = GL_RED;
                else if (nrComponents == 3)
                    format = GL_RGB;
                else if (nrComponents == 4)
                    format = GL_RGBA;

                glBindTexture(GL_TEXTURE_2D, textureID);
                glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
                glGenerateMipmap(GL_TEXTURE_2D);

                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

                stbi_image_free(data);
            }
            else
            {
                std::cout << "Texture failed to load at path: " << path << std::endl;
                stbi_image_free(data);
            }

            return textureID;
        }  

};


#endif