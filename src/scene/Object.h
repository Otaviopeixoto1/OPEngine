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
    OP_MATERIAL_UNLIT = 1 << 7  //128
};




struct MaterialTemplate
{
    unsigned int id;
    std::vector<std::string> texturePaths;
    unsigned int flags;

    struct MaterialBinding
    {

    };

    

    MaterialTemplate(unsigned int setFlags)
    {
        this->flags = setFlags;
    }

};

class MaterialInstance
{
    public:
        
        glm::vec3 albedoColor;
        glm::vec3 emissiveColor;
        unsigned int numTextures;
        
        MaterialInstance(MaterialTemplate &matTemp) 
        {
            this->numTextures = matTemp.texturePaths.size();
            this->texturePaths = matTemp.texturePaths;
            this->flags = matTemp.flags;
            this->templateId = matTemp.id;
        }
        bool HasFlag(MaterialFlags flag)
        {
            return (flags & (int)flag) == (int)flag;
        }
        void AddFlag(MaterialFlags flag)
        {
            flags = flags | flag;
        }
        unsigned int TemplateId()
        {
            return templateId;
        }
        std::string GetTexturePath(unsigned int i)
        {
            if (i >= numTextures)
            {
                return "";
            }

            return texturePaths[i];
        }
    private:
        unsigned int flags;
        unsigned int templateId;
        unsigned int instanceId;
        std::vector<std::string> texturePaths;
        //MaterialTemplate* baseTemplate;

};





struct Texture {
    unsigned int id;
    TextureType type;
};  

struct Object
{
    //currently each object can only have one mesh
    std::shared_ptr<Mesh> mesh;
    std::unique_ptr<MaterialInstance> materialInstance;

    glm::mat4 objToWorld;

    Object()
    {
    }
};



#endif