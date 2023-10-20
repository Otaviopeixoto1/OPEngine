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

enum MaterialFlags
{
    OP_MATERIAL_DEFAULT = 0, // 0
    OP_MATERIAL_TEXTURED_DIFFUSE = 1 << 0, // 1
    OP_MATERIAL_TEXTURED_SPECULAR = 1 << 1, // 2
    OP_MATERIAL_TEXTURED_NORMAL = 1 << 2, // 4
    Flag4 = 1 << 3, // 8
    Flag5 = 1 << 4, // 16
    Flag6 = 1 << 5, // 32
    Flag7 = 1 << 6, // 64
    Flag8 = 1 << 7  //128
};


struct Material
{
    // albedoColor will only be used for simple materials and simple shaders where we dont
    // sample textures
    unsigned int id;
    glm::vec3 albedoColor;
    glm::vec3 emissiveColor;
    std::vector<std::string> texturePaths;
    unsigned int flags;

    struct MaterialBinding
    {

    };
    bool HasFlag(MaterialFlags flag)
    {
        return (flags & (int)flag) == (int)flag;
    }

    Material(unsigned int setFlags)
    {
        this->flags = setFlags;
    }

};

struct Texture {
    unsigned int id;
    TextureType type;
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