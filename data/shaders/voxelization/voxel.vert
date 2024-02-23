#version 440 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 3) in vec2 aTexCoords;

out vec2 TexCoords;

layout (std140) uniform GlobalMatrices
{
    mat4 projectionMatrix;
    mat4 viewMatrix;
    mat4 inverseViewMatrix;
    mat4 SceneMatrices[3];
};

layout (std140) uniform LocalMatrices
{
    mat4 modelMatrix;
    mat4 normalMatrix;
};

 


void main()
{
    TexCoords = aTexCoords;    
    gl_Position = modelMatrix * vec4(aPos, 1.0);
}