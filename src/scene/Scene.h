
#ifndef SCENE_H
#define SCENE_H

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>


#include <glm/gtx/string_cast.hpp>

#include "../common/Shader.h"
#include "Mesh.h"
#include "Object.h"
#include "lights.h"

#include "env.h"
#include "../gl/Texture.h"



// Observations:
// -avoid creating openGl objects in class constructors, if that object is copied (by vector.push_back for example)
// they will generate entirely new opengl objects and possibly mess with the indices
// If that is necessary, try to allocate object in heap (so that it doesnt get destroyed when out of scope) and use 
// hte object pointer


class Scene
{
    public:
        int MAX_DIR_LIGHTS;
        int MAX_POINT_LIGHTS;

        Scene()
        {

        }

        using ObjectCallback = std::function<void(glm::mat4 objectToWorld, std::unique_ptr<MaterialInstance> &materialInstance, std::shared_ptr<Mesh> mesh, unsigned int verticesCount, unsigned int indicesCount)>;
        void IterateObjects(ObjectCallback objectCallback)
        {
           
            for (auto &object : objects)
            {
                objectCallback(object->objToWorld, object->materialInstance, object->mesh, object->mesh->verticesCount, object->mesh->indicesCount);
            }
        }

        bool HasTexture(const std::string &path)
        {
            return loadedTextures.find(path) != loadedTextures.end();
        }

        Texture2D &GetTexture(const std::string &path)
        {   
            return loadedTextures.at(path);
        }

        void AddTexture(std::string path, Texture2D &&texture)
        {
            loadedTextures.emplace(path, std::move(texture));
        }

        std::shared_ptr<Object> AddObject(std::shared_ptr<Mesh> mesh, glm::mat4 objToWorld, MaterialInstance::MaterialProperties materialProperties, MaterialTemplate materialTemplate, unsigned int materialOverrideFlags)
        {
            Object* newObject = new Object();
            newObject->mesh = mesh;
            newObject->materialInstance = std::make_unique<MaterialInstance>(materialTemplate);
            newObject->materialInstance->AddFlags(materialOverrideFlags);

            //Material properties:
            newObject->materialInstance->properties = materialProperties;
            
            
            newObject->objToWorld = objToWorld;
            objects.emplace_back(newObject);
            return objects[objects.size()-1];
        }

        //Adds an ambient light to the scene
        void AddLight(glm::vec4 color)
        {
            ambientLight = color;
        }
        //Adds a directional light to the scene
        void AddLight(glm::vec3 direction, glm::vec3 color, std::shared_ptr<Object> boundObject)
        {
            directionalLights.emplace_back(direction,color,boundObject);
        }
        //Adds a point light to the scene
        void AddLight(glm::vec3 color, float constant,float linear, float quadratic, std::shared_ptr<Object> boundObject)
        {
            pointLights.emplace_back(color,constant,linear,quadratic,boundObject);
        }
        
        //Returns the light data in view space
        GlobalLightData GetLightData(glm::mat4 &viewMatrix)
        {
            auto gLightData = GlobalLightData();
            gLightData.ambientLight = ambientLight;

            // DirectionalLights:       
            for (std::size_t i = 0; i < MAX_DIR_LIGHTS; i++)
            {
                if(i >= directionalLights.size())
                {
                    break;
                }
                auto lightData = directionalLights[i].GetLightData();
                lightData.lightDirection = viewMatrix * lightData.lightDirection;
                gLightData.directionalLights.push_back(lightData);
            }

            // PointLights:             
            for (std::size_t i = 0; i < MAX_POINT_LIGHTS; i++)
            {
                if(i >= pointLights.size())
                {
                    break;
                }
                auto lightData = pointLights[i].GetLightData();
                lightData.position = viewMatrix * lightData.position;
                gLightData.pointLights.push_back(lightData);
            }
            


            gLightData.numDirLights = gLightData.directionalLights.size();
            gLightData.numPointLights = gLightData.pointLights.size();

            return gLightData;

        }

        int GetDirLightCount()
        {
            return directionalLights.size();
        }

     
    private:
        //maps texture (file) paths to each texture object
        //std::unordered_map<std::string, Texture> loadedTextures;

        //Currently this would not support dynamic scene loading and unloading as the texture IDs would still exist inside the MaterialInstance after deletion
        //Instead, PASS TEXTURE HANDLES ARROUND
        //REPLACE WITH  Pool<Texture>
        std::unordered_map<std::string, Texture2D> loadedTextures;


        std::vector<std::shared_ptr<Object>> objects;


        glm::vec4 ambientLight = glm::vec4(0);
        std::vector<DirectionalLight> directionalLights;
        std::vector<PointLight> pointLights;

};







#endif