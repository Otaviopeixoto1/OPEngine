#ifndef SCENEOBJECT_H
#define SCENEOBJECT_H

#include <string>
#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include "Mesh.h"

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
    unsigned int id;
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

    Object()
    {
    }
};



#endif