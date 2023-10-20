#ifndef BASE_LIGHT_H
#define BASE_LIGHT_H

#include "Object.h"
#include <glm/glm.hpp>

class BaseLight
{
    public:

        struct LightData
        {
            glm::vec3 lightColor;
            glm::vec3 lightDirection;
            glm::mat4 lightMatrix;
        };

        bool hasObject = false;

        BaseLight(){}
        virtual ~BaseLight(){}

        virtual LightData GetLightData(){}
        virtual void SetObject(){}

    protected:
        Object *object = nullptr;
        glm::vec3 lightColor;

};

#endif