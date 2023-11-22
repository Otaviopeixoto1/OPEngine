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
    OP_MESH_Flag5 = 1 << 3, // 8
    OP_MESH_Flag6 = 1 << 4, // 16
    OP_MESH_Flag7 = 1 << 5, // 32
    OP_MESH_Flag8 = 1 << 6, // 64
    OP_MESH_Flag9 = 1 << 7  //128
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

        std::vector<MeshData::Vertex> vertices;
        std::vector<unsigned int> indices;

        Mesh(MeshData &meshData)
        {
            this->vertices = meshData.vertices;
            this->indices = meshData.indices;
            this->flags = meshData.flags;

            InitBuffers();
            vertices.clear();
            indices.clear();
        }

        // Generates a default mesh. Expects normals and texcoords to be filled in vertex data
        Mesh(std::vector<MeshData::Vertex> &mVertices, std::vector<unsigned int> &mIndices)
        {
            this->flags = OP_MESH_COORDS | OP_MESH_NORMALS | OP_MESH_TANGENTS | OP_MESH_TEXCOORDS ;
            this->vertices = mVertices;
            this->indices = mIndices;

            InitBuffers();
            vertices.clear();
            indices.clear();
        }
        

        bool HasFlags(unsigned int flag)
        {
            return (flags & (int)flag) == (int)flag;
        }
        
        void BindBuffers()
        {
            glBindVertexArray(VAO);
        }
        void UnbindBuffers()
        {
            glBindVertexArray(0);
        }

        void Draw(Shader &shader)
        {

        }

    private:
        //Vertex + Index buffers
        unsigned int VAO, VBO, EBO;
        unsigned int flags;

        void InitBuffers()
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
                glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(MeshData::Vertex), (void*)0);
                glEnableVertexAttribArray(0);
            }
            

            // vertex normals
            if(HasFlags(OP_MESH_NORMALS))
            {
                glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(MeshData::Vertex), (void*)offsetof(MeshData::Vertex, Normal));
                glEnableVertexAttribArray(1);
            }

            // vertex tangent
            if (HasFlags(OP_MESH_TANGENTS))
            {
                glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(MeshData::Vertex), (void*)offsetof(MeshData::Vertex, Tangent));
                glEnableVertexAttribArray(2);	
            }

            // vertex texture coords
            if (HasFlags(OP_MESH_TEXCOORDS))
            {
                glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(MeshData::Vertex), (void*)offsetof(MeshData::Vertex, TexCoords));
                glEnableVertexAttribArray(3);	
            }

            
            

            glBindVertexArray(0);
            verticesCount = vertices.size();
            indicesCount = indices.size();
        }

};

#endif