#ifndef MESH_H
#define MESH_H

#include <glad/glad.h>
#include <vector>
#include <glm/glm.hpp>


#include "../common/Shader.h"




// Container for mesh data as well as methods to generate useful meshes
class MeshData
{
    public:

        
        struct Vertex 
        {
            glm::vec3 Position;
            glm::vec3 Normal;
            glm::vec2 TexCoords;
        };
        

        std::vector<Vertex> vertices;
        std::vector<unsigned int> indices;

        //give a different type of input and build the vertices from it
        MeshData(std::vector<Vertex> vertices, std::vector<unsigned int> indices)
        {
            this->vertices = vertices;
            this->indices = indices;
        }

        //static functions for generating meshes based on an input mesh:







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
            vertices = meshData.vertices;
            indices = meshData.indices;

            InitBuffers();
            
        }

        Mesh(std::vector<MeshData::Vertex> mVertices, std::vector<unsigned int> mIndices)
        {
            this->vertices = mVertices;
            this->indices = mIndices;

            InitBuffers();
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

        // render data
        unsigned int VAO, VBO, EBO;

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
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(MeshData::Vertex), (void*)0);
            glEnableVertexAttribArray(0);

            // vertex normals
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(MeshData::Vertex), (void*)offsetof(MeshData::Vertex, Normal));
            glEnableVertexAttribArray(1);

            // vertex texture coords
            glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(MeshData::Vertex), (void*)offsetof(MeshData::Vertex, TexCoords));
            glEnableVertexAttribArray(2);	

            // unbind all buffers after configuration
            //glBindVertexArray(0);
            //glBindBuffer(GL_ARRAY_BUFFER, 0); 
            //glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0); 
            glBindVertexArray(0);
            verticesCount = vertices.size();
            indicesCount = indices.size();
        }

};

#endif