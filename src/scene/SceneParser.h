#ifndef SCENE_PARSER_H
#define SCENE_PARSER_H

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



#include "../common/AssimpHelpers.h"
#include "../common/JsonHelpers.h"


#include "Scene.h"
#include "Camera.h"

//ObjectBlueprint contains data used to build objects
struct ObjectBlueprint
{
    glm::mat4 localTransform;
    std::shared_ptr<Mesh> mesh;
    unsigned int materialId;
};


enum SceneLoadingFormat
{
    OP_OBJ,
    OP_OTHER,
};

namespace JsonHelpers
{

    class SceneParser
    {
        public:
            SceneParser(){}
            void Parse(Scene &scene, Camera *camera, const std::string &relativePath, SceneLoadingFormat loadingFormat)
            {
                sceneFilePath = relativePath;
                std::cout << "Loading Scene: \n";
                this->sceneLoadingFormat = loadingFormat;

                Json::Reader reader;


                std::ifstream fileStream(BASE_DIR + relativePath);

                if (!fileStream.is_open())
                    std::cout << "Can't open scene file";

                bool result = reader.parse(fileStream, sceneFileRoot);

                if (result)
                {
                    std::cout << "File " << BASE_DIR +  relativePath << ", parsing successful\n";
                    
                    //Backup the scene file
                    Json::StreamWriterBuilder builder;
                    builder["commentStyle"] = "None";
                    builder["indentation"] = "   ";
                    
                    std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
                    std::ofstream outputFileStream(BASE_DIR + sceneFilePath + ".OP"); 
                    writer -> write(sceneFileRoot, &outputFileStream);
                    outputFileStream.close();
                }
                else
                {
                    std::cout << "Error: File " << BASE_DIR +  relativePath << ", parsing failed with errors: " << reader.getFormattedErrorMessages() << "\n";
                }


                // Loading Camera data:
                if (!sceneFileRoot["renderer"]["Camera"].empty())
                {
                    auto cameraData = CameraData();
                    cameraData.position = JsonHelpers::GetJsonVec3f(sceneFileRoot["renderer"]["Camera"]["Position"]);
                    cameraData.up = JsonHelpers::GetJsonVec3f(sceneFileRoot["renderer"]["Camera"]["Up"]);
                    cameraData.front = JsonHelpers::GetJsonVec3f(sceneFileRoot["renderer"]["Camera"]["Front"]);
                    cameraData.yaw = sceneFileRoot["renderer"]["Camera"]["Yaw"].asFloat();
                    cameraData.pitch = sceneFileRoot["renderer"]["Camera"]["Pitch"].asFloat();
                    cameraData.near = sceneFileRoot["renderer"]["Camera"]["Near"].asFloat();
                    cameraData.far = sceneFileRoot["renderer"]["Camera"]["Far"].asFloat();
                    cameraData.aspect = sceneFileRoot["renderer"]["Camera"]["Aspect"].asFloat();
                    cameraData.movementSpeed = sceneFileRoot["renderer"]["Camera"]["MovementSpeed"].asFloat();
                    cameraData.mouseSensitivity = sceneFileRoot["renderer"]["Camera"]["MouseSensitivity"].asFloat();
                    cameraData.zoom = sceneFileRoot["renderer"]["Camera"]["Zoom"].asFloat();
                    cameraData.rotationLocked = sceneFileRoot["renderer"]["Camera"]["RotationLocked"].asBool();

                    *camera = Camera(cameraData);
                }



                scene.AddLight(glm::vec4(JsonHelpers::GetJsonVec3f(sceneFileRoot["renderer"]["ambientLight"]), 1.0));

                //construct blueprints from object file or use existing ones
                Json::Value meshArray = sceneFileRoot["scene"]["meshes"];

                for (Json::ArrayIndex meshIndex = 0; meshIndex < meshArray.size(); meshIndex++)
                {
                    Json::Value currMesh = meshArray[meshIndex];
                    std::string meshFilename = currMesh.get("filename", "<unspecified>").asString();


                    std::string objFile = BASE_DIR+meshFilename;
                    std::string meshName = currMesh.get("name", "<unspecified_mesh>").asString();

                    if(objectBlueprints.find(meshName) == objectBlueprints.end())
                    {
                        AssimpLoadObjects(scene, objFile, meshName);
                        materialIdOffset = materialTemplates.size();
                    }
                }
                

                //use the blueprints to build the objects
                Json::Value objectArray = sceneFileRoot["scene"]["objects"];
                

                for (Json::ArrayIndex objectIndex = 0; objectIndex < objectArray.size(); objectIndex++)
                {
                    Json::Value currObject = objectArray[objectIndex];

                    std::string meshName = currObject.get("mesh", "<unspecified_object>").asString();
                    glm::vec3 worldPosition = JsonHelpers::GetJsonVec3f(currObject["pos"]); 
                    glm::vec3 scale = JsonHelpers::GetJsonVec3f(currObject["scale"]); 


                    // Material properties. 
                    // Refactor: the properties being set should depend on material type:

                    glm::vec3 albedo = JsonHelpers::GetJsonVec3f(currObject["Material"]["albedoColor"]);
                    glm::vec4 specular = glm::vec4(0.0);

                    unsigned int overrideFlags = OP_MATERIAL_DEFAULT;

                    if (currObject["Material"]["type"].asString() == "default")
                    {
                        specular = glm::vec4(
                            JsonHelpers::GetJsonVec3f(currObject["Material"]["specularStrength"]), 
                            currObject["Material"]["specularPower"].asFloat()
                        );
                    }
                    else if(currObject["Material"]["type"].asString() == "unlit")
                    {
                        overrideFlags = OP_MATERIAL_UNLIT;
                    }
                    


                    bool hasLight = !(currObject["Light"].empty());

                    glm::mat4 rootTransform = glm::mat4(1);
                    rootTransform = glm::translate(rootTransform, worldPosition);
                    rootTransform = glm::scale(rootTransform, scale);
                    
                    
                    for (auto &blueprint : objectBlueprints[meshName])
                    {
                        auto materialProperties = MaterialInstance::MaterialProperties();
                        materialProperties.albedoColor = glm::vec4(albedo,1.0);
                        materialProperties.specular = specular;
                        
                        // currently the objects are being copied into a vector, but object bascially only stores
                        // references, so it doesnt impact as much
                        auto newObject = scene.AddObject(
                            blueprint.mesh,
                            rootTransform * blueprint.localTransform,
                            materialProperties,
                            materialTemplates[blueprint.materialId],
                            overrideFlags
                        );

                        if(hasLight)
                        {
                            newObject->materialInstance->AddFlag(OP_MATERIAL_UNLIT);
                            std::string lightType = currObject["Light"]["type"].asString();

                            if (lightType == "directional")
                            {
                                glm::vec3 dir = JsonHelpers::GetJsonVec3f(currObject["Light"]["direction"]);
                                glm::vec3 col = JsonHelpers::GetJsonVec3f(currObject["Light"]["lightColor"]);
                                scene.AddLight(dir,col,newObject);
                            }
                            if (lightType == "point")
                            {
                                glm::vec3 col = JsonHelpers::GetJsonVec3f(currObject["Light"]["lightColor"]);
                                float c = currObject["Light"]["constant"].asFloat();
                                float l = currObject["Light"]["linear"].asFloat();
                                float q = currObject["Light"]["quadratic"].asFloat();
                                scene.AddLight(col,c,l,q,newObject);
                            }

                            //only the first submesh within an object is used as the light source
                            hasLight = false;
                        }
                    }




                }


                /*
                std::cout << "Loading Success: \n";
                std::cout << "Materials: " << materialTemplates.size() << "\n";

                for (unsigned int i = 0; i < materialTemplates.size(); i++)
                {
                    std::cout << "->material: " + std::to_string(i) << ":\n";
                    for (unsigned int j = 0; j < materialTemplates[i].texturePaths.size(); j++)
                    {
                        std::cout << "\t-texture[" << std::to_string(loadedTextures[materialTemplates[i].texturePaths[j]].id) << "]\n";
                    }
                }
                
                std::cout << "Unique Meshes: " << objectBlueprints.size() << "\n";
                std::cout << "Objects: " << objects.size() << "\n";
                std::cout << "directional lights: " << directionalLights.size() << "\n";
                std::cout << "point lights: " << pointLights.size() << "\n";*/
            }

            void SerializeToJson(CameraData &camdata)
            {
                sceneFileRoot["renderer"]["Camera"] = camdata.SerializeToJson();
                //std::cout << sceneFileRoot << std::endl;

                Json::StreamWriterBuilder builder;
                builder["commentStyle"] = "None";
                builder["indentation"] = "   ";
                

                std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
                std::ofstream outputFileStream(BASE_DIR + sceneFilePath); 
                writer -> write(sceneFileRoot, &outputFileStream);
                outputFileStream.close();

            }

        private:
            std::string sceneFilePath;
            Json::Value sceneFileRoot;


            SceneLoadingFormat sceneLoadingFormat = OP_OBJ;

            // maps the mesh name to an object blueprint
            std::unordered_map<std::string, std::vector<ObjectBlueprint>> objectBlueprints;

            std::vector<MaterialTemplate> materialTemplates;
            unsigned int materialIdOffset = 0;

            void AssimpLoadObjects(Scene &scene, const std::string &objFile, const std::string & meshName)
            {
                Assimp::Importer import;
                const aiScene *assimpScene = import.ReadFile(objFile, 
                    aiProcess_Triangulate | 
                    aiProcess_FlipUVs | 
                    aiProcess_TransformUVCoords |
                    aiProcess_CalcTangentSpace | 
                    aiProcess_JoinIdenticalVertices |
                    aiProcess_SplitLargeMeshes |
                    aiProcess_OptimizeMeshes |
                    aiProcess_GenNormals |
                    aiProcess_SortByPType 
                
                );	
                    
                if(!assimpScene || assimpScene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !assimpScene->mRootNode) 
                {
                    std::cout << "ERROR::ASSIMP::" << import.GetErrorString() << "\n";
                    return;
                }
                

                objectBlueprints[meshName] = std::vector<ObjectBlueprint>();

                std::string baseDirectory = objFile.substr(0, objFile.find_last_of('/'));
                AssimpLoadMaterials(scene, assimpScene, baseDirectory);

                // Loading meshes
                for(unsigned int m = 0; m < assimpScene->mNumMeshes; m++)
                {
                    aiMesh *mMesh = assimpScene->mMeshes[m];
                    
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
                        if (mMesh->HasTangentsAndBitangents())
                        {
                            vertex.Tangent = AssimpHelpers::GetGLMVec3(mMesh->mTangents[i]);
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
                ProcessAssimpNode(assimpScene->mRootNode, identity, meshName);
            }  


            void ProcessAssimpNode(aiNode *node, glm::mat4 &parentTransform, const std::string &meshName)
            {
                glm::mat4 objectTransform = parentTransform * AssimpHelpers::ConvertMatrixToGLMFormat(node->mTransformation);

                for(unsigned int i = 0; i < node->mNumMeshes; i++)
                {
                    unsigned int mMeshId = node->mMeshes[i]; 
                    objectBlueprints[meshName][mMeshId].localTransform = objectTransform;
                }

                for(unsigned int i = 0; i < node->mNumChildren; i++)
                {
                    ProcessAssimpNode((node->mChildren[i]), objectTransform, meshName);
                }
            }  


            void AssimpLoadMaterials(Scene &scene, const aiScene *assimpScene, const std::string &directory)
            {
                for (unsigned int i = 0; i < assimpScene->mNumMaterials; i++)
                {
                    aiMaterial *mMaterial = assimpScene->mMaterials[i];

                    std::vector<std::string> mTextures;

                    std::vector<std::string> diffuseMaps = LoadMaterialTextures(scene, mMaterial, 
                                                        aiTextureType_DIFFUSE, OP_TEXTURE_DIFFUSE, directory);
                    mTextures.insert(mTextures.end(), diffuseMaps.begin(), diffuseMaps.end());
                    std::vector<std::string> specularMaps = LoadMaterialTextures(scene, mMaterial, 
                                                        aiTextureType_SPECULAR, OP_TEXTURE_SPECULAR, directory);
                    mTextures.insert(mTextures.end(), specularMaps.begin(), specularMaps.end());

                    std::vector<std::string> normalMaps;
                    
                    normalMaps = LoadMaterialTextures(scene, mMaterial, aiTextureType_NORMALS, OP_TEXTURE_NORMAL, directory);
                    /*
                    if (sceneLoadingFormat == OP_OBJ) 
                    {
                        normalMaps = LoadMaterialTextures(mMaterial, aiTextureType_HEIGHT, OP_TEXTURE_NORMAL, directory);
                    }
                    else
                    {
                        normalMaps = LoadMaterialTextures(mMaterial, aiTextureType_NORMALS, OP_TEXTURE_NORMAL, directory);
                    }*/
                    
                    
                    mTextures.insert(mTextures.end(), normalMaps.begin(), normalMaps.end());
                    
                    unsigned int flags = OP_MATERIAL_DEFAULT;

                    if (diffuseMaps.size() > 0)
                    {
                        flags = flags | OP_MATERIAL_TEXTURED_DIFFUSE;
                    }
                    if (specularMaps.size() > 0)
                    {
                        flags = flags | OP_MATERIAL_TEXTURED_SPECULAR;
                    }
                    if (normalMaps.size() > 0)
                    {
                        flags = flags | OP_MATERIAL_TEXTURED_NORMAL;
                    }

                    MaterialTemplate material = MaterialTemplate(flags);
                    material.texturePaths = mTextures;
                    material.id = i + materialIdOffset;
                    materialTemplates.push_back(material);
                }
            }


            std::vector<std::string> LoadMaterialTextures(Scene &scene, aiMaterial *mMaterial, aiTextureType type, TextureType OPtype, const std::string &directory)
            {
                std::vector<std::string> texturePaths;
                for(unsigned int i = 0; i < mMaterial->GetTextureCount(type); i++)
                {
                    aiString aiPath;
                    mMaterial->GetTexture(type, i, &aiPath);


                    if (scene.HasTexture(aiPath.C_Str()))
                    {
                        texturePaths.push_back(aiPath.C_Str());
                    }
                    else
                    {   
                        Texture texture;
                        texture.id = TextureFromFile(aiPath.C_Str(), directory);
                        texture.type = OPtype;
                        scene.AddTexture(aiPath.C_Str(), texture);
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
                    std::cout << "Texture failed to load at path: " << filename << "\n";
                    stbi_image_free(data);
                }

                return textureID;
            }  


    };



}

#endif