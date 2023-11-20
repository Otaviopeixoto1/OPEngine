#ifndef LIGHTS_H
#define LIGHTS_H


#include "Object.h"
#include <glm/glm.hpp>
#include <memory>


class DirectionalLight
{

    public:
        #pragma pack(push, 1)
        struct DirectionalLightData
        {
            glm::vec4 lightColor;
            glm::mat4 lightViewMatrix;
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
                float near_plane = -20.0f, far_plane = 10.0f;
                glm::mat4 lightProjection = glm::ortho(-10.0f, 10.0f, -10.0f, 10.0f, near_plane, far_plane); 

                glm::mat4 lightView = glm::lookAt
                (
                    glm::vec3(srcObject->objToWorld[3]), 
                    -direction + glm::vec3(srcObject->objToWorld[3]), 
                    glm::vec3( 0.0f, 1.0f,  0.0f)
                );  

                this->lightData.lightViewMatrix =  lightView;
                this->object = srcObject;
            }
            else
            {
                this->lightData.lightViewMatrix = glm::mat4(1.0);
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
    static inline float GetPointLightVolumeRadius(glm::vec3 color, float constant, float linear, float quadratic)
    {
        float maxC = std::max(color.x, std::max(color.y,color.z));
        return (-linear + sqrt(linear * linear -4* quadratic * (constant - 1024 * maxC) ))/(2 * quadratic);
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
    std::vector<DirectionalLight::DirectionalLightData> directionalLights;
    std::vector<PointLight::PointLightData> pointLights;
};
#pragma pack(pop)



#endif