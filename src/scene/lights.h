#ifndef LIGHTS_H
#define LIGHTS_H

#include <glm/glm.hpp>

struct LightData
{
    glm::vec3 lightColor;
    glm::vec3 lightPosition;
    glm::vec3 lightDirection;
    glm::mat4 lightMatrix;
};

class Light
{
    public:
        bool hasObject = false;

        Light(){}
        virtual ~Light(){}

        virtual LightData GetLightData(){}
        virtual void SetObject(){}

    protected:
        unsigned int VAO;
        glm::vec3 lightColor;
        glm::vec3 lightPosition;

};




#endif