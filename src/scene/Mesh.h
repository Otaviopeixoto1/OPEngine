#ifndef MESH_H
#define MESH_H

#include <glad/glad.h>
#include <vector>
#include <glm/glm.hpp>
#include <memory>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "../common/AssimpHelpers.h"

#include "../common/Shader.h"


class Mesh;

enum MeshFlags
{
    OP_MESH_COORDS = 0, // 0
    OP_MESH_NORMALS = 1 << 0, // 1
    OP_MESH_TANGENTS = 1 << 1, // 2
    OP_MESH_TEXCOORDS = 1 << 2, // 4
    OP_MESH_EXTRA_ATTRIBUTE0 = 1 << 3, // 8
    OP_MESH_EXTRA_ATTRIBUTE1 = 1 << 4, // 16
    OP_MESH_EXTRA_ATTRIBUTE2 = 1 << 5, // 32
    OP_MESH_EXTRA_ATTRIBUTE3 = 1 << 6, // 64
    OP_MESH_EXTRA_ATTRIBUTE4 = 1 << 7  //128
};

enum MainMeshAttributeBindings
{
    MESH_COORDS_ATTRIBUTE = 0,
    MESH_NORMALS_ATTRIBUTE = 1, 
    MESH_TANGENTS_ATTRIBUTE = 2,
    MESH_TEXCOORDS_ATTRIBUTE = 3,
    MESH_EXTRA_ATTRIBUTE0 = 4,
    MESH_EXTRA_ATTRIBUTE1 = 5, 
    MESH_EXTRA_ATTRIBUTE2 = 6, 
    MESH_EXTRA_ATTRIBUTE3 = 7, 
    MESH_EXTRA_ATTRIBUTE4 = 8  
};

// Container for mesh data as well as methods to generate useful meshes
class MeshData
{
    public:
        struct Vertex 
        {
            glm::vec3 Position;
            glm::vec3 Normal;
            glm::vec3 Tangent;
            glm::vec2 TexCoords;
        };
        unsigned int flags = OP_MESH_COORDS;

        std::vector<Vertex> vertices;
        std::vector<unsigned int> indices;

        //give a different type of input and build the vertices from it
        MeshData(std::vector<Vertex> &vertices, std::vector<unsigned int> &indices)
        {
            this->vertices = vertices;
            this->indices = indices;
        }

        void AddFlags(unsigned int flag)
        {
            flags = flags | flag;
        }

        bool HasFlags(unsigned int flag)
        {
            return (flags & (int)flag) == (int)flag;
        }

        //static functions for generating meshes based on an input mesh:
        
        static MeshData LoadMeshDataFromFile(const std::string& filePath)
        {
            Assimp::Importer import;
            const aiScene *scene = import.ReadFile(filePath, aiProcess_Triangulate | aiProcess_FlipUVs);	
                
            if(!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) 
            {
                std::cout << "ERROR::ASSIMP::" << import.GetErrorString() << std::endl;
            }

            aiMesh *mMesh = scene->mMeshes[0];
            
            std::vector<MeshData::Vertex> mVertices;
            std::vector<unsigned int> mIndices;

            // process vertex positions, normals and texture coordinates for each vertex in the mesh
            for(unsigned int i = 0; i < mMesh->mNumVertices; i++)
            {
                MeshData::Vertex vertex;
                vertex.Position = AssimpHelpers::GetGLMVec3(mMesh->mVertices[i]);

                if (mMesh->HasNormals())
                {
                    vertex.Normal = AssimpHelpers::GetGLMVec3(mMesh->mNormals[i]);
                }

                //there can be up to 8 texcoords
                if(mMesh->mTextureCoords[0]) // check if the mesh contain texture coordinates
                {
                    glm::vec2 vec;
                    vec.x = mMesh->mTextureCoords[0][i].x; 
                    vec.y = mMesh->mTextureCoords[0][i].y;
                    vertex.TexCoords = vec;
                }
                else
                    vertex.TexCoords = glm::vec2(0.0f, 0.0f); 

                mVertices.push_back(vertex);
            }
            // process indices
            for(unsigned int i = 0; i < mMesh->mNumFaces; i++)
            {
                aiFace face = mMesh->mFaces[i];
                for(unsigned int j = 0; j < face.mNumIndices; j++)
                    mIndices.push_back(face.mIndices[j]);
            }  
            
            return MeshData(mVertices, mIndices);
        }
};



// Mesh could just be a struct without any function deffinitions
class Mesh
{
    public:
        unsigned int verticesCount;
        unsigned int indicesCount;

        Mesh(){}

        Mesh(MeshData &meshData)
        {
            this->flags = meshData.flags;
            InitBuffers(meshData.vertices, meshData.indices);
        }

        // Generates a default mesh. Expects normals and texcoords to be filled in vertex data
        Mesh(std::vector<MeshData::Vertex> &mVertices, std::vector<unsigned int> &mIndices)
        {
            this->flags = OP_MESH_COORDS | OP_MESH_NORMALS | OP_MESH_TANGENTS | OP_MESH_TEXCOORDS ;
            InitBuffers(mVertices, mIndices);
        }

        ~Mesh()
        {   
            glDeleteVertexArrays(1, &VAO);
            glDeleteBuffers(1, &VBO);      
            glDeleteBuffers(1, &EBO);
        }
        
        static std::unique_ptr<Mesh> QuadMesh()
        {
            Mesh* quad = new Mesh();
            float quadVertices[24] = 
            {   // vertex attributes for a quad that fills the entire screen 
                // positions   // texCoords
                -1.0f,  1.0f,  0.0f, 1.0f,
                -1.0f, -1.0f,  0.0f, 0.0f,
                1.0f, -1.0f,  1.0f, 0.0f,

                -1.0f,  1.0f,  0.0f, 1.0f,
                1.0f, -1.0f,  1.0f, 0.0f,
                1.0f,  1.0f,  1.0f, 1.0f
            };

            glGenVertexArrays(1, &quad->VAO);
            glGenBuffers(1, &quad->VBO);
            glGenBuffers(1, &quad->EBO);

            glBindVertexArray(quad->VAO);
            glBindBuffer(GL_ARRAY_BUFFER, quad->VBO);
            glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

            return std::unique_ptr<Mesh>(quad);
        }

        bool HasFlags(unsigned int flag)
        {
            return (flags & (int)flag) == (int)flag;
        }

        // Adds data from a given buffer into the vertex attributes 
        unsigned int AddVertexAttribute(GLuint attBuffer)
        {
            for (unsigned int i = 0; i < 5; i++)
            {
                if (HasFlags(OP_MESH_EXTRA_ATTRIBUTE0 << i))
                {
                    continue;
                }
                BindBuffers();
                glBindBuffer(GL_ARRAY_BUFFER, attBuffer);
                glEnableVertexAttribArray(MESH_EXTRA_ATTRIBUTE0 + i);
                glVertexAttribIPointer(MESH_EXTRA_ATTRIBUTE0 + i, 1, GL_UNSIGNED_INT, 0, 0);
                glVertexAttribDivisor(MESH_EXTRA_ATTRIBUTE0 + i, 1);
                UnbindBuffers();

                return i;

            }
            
        }

        void BindBuffers()
        {
            glBindVertexArray(VAO);
        }
        void UnbindBuffers()
        {
            glBindVertexArray(0);
        }

    private:
        //Vertex + Index buffers
        GLuint VAO, VBO, EBO;
        unsigned int flags;

        void InitBuffers(std::vector<MeshData::Vertex> &vertices, std::vector<unsigned int> &indices)
        {
            glGenVertexArrays(1, &VAO);
            glGenVertexArrays(1, &VAO);
            glGenBuffers(1, &VBO);
            glGenBuffers(1, &EBO);
            

            glBindVertexArray(VAO);
            glBindBuffer(GL_ARRAY_BUFFER, VBO);

            glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(MeshData::Vertex), 
                        &vertices[0], GL_STATIC_DRAW);  

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), 
                        &indices[0], GL_STATIC_DRAW);


            // vertex positions
            if(HasFlags(OP_MESH_COORDS))
            {
                glVertexAttribPointer(MESH_COORDS_ATTRIBUTE, 3, GL_FLOAT, GL_FALSE, sizeof(MeshData::Vertex), (void*)0);
                glEnableVertexAttribArray(MESH_COORDS_ATTRIBUTE);
            }
            

            // vertex normals
            if(HasFlags(OP_MESH_NORMALS))
            {
                glVertexAttribPointer(MESH_NORMALS_ATTRIBUTE, 3, GL_FLOAT, GL_FALSE, sizeof(MeshData::Vertex), (void*)offsetof(MeshData::Vertex, Normal));
                glEnableVertexAttribArray(MESH_NORMALS_ATTRIBUTE);
            }

            // vertex tangent
            if (HasFlags(OP_MESH_TANGENTS))
            {
                glVertexAttribPointer(MESH_TANGENTS_ATTRIBUTE, 3, GL_FLOAT, GL_FALSE, sizeof(MeshData::Vertex), (void*)offsetof(MeshData::Vertex, Tangent));
                glEnableVertexAttribArray(MESH_TANGENTS_ATTRIBUTE);	
            }

            // vertex texture coords
            if (HasFlags(OP_MESH_TEXCOORDS))
            {
                glVertexAttribPointer(MESH_TEXCOORDS_ATTRIBUTE, 2, GL_FLOAT, GL_FALSE, sizeof(MeshData::Vertex), (void*)offsetof(MeshData::Vertex, TexCoords));
                glEnableVertexAttribArray(MESH_TEXCOORDS_ATTRIBUTE);	
            }

            
            

            glBindVertexArray(0);
            verticesCount = vertices.size();
            indicesCount = indices.size();
            vertices.clear();
            indices.clear();
        }

};

#endif