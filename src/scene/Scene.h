
#ifndef SCENE_H
#define SCENE_H

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <stb_image.h>
#include <json/json.h>

#include "../common/Shader.h"
#include "../common/AssimpHelpers.h"
#include "../common/JsonHelpers.h"
#include "Mesh.h"
#include "Object.h"
#include "lights.h"

#include "env.h"




//currently, each scene can have many object files, each object file can actualy have more than one 
//object, one for each mesh. Each of these objects will have the same overridden properties
//that was set on the scene file for the whole object file, but otherwise they will be treated as
//independent objects (no hierarchy)

//add property overrides in order to set some material special properties like albedo and emissive
//and also set the root transform for the object



//ObjectData contains references used to build objects
struct ObjectBlueprint
{
    glm::mat4 localTransform;
    std::shared_ptr<Mesh> mesh;
    unsigned int materialId;
};




class Scene
{
    public:
        Scene(const std::string &relativePath)
        {
            std::cout << "Loading Scene: " << "\n";

            Json::Value configRoot;
            Json::Reader reader;


            std::ifstream fileStream(BASE_DIR + relativePath);

            if (!fileStream.is_open())
                std::cout << "Can't open scene file";

            bool result = reader.parse(fileStream, configRoot);
            if (result)
            {
                std::cout << "File " << BASE_DIR +  relativePath << ", parsing successful\n";
            }
            else
            {
                std::cout << "Error: File " << BASE_DIR +  relativePath << ", parsing failed with errors: " << reader.getFormattedErrorMessages() << "\n";
            }

            //construc blueprints from object file or use existing ones
            Json::Value meshArray = configRoot["scene"]["meshes"];

            for (Json::ArrayIndex meshIndex = 0; meshIndex < meshArray.size(); meshIndex++)
            {
                Json::Value currMesh = meshArray[meshIndex];
                std::string meshFilename = currMesh.get("filename", "<unspecified>").asString();


                std::string objFile = BASE_DIR+meshFilename;
                std::string meshName = currMesh.get("name", "<unspecified_mesh>").asString();

                if(objectBlueprints.find(meshName) == objectBlueprints.end())
                {
                    AssimpLoadObjects(objFile, meshName);
                    materialIdOffset = materials.size();
                }
            }

            //use the blueprints to build an objects
            Json::Value objectArray = configRoot["scene"]["objects"];

            for (Json::ArrayIndex objectIndex = 0; objectIndex < objectArray.size(); objectIndex++)
            {
                Json::Value currObject = objectArray[objectIndex];

                std::string meshName = currObject.get("mesh", "<unspecified_object>").asString();
                glm::vec3 worldPosition = JsonHelpers::GetJsonVec3f(currObject["pos"]); 
                glm::vec3 scale = JsonHelpers::GetJsonVec3f(currObject["scale"]); 
                
                glm::mat4 rootTransform = glm::mat4(1);
                rootTransform = glm::scale(rootTransform, scale);
                rootTransform = glm::translate(rootTransform, worldPosition);

                for (auto &blueprint : objectBlueprints[meshName])
                {
                    Object newObject = Object();
                    newObject.mesh = blueprint.mesh;
                    newObject.material = &materials[blueprint.materialId];
                    newObject.objToWorld = rootTransform * blueprint.localTransform;

                    objects.push_back(newObject);
                }
            }







            std::cout << "Loading Success: " << "\n";
            std::cout << "Materials: " << materials.size() << std::endl;

            for (unsigned int i = 0; i < materials.size(); i++)
            {
                std::cout << "->material: " + std::to_string(i) << ":\n";
                for (unsigned int j = 0; j < materials[i].texturePaths.size(); j++)
                {
                    std::cout << "--texture[" << std::to_string(loadedTextures[materials[i].texturePaths[j]].id) << "]\n";
                }
            }
            
            std::cout << "Mesh Groups: " << objectBlueprints.size() << std::endl;
            std::cout << "Objects: " << objects.size() << std::endl;
            std::cout << "object materials: [";

            for (unsigned int i = 0; i < objects.size(); i++)
            {
                std::cout << std::to_string(objects[i].material->id) << " ";
            }
            std::cout << "]\n";
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

        void AddObject(std::string filename)
        {

        }


        
    private:

        ////////////////////////////////////////////////////////////////////////////////////////////////////
        // ADD A TRANSFORM HIERARCHY
        ////////////////////////////////////////////////////////////////////////////////////////////////////

        //load/instantiation order: textures -> materials -> meshes -> objects

        //maps texture (file) paths to each texture object
        std::unordered_map<std::string, Texture> loadedTextures;
        std::vector<Material> materials;
        unsigned int materialIdOffset = 0;
        
        std::unordered_map<std::string, std::vector<ObjectBlueprint>> objectBlueprints;
        std::vector<Object> objects;

        //used for batching draw calls by merging multiple objects together
        unsigned int indexBufferOffset = 0;


        

        
        void AssimpLoadObjects(const std::string &objFile, const std::string & meshName)
        {
            Assimp::Importer import;
            const aiScene *scene = import.ReadFile(objFile, aiProcess_Triangulate | aiProcess_FlipUVs);	
                
            if(!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) 
            {
                std::cout << "ERROR::ASSIMP::" << import.GetErrorString() << std::endl;
                return;
            }
            

            objectBlueprints[meshName] = std::vector<ObjectBlueprint>();

            std::string baseDirectory = objFile.substr(0, objFile.find_last_of('/'));
            AssimpLoadMaterials(scene, baseDirectory);

            // Loading meshes
            for(unsigned int m = 0; m < scene->mNumMeshes; m++)
            {
                aiMesh *mMesh = scene->mMeshes[m];
                
                std::vector<MeshData::Vertex> mVertices;
                std::vector<unsigned int> mIndices;

                // process vertex positions, normals and texture coordinates for each vertex in the mesh
                for(unsigned int i = 0; i < mMesh->mNumVertices; i++)
                {
                    MeshData::Vertex vertex;
                    vertex.Position = AssimpHelpers::GetGLMVec3(mMesh->mVertices[i]);

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


                auto meshptr = std::make_shared<Mesh>(mVertices, mIndices);
                
                auto blueprint = ObjectBlueprint(); 
                blueprint.mesh = meshptr;
                blueprint.materialId = mMesh->mMaterialIndex + materialIdOffset;

                objectBlueprints[meshName].push_back(blueprint);

                
            }


            auto identity = glm::mat4(1);
            ProcessAssimpNode(scene->mRootNode, scene, identity, meshName);
        }  

        void ProcessAssimpNode(aiNode *node, const aiScene *scene, glm::mat4 &parentTransform, const std::string &meshName)
        {
            glm::mat4 objectTransform = parentTransform * AssimpHelpers::ConvertMatrixToGLMFormat(node->mTransformation);

            for(unsigned int i = 0; i < node->mNumMeshes; i++)
            {
                unsigned int mMeshId = node->mMeshes[i]; 
                objectBlueprints[meshName][mMeshId].localTransform = objectTransform;
            }

            for(unsigned int i = 0; i < node->mNumChildren; i++)
            {
                ProcessAssimpNode((node->mChildren[i]), scene, objectTransform, meshName);
            }
        }  

        void AssimpLoadMaterials(const aiScene *scene, const std::string &directory)
        {
            for (unsigned int i = 0; i < scene->mNumMaterials; i++)
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
                material.id = i + materialIdOffset;
                materials.push_back(material);
            }
        }

        std::vector<std::string> LoadMaterialTextures(aiMaterial *mMaterial, aiTextureType type, TextureType OPtype, const std::string &directory)
        {
            std::vector<std::string> texturePaths;
            for(unsigned int i = 0; i < mMaterial->GetTextureCount(type); i++)
            {
                aiString aiPath;
                mMaterial->GetTexture(type, i, &aiPath);


                if (loadedTextures.find(aiPath.C_Str()) != loadedTextures.end())
                {
                    texturePaths.push_back(aiPath.C_Str());
                }
                else
                {   
                    Texture texture;
                    texture.id = TextureFromFile(aiPath.C_Str(), directory);
                    texture.type = OPtype;
                    loadedTextures[aiPath.C_Str()] = texture;
                    std::cout << "Loaded Texture [" << std::to_string(texture.id) + "]: " << aiPath.C_Str() << "\n";
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