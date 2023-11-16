#ifndef LIGHTS_H
#define LIGHTS_H


#include "Object.h"
#include <glm/glm.hpp>
#include <memory>

#define MAX_DIR_LIGHTS 5
#define MAX_POINT_LIGHTS 3


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
            this->lightData.lightDirection = glm::vec4(direction,0.0f);
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




namespace LightVolumes
{
    float GetPointLightVolumeRadius(glm::vec3 color, float constant, float linear, float quadratic)
    {
        float maxC =std::max(color.x, std::max(color.y,color.z));
        return (-linear + sqrt(linear * linear -4* quadratic * (constant - 256 * maxC) ))/(2 * quadratic);
    } 
}



class PointLight
{

    public:
        #pragma pack(push, 1)
        struct PointLightData
        {
            glm::vec4 position;
            glm::vec4 lightColor;
            float constant;
            float linear;
            float quadratic;

            float radius;
        };
        #pragma pack(pop)

        PointLight(glm::vec3 color, float c, float l, float q, std::shared_ptr<Object> srcObject)
        {
            this->object = srcObject;
            this->lightData = PointLightData();
            this->lightData.lightColor = glm::vec4(color.x,color.y,color.z,1.0f);
            this->lightData.position = glm::vec4(srcObject->objToWorld[3]);
            this->lightData.position.w = 1.0;
            this->lightData.constant = c;
            this->lightData.linear = l;
            this->lightData.quadratic = q;
            this->lightData.radius = LightVolumes::GetPointLightVolumeRadius(color, c,l,q);
            //std::cout <<"lightradius:>>>>>>" << lightData.radius << "\n"; 
        }
        


        PointLightData GetLightData()
        {
            this->lightData.position = glm::vec4(object->objToWorld[3]);
            this->lightData.position.w = 1.0;
            return lightData;
        }

    private:
        PointLightData lightData;
        std::shared_ptr<Object> object;
};  







#pragma pack(push, 1)
struct GlobalLightData
{
    glm::vec4 ambientLight;
    int numDirLights;
    int numPointLights;
    int pad2;
    int pad3;
    DirectionalLight::DirectionalLightData directionalLights[MAX_DIR_LIGHTS];
    PointLight::PointLightData pointLights[MAX_POINT_LIGHTS];
};
#pragma pack(pop)



#endif