#ifndef DIRECTIONAL_LIGHT_H
#define DIRECTIONAL_LIGHT_H

#include "BaseLight.h"


class DirectionalLight : public BaseLight
{

    public:
        DirectionalLight(glm::vec3 direction, glm::vec3 color)
        {
            this->lightDirection = direction;
            this->lightColor = color;
        }

        LightData GetLightData()
        {
            LightData lightData = LightData();
            lightData.lightColor = lightColor;
            lightData.lightDirection = lightDirection;

            if (object != nullptr)
            {
                lightData.lightMatrix = object->objToWorld;
            }
            else
            {
                lightData.lightMatrix = glm::mat4(1.0);
            }

            return lightData;
            

        }
        void SetObject(Object &aObject)
        {
            object = &aObject;
        }
    private:
        glm::vec3 lightDirection;
        glm::vec3 lightColor;
};  

#endif