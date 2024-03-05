#version 440 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 3) in vec2 aTexCoords;

out vec2 TexCoords;
out vec4 vNormal;

layout (std140) uniform GlobalMatrices
{
    mat4 projectionMatrix;
    mat4 viewMatrix;
    mat4 inverseViewMatrix;
    mat4 SceneMatrices[3];
    mat4 inverseVoxelMatrix;
};

layout (std140) uniform LocalMatrices
{
    mat4 modelMatrix;
    mat4 normalMatrix;
};

 


void main()
{
    TexCoords = aTexCoords;    
    vNormal = normalMatrix * vec4(aNormal, 0.0f);
    gl_Position = modelMatrix * vec4(aPos, 1.0f);
}