#ifndef LIGHTS_H
#define LIGHTS_H


#include "Object.h"
#include <glm/glm.hpp>
#include <memory>

#define MAX_DIR_LIGHTS 3



class DirectionalLight
{

    public:
        #pragma pack(push, 1)
        struct DirectionalLightData
        {
            glm::vec4 lightColor;
            glm::mat4 lightMatrix;
            glm::vec4 lightDirection;
        };
        #pragma pack(pop)

        DirectionalLight(glm::vec3 direction, glm::vec3 color, std::shared_ptr<Object> srcObject)
        {
            this->lightData = DirectionalLightData();
            this->lightData.lightColor = glm::vec4(color.x,color.y,color.z,1.0f);
            this->lightData.lightDirection = glm::vec4(direction,1.0f);
            if (srcObject != nullptr)
            {
                this->lightData.lightMatrix = srcObject->objToWorld;
                this->object = srcObject;
            }
            else
            {
                this->lightData.lightMatrix = glm::mat4(1.0);
            }

        }
        
        //every change that happens to the light will be throgh these functions
        void LookAt()
        {

        }

        void Rotate()
        {

        }
        void SetDirection(glm::vec3 direction)
        {

        }

        DirectionalLightData GetLightData()
        {
            return lightData;
        }

    private:
        DirectionalLightData lightData;
        std::shared_ptr<Object> object;
};  








#pragma pack(push, 1)
struct GlobalLightData
{
    glm::vec4 ambientLight = glm::vec4(1.0,1.0,1.0,0.3);
    int numDirLights;
    int pad1;
    int pad2;
    int pad3;
    DirectionalLight::DirectionalLightData directionalLights[MAX_DIR_LIGHTS];
};
#pragma pack(pop)



#endif